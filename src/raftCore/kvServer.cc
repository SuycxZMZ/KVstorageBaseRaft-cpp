#include "kvServer.h"
#include "rpc/KVrpcprovider.h"
#include "sylar/rpc/rpcconfig.h"
#include "common/config.h"

void KvServer::DprintfKVDB() {
    if (!Debug) {
        return;
    }
    std::lock_guard<std::mutex> lg(m_mtx);
    DEFER {
        // for (const auto &item: m_kvDB) {
        //     DPrintf("[DBInfo ----]Key : %s, Value : %s", &item.first, &item.second);
        // }
        m_skipList.display_list();
    };
}

void KvServer::ExecuteAppendOpOnKVDB(Op op) {
    // if op.IfDuplicate {   //get请求是可重复执行的，因此可以不用判复
    //	return
    // }
    m_mtx.lock();

    m_skipList.insert_set_element(op.Key, op.Value);

    // if (m_kvDB.find(op.Key) != m_kvDB.end()) {
    //     m_kvDB[op.Key] = m_kvDB[op.Key] + op.Value;
    // } else {
    //     m_kvDB.insert(std::make_pair(op.Key, op.Value));
    // }
    m_lastRequestId[op.ClientId] = op.RequestId;
    m_mtx.unlock();

    //    DPrintf("[KVServerExeAPPEND-----]ClientId :%d ,RequestID :%d ,Key : %v, value : %v", op.ClientId,
    //    op.RequestId, op.Key, op.Value)
    DprintfKVDB();
}

void KvServer::ExecuteGetOpOnKVDB(Op op, std::string *value, bool *exist) {
    m_mtx.lock();
    *value = "";
    *exist = false;
    if (m_skipList.search_element(op.Key, *value)) {
        *exist = true;
        // *value = m_skipList.se //value已经完成赋值了
    }
    // if (m_kvDB.find(op.Key) != m_kvDB.end()) {
    //     *exist = true;
    //     *value = m_kvDB[op.Key];
    // }
    m_lastRequestId[op.ClientId] = op.RequestId;
    m_mtx.unlock();

    if (*exist) {
        //                DPrintf("[KVServerExeGET----]ClientId :%d ,RequestID :%d ,Key : %v, value :%v", op.ClientId,
        //                op.RequestId, op.Key, value)
    } else {
        //        DPrintf("[KVServerExeGET----]ClientId :%d ,RequestID :%d ,Key : %v, But No KEY!!!!", op.ClientId,
        //        op.RequestId, op.Key)
    }
    DprintfKVDB();
}

void KvServer::ExecutePutOpOnKVDB(Op op) {
    m_mtx.lock();
    m_skipList.insert_set_element(op.Key, op.Value);
    // m_kvDB[op.Key] = op.Value;
    m_lastRequestId[op.ClientId] = op.RequestId;
    m_mtx.unlock();

    //    DPrintf("[KVServerExePUT----]ClientId :%d ,RequestID :%d ,Key : %v, value : %v", op.ClientId, op.RequestId,
    //    op.Key, op.Value)
    DprintfKVDB();
}

// 处理来自clerk的Get RPC
void KvServer::Get(const raftKVRpcProctoc::GetArgs *args, raftKVRpcProctoc::GetReply *reply) {
    // 根据请求参数生成Op，生成Op是因为raft和raftServer沟通用的是类似于go中的channel的机制
    Op op;
    op.Operation = "Get";
    op.Key = args->key();
    op.Value = "";
    op.ClientId = args->clientid();
    op.RequestId = args->requestid();

    int raftIndex = -1;
    int _ = -1;
    int isLeader = m_raftNode->Start(op, &raftIndex, &_);  // raftIndex：raft预计的logIndex
                                   // ，虽然是预计，但是正确情况下是准确的，op的具体内容对raft来说 是隔离的
    if (!isLeader) {
        reply->set_err(ErrWrongLeader);
        return;
    }

    m_mtx.lock();
    if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
        waitApplyCh.insert(std::make_pair(raftIndex, new LockQueue<Op>()));
    }
    // 拿命令
    auto chForRaftIndex = waitApplyCh[raftIndex];
    m_mtx.unlock();  // 直接解锁，等待任务执行完成，不能一直拿锁等待

    Op raftCommitOp; // timeout
    if (!chForRaftIndex->timeOutPop(CONSENSUS_TIMEOUT, &raftCommitOp)) { // 待执行命令 为空
        int _ = -1;
        int isLeader = m_raftNode->GetState(&_);
        if (ifRequestDuplicate(op.ClientId, op.RequestId) && isLeader) {
            // 待执行命令为空，代表raft集群不保证已经commitIndex该日志。
            // 但是如果是已经提交过的get请求，是可以再执行的,不会违反线性一致性
            std::string value;
            bool exist = false;
            // 操作 kvDB
            ExecuteGetOpOnKVDB(op, &value, &exist); 
            if (exist) {
                reply->set_err(OK);
                reply->set_value(value);
            } else {
                reply->set_err(ErrNoKey);
                reply->set_value("");
            } 
        } else {
            reply->set_err(ErrWrongLeader);  // 返回这个，其实就是让clerk换一个节点重试
        }
    } else { // 待执行命令 非空
        // raft已经提交了该command（op），可以正式开始执行了
        // 再次检验
        if (raftCommitOp.ClientId == op.ClientId && raftCommitOp.RequestId == op.RequestId) {
            std::string value;
            bool exist = false;
            ExecuteGetOpOnKVDB(op, &value, &exist);
            if (exist) {
                reply->set_err(OK);
                reply->set_value(value);
            } else {
                reply->set_err(ErrNoKey);
                reply->set_value("");
            }
        } else {
            reply->set_err(ErrWrongLeader);
        }
    }
    m_mtx.lock();  // todo 這個可以先弄一個defer，因爲刪除優先級並不高，先把rpc發回去更加重要
    auto tmp = waitApplyCh[raftIndex];
    waitApplyCh.erase(raftIndex);
    delete tmp;
    m_mtx.unlock();
}

/**
 * @brief 从raft节点获取命令，操作kvDB
 * @param message
 */
void KvServer::GetCommandFromRaft(ApplyMsg message) {
    Op op;
    op.parseFromString(message.Command);

    DPrintf(
        "[KvServer::GetCommandFromRaft-kvserver{%d}] , Got Command --> Index:{%d} , ClientId {%s}, RequestId {%d}, "
        "Opreation {%s}, Key :{%s}, Value :{%s}",
        m_me, message.CommandIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);
    if (message.CommandIndex <= m_lastSnapShotRaftLogIndex) {
        return;
    }

    // State Machine (KVServer solute the duplicate problem)
    // duplicate command will not be exed
    if (!ifRequestDuplicate(op.ClientId, op.RequestId)) {
        // execute command
        if (op.Operation == "Put") {
            ExecutePutOpOnKVDB(op);
        }
        if (op.Operation == "Append") {
            ExecuteAppendOpOnKVDB(op);
        }
        //  kv.lastRequestId[op.ClientId] = op.RequestId  在Executexxx函数里面更新的
    }
    // 到这里kvDB已经制作了快照
    if (m_maxRaftState != -1) {
        IfNeedToSendSnapShotCommand(message.CommandIndex, 9);
        // 如果raft的log太大（大于指定的比例）就把制作快照
    }

    // Send message to the chan of op.ClientId
    SendMessageToWaitChan(op, message.CommandIndex);
}

bool KvServer::ifRequestDuplicate(std::string ClientId, int RequestId) {
    std::lock_guard<std::mutex> lg(m_mtx);
    if (m_lastRequestId.find(ClientId) == m_lastRequestId.end()) {
        return false;
        // todo :不存在这个client就创建
    }
    return RequestId <= m_lastRequestId[ClientId];
}

// get和put//append執行的具體細節是不一樣的
// PutAppend在收到raft消息之後執行，具體函數裏面只判斷冪等性（是否重複）
// get函數收到raft消息之後在，因爲get無論是否重複都可以再執行
void KvServer::PutAppend(const raftKVRpcProctoc::PutAppendArgs *args, raftKVRpcProctoc::PutAppendReply *reply) {
    Op op;
    op.Operation = args->op();
    op.Key = args->key();
    op.Value = args->value();
    op.ClientId = args->clientid();
    op.RequestId = args->requestid();
    int raftIndex = -1;
    int _ = -1;
    int isleader = m_raftNode->Start(op, &raftIndex, &_);

    if (0 == isleader) {
        DPrintf(
            "[func -KvServer::PutAppend -kvserver{%d}]From Client %s (Request %d) To Server %d, key %s, raftIndex %d , "
            "but "
            "not leader",
            m_me, &args->clientid(), args->requestid(), m_me, &op.Key, raftIndex);

        reply->set_err(ErrWrongLeader);
        return;
    }
    DPrintf(
        "[func -KvServer::PutAppend -kvserver{%d}]From Client %s (Request %d) To Server %d, key %s, raftIndex %d , is "
        "leader ",
        m_me, &args->clientid(), args->requestid(), m_me, &op.Key, raftIndex);
    m_mtx.lock();
    if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
        waitApplyCh.insert(std::make_pair(raftIndex, new LockQueue<Op>()));
    }
    auto chForRaftIndex = waitApplyCh[raftIndex];

    m_mtx.unlock();  // 直接解锁，等待任务执行完成，不能一直拿锁等待

    // timeout
    Op raftCommitOp;

    if (!chForRaftIndex->timeOutPop(CONSENSUS_TIMEOUT, &raftCommitOp)) {
        DPrintf(
            "[func -KvServer::PutAppend -kvserver{%d}]TIMEOUT PUTAPPEND !!!! Server %d , get Command <-- Index:%d , "
            "ClientId %s, RequestId %s, Opreation %s Key :%s, Value :%s",
            m_me, m_me, raftIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);

        if (ifRequestDuplicate(op.ClientId, op.RequestId)) {
            reply->set_err(OK);  // 超时了,但因为是重复的请求，返回ok，实际上就算没有超时，在真正执行的时候也要判断是否重复
        } else {
            reply->set_err(ErrWrongLeader);  /// 这里返回这个的目的让clerk重新尝试
        }
    } else {
        DPrintf(
            "[func -KvServer::PutAppend -kvserver{%d}]WaitChanGetRaftApplyMessage<--Server %d , get Command <-- "
            "Index:%d , "
            "ClientId %s, RequestId %d, Opreation %s, Key :%s, Value :%s",
            m_me, m_me, raftIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);
        if (raftCommitOp.ClientId == op.ClientId && raftCommitOp.RequestId == op.RequestId) {
            // 可能发生leader的变更导致日志被覆盖，因此必须检查
            reply->set_err(OK);
        } else {
            reply->set_err(ErrWrongLeader);
        }
    }

    m_mtx.lock();

    auto tmp = waitApplyCh[raftIndex];
    waitApplyCh.erase(raftIndex);
    delete tmp;
    m_mtx.unlock();
}

/**
 * @brief 一直等待raft传来的applyCh
*/
void KvServer::ReadRaftApplyCommandLoop() {
    while (true) {
        // 如果只操作applyChan不用拿锁，因为applyChan自己带锁
        auto message = applyChan->Pop();  // 阻塞弹出
        DPrintf(
            "---------------tmp-------------[func-KvServer::ReadRaftApplyCommandLoop()-kvserver{%d}] "
            "收到了下raft的消息",
            m_me);
        // listen to every command applied by its raft ,delivery to relative RPC Handler

        if (message.CommandValid) {
            GetCommandFromRaft(message);
        }
        if (message.SnapshotValid) {
            GetSnapShotFromRaft(message);
        }
    }
    std::cout << "Error !!! ReadRaftApplyCommandLoop() end" << std::endl;
}

// raft会与persist层交互，kvserver层也会，因为kvserver层开始的时候需要恢复kvdb的状态
//  关于快照raft层与persist的交互：保存kvserver传来的snapshot；生成leaderInstallSnapshot RPC的时候也需要读取snapshot；
//  因此snapshot的具体格式是由kvserver层来定的，raft只负责传递这个东西
//  snapShot里面包含kvserver需要维护的persist_lastRequestId 以及kvDB真正保存的数据persist_kvdb
void KvServer::ReadSnapShotToInstall(std::string snapshot) {
    if (snapshot.empty()) {
        // bootstrap without any state?
        return;
    }
    parseFromString(snapshot);

    //    r := bytes.NewBuffer(snapshot)
    //    d := labgob.NewDecoder(r)
    //
    //    var persist_kvdb map[string]string  //理应快照
    //    var persist_lastRequestId map[int64]int //快照这个为了维护线性一致性
    //
    //    if d.Decode(&persist_kvdb) != nil || d.Decode(&persist_lastRequestId) != nil {
    //                DPrintf("KVSERVER %d read persister got a problem!!!!!!!!!!",kv.me)
    //        } else {
    //        kv.kvDB = persist_kvdb
    //        kv.lastRequestId = persist_lastRequestId
    //    }
}

bool KvServer::SendMessageToWaitChan(const Op &op, int raftIndex) {
    std::lock_guard<std::mutex> lg(m_mtx);
    DPrintf(
        "[RaftApplyMessageSendToWaitChan--> raftserver{%d}] , Send Command --> Index:{%d} , ClientId {%d}, RequestId "
        "{%d}, Opreation {%v}, Key :{%v}, Value :{%v}",
        m_me, raftIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);

    if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
        return false;
    }
    waitApplyCh[raftIndex]->Push(op);
    DPrintf(
        "[RaftApplyMessageSendToWaitChan--> raftserver{%d}] , Send Command --> Index:{%d} , ClientId {%d}, RequestId "
        "{%d}, Opreation {%v}, Key :{%v}, Value :{%v}",
        m_me, raftIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);
    return true;
}

void KvServer::IfNeedToSendSnapShotCommand(int raftIndex, int proportion) {
    if (m_raftNode->GetRaftStateSize() > m_maxRaftState / 10.0) {
        // Send SnapShot Command
        auto snapshot = MakeSnapShot();
        m_raftNode->Snapshot(raftIndex, snapshot);
    }
}

void KvServer::GetSnapShotFromRaft(ApplyMsg message) {
    std::lock_guard<std::mutex> lg(m_mtx);

    if (m_raftNode->CondInstallSnapshot(message.SnapshotTerm, message.SnapshotIndex, message.Snapshot)) {
        ReadSnapShotToInstall(message.Snapshot);
        m_lastSnapShotRaftLogIndex = message.SnapshotIndex;
    }
}

std::string KvServer::MakeSnapShot() {
    std::lock_guard<std::mutex> lg(m_mtx);
    std::string snapshotData = getSnapshotData();
    return snapshotData;
}

void KvServer::PutAppend(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::PutAppendArgs *request,
                         ::raftKVRpcProctoc::PutAppendReply *response, ::google::protobuf::Closure *done) {
    KvServer::PutAppend(request, response);
    done->Run();
}

void KvServer::Get(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::GetArgs *request,
                   ::raftKVRpcProctoc::GetReply *response, ::google::protobuf::Closure *done) {
    KvServer::Get(request, response);
    done->Run();
}


void KvServer::InitRpcAndRun(short port) {
    m_KvRpcProvider->NotifyService(this);
    m_KvRpcProvider->NotifyService(
        this->m_raftNode.get());  // todo：这里获取了原始指针，后面检查一下有没有泄露的问题 或者 shareptr释放的问题
    // 启动一个rpc服务发布节点   Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    m_KvRpcProvider->KVRpcProviderRunInit(m_me, port);
    m_KvRpcProvider->Run(); 
}

/**
 * @brief 构造函数
 * @param me 节点编号
 * @param maxraftstate 快照阈值，raft日志超过这个值时，会触发快照
 * @param nodeInforFileName 节点信息文件名
 * @param port 监听端口
*/
KvServer::KvServer(int me, int maxraftstate, std::string nodeInforFileName, short port) : 
        m_me(me),
        applyChan(std::make_shared<LockQueue<ApplyMsg> >()),
        m_maxRaftState(maxraftstate),
        m_skipList(6),  
        m_iom(std::make_shared<sylar::IOManager>(FIBER_THREAD_NUM, false)),
        m_raftNode(std::make_shared<Raft>(m_iom)),
        m_KvRpcProvider(std::make_shared<KVRpcProvider>(m_iom))
{
    sleep(6);
    // std::thread t(std::bind(&KvServer::InitRpcAndRun, this, port));
    // t.detach();
    m_iom->schedule([this, port]() -> void {
        this->m_KvRpcProvider->NotifyService(this);
        this->m_KvRpcProvider->NotifyService(
            this->m_raftNode.get()); 
        // 启动一个rpc服务发布节点
        this->m_KvRpcProvider->KVRpcProviderRunInit(m_me, port);
        this->m_KvRpcProvider->InnerStart();
    });

    // m_KvServerIoManager->schedule(std::bind(&KvServer::InitRpcAndRun, this, port));

    //开启rpc远程调用能力，需要注意必须要保证所有节点都开启rpc接受功能之后才能开启rpc远程调用能力
    //这里使用睡眠来保证
    std::cout << "raftServer node:" << m_me << " start to sleep to wait all ohter raftnode start!!!!" << std::endl;
    sleep(6);
    std::cout << "raftServer node:" << m_me << " wake up!!!! start to connect other raftnode" << std::endl;
    // 获取所有raft节点ip、port,并进行连接,要排除自己
    sylar::rpc::MprpcConfig config;
    config.LoadConfigFile(nodeInforFileName.c_str());
    std::vector<std::pair<std::string, short> > ipPortVt;
    for (int i = 0; i < INT_MAX - 1; ++i) {
        std::string node = "node" + std::to_string(i);

        std::string nodeIp = config.Load(node + "ip");
        std::string nodePortStr = config.Load(node + "port");
        if (nodeIp.empty()) {
            break;
        }
        ipPortVt.emplace_back(nodeIp, atoi(nodePortStr.c_str()));  // 沒有atos方法，可以考慮自己实现
    }
    std::vector<std::shared_ptr<RaftRpcUtil> > servers;
    // 进行连接
    for (int i = 0; i < ipPortVt.size(); ++i) {
        if (i == m_me) {
            servers.push_back(nullptr);
            continue;
        }
        std::string otherNodeIp = ipPortVt[i].first;
        short otherNodePort = ipPortVt[i].second;
        // RaftRpcUtil *rpc = new RaftRpcUtil(otherNodeIp, otherNodePort);
        servers.emplace_back(std::make_shared<RaftRpcUtil>(otherNodeIp, otherNodePort)); // 与其他节点通信的rpc通道
        std::cout << "node" << m_me << " 连接node" << i << " success!" << std::endl;
    }
    sleep(ipPortVt.size() - me);  // 等待所有节点相互连接成功，再启动raft
    std::shared_ptr<Persister> persister(new Persister(me));

    sleep(30);

    m_raftNode->init(servers, m_me, persister, applyChan);
    // while (true) {
    //     std::cout << "------------------ test -----------------" << std::endl;
    //     sleep(2);
    // }

    // You may need initialization code here.
    // m_kvDB; //kvdb初始化
    m_skipList;
    waitApplyCh;
    m_lastRequestId;
    m_lastSnapShotRaftLogIndex = 0;  // todo:感覺這個函數沒什麼用，不如直接調用raft節點中的snapshot值？？？
    auto snapshot = persister->ReadSnapshot();
    if (!snapshot.empty()) {
        ReadSnapShotToInstall(snapshot);
    }

    std::thread t2(&KvServer::ReadRaftApplyCommandLoop, this);  // 马上向其他节点宣告自己就是leader
    t2.join();  // 由於ReadRaftApplyCommandLoop一直不會結束，达到一直卡在这的目的
}
