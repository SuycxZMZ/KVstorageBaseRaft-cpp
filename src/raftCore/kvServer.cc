#include "kvServer.h"
#include "rpc/KVrpcprovider.h"
#include "sylar/rpc/rpcconfig.h"
#include "common/config.h"

void KvServer::DprintfKVDB() {
    if (!Debug) {
        return;
    }
    std::lock_guard<std::mutex> lg(m_mtx);
    m_skipList.display_list();
}

void KvServer::ExecuteAppendOpOnKVDB(Op op) {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_skipList.insert_set_element(op.Key, op.Value);
        m_lastRequestClientAndId[op.ClientId] = op.RequestId;
    }
    //    DPrintf("[KVServerExeAPPEND-----]ClientId :%d ,RequestID :%d ,Key : %v, value : %v", op.ClientId,
    //    op.RequestId, op.Key, op.Value)
    DprintfKVDB();
}

void KvServer::ExecuteGetOpOnKVDB(Op op, std::string *value, bool *exist) {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        *value = "";
        *exist = false;
        if (m_skipList.search_element(op.Key, *value)) {
            *exist = true;
            // *value = m_skipList.se //value已经完成赋值了
        }
        m_lastRequestClientAndId[op.ClientId] = op.RequestId;
    }

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
    m_lastRequestClientAndId[op.ClientId] = op.RequestId;
    m_mtx.unlock();

    //    DPrintf("[KVServerExePUT----]ClientId :%d ,RequestID :%d ,Key : %v, value : %v", op.ClientId, op.RequestId,
    //    op.Key, op.Value)
    DprintfKVDB();
}

// 处理来自clerk的Get RPC
void KvServer::Get(const raftKVRpcProctoc::GetArgs *args, raftKVRpcProctoc::GetReply *reply) {
    // 根据请求参数生成Op，生成Op是因为raft和raftServer沟通用的是阻塞队列
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

    LockQueue<Op>* chForRaftIndex = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (m_waitApplyCh.find(raftIndex) == m_waitApplyCh.end()) {
            m_waitApplyCh.insert(std::make_pair(raftIndex, new LockQueue<Op>()));
        }
        // 拿命令
        chForRaftIndex = m_waitApplyCh[raftIndex];
    }

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

    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto tmp = m_waitApplyCh[raftIndex];
        m_waitApplyCh.erase(raftIndex);
        delete tmp;
    }
}

/**
 * @brief 从raft节点获取命令并解析，操作kvDB
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
    }

    // Send message to the chan of op.ClientId
    SendMessageToWaitChan(op, message.CommandIndex);
}

bool KvServer::ifRequestDuplicate(std::string ClientId, int RequestId) {
    std::lock_guard<std::mutex> lg(m_mtx);
    if (m_lastRequestClientAndId.find(ClientId) == m_lastRequestClientAndId.end()) {
        return false;
        // todo :不存在这个client就创建
    }
    return RequestId <= m_lastRequestClientAndId[ClientId];
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
    LockQueue<Op>* chForRaftIndex = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (m_waitApplyCh.find(raftIndex) == m_waitApplyCh.end()) {
            m_waitApplyCh.insert(std::make_pair(raftIndex, new LockQueue<Op>()));
        }
        chForRaftIndex = m_waitApplyCh[raftIndex];
    }

    // timeout
    Op raftCommitOp;

    if (!chForRaftIndex->timeOutPop(CONSENSUS_TIMEOUT, &raftCommitOp)) {
        DPrintf(
            "[func -KvServer::PutAppend -kvserver{%d}]TIMEOUT PUTAPPEND !!!! Server %d , get Command <-- Index:%d , "
            "ClientId %s, RequestId %d, Opreation %s Key :%s, Value :%s",
            m_me, m_me, raftIndex, 
            op.ClientId.c_str(), op.RequestId, op.Operation.c_str(), op.Key.c_str(), op.Value.c_str());

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

    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto tmp = m_waitApplyCh[raftIndex];
        m_waitApplyCh.erase(raftIndex);
        delete tmp;
    }
}

/**
 * @brief 一直等待 raft 传来的ApplyMsg
*/
void KvServer::ReadRaftApplyCommandLoop() {
    while (true) {
        auto message = m_applyChan->Pop();  // 阻塞弹出
        // listen to every command applied by its raft ,delivery to relative RPC Handler

        // 应用到 KVDB
        if (message.CommandValid) {
            GetCommandFromRaft(message);
        }
        if (message.SnapshotValid) {
            GetSnapShotFromRaft(message);
        }
    }
    std::cout << "Error !!! ReadRaftApplyCommandLoop() end" << std::endl;
}

// raft会与persister交互，kvserver层也会，因为kvserver层开始的时候需要恢复kvdb的状态
//  关于快照 raft层与persist的交互：保存kvserver传来的snapshot；生成leaderInstallSnapshot RPC的时候也需要读取snapshot；
//  因此snapshot的具体格式是由kvserver层来定的，raft只负责传递这个东西
//  snapShot里面包含kvserver需要维护的persist_lastRequestId 以及kvDB真正保存的数据persist_kvdb
void KvServer::ReadSnapShotToInstall(std::string snapshot) {
    if (snapshot.empty()) {
        return;
    }
    parseFromString(snapshot);
}

bool KvServer::SendMessageToWaitChan(const Op &op, int raftIndex) {
    std::lock_guard<std::mutex> lg(m_mtx);
    DPrintf(
        "[RaftApplyMessageSendToWaitChan--> raftserver{%d}] , Send Command --> Index:{%d} , ClientId {%d}, RequestId "
        "{%d}, Opreation {%v}, Key :{%v}, Value :{%v}",
        m_me, raftIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);

    if (m_waitApplyCh.find(raftIndex) == m_waitApplyCh.end()) {
        return false;
    }
    m_waitApplyCh[raftIndex]->Push(op);
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

/**
 * @brief 构造函数
 * @param me 节点编号
 * @param maxraftstate 快照阈值，raft日志超过这个值时，会触发快照
 * @param nodeInforFileName 节点信息文件名
 * @param port 监听端口
*/
KvServer::KvServer(int me, int maxraftstate, std::string nodeInforFileName, short port) : 
        m_me(me),
        m_applyChan(std::make_shared<LockQueue<ApplyMsg> >()),
        m_maxRaftState(maxraftstate),
        m_skipList(6),  
        m_lastSnapShotRaftLogIndex(0),
        m_iom(std::make_shared<sylar::IOManager>(FIBER_THREAD_NUM, false)),
        m_raftNode(std::make_shared<Raft>(m_iom)),
        m_KvRpcProvider(std::make_shared<KVRpcProvider>(m_iom))
{
    sleep(3);

    // rpc层起来，初始化阶段，调度器也没事情干，可以放调度器执行
    m_iom->schedule([this, port]() -> void {
        // 发布raft层方法，以及对客户端的方法
        this->m_KvRpcProvider->NotifyService(this);
        this->m_KvRpcProvider->NotifyService(this->m_raftNode.get()); 
        // 启动一个rpc服务发布节点
        this->m_KvRpcProvider->KVRpcProviderRunInit(m_me, port);
        this->m_KvRpcProvider->InnerStart();
    });

    // 等其他节点初始化完成
    std::cout << "raftServer node:" << m_me << " start to sleep to wait all ohter raftnode start!!!!" << std::endl;
    sleep(5);
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
        ipPortVt.emplace_back(nodeIp, std::stoi(nodePortStr));
    }

    std::vector<std::shared_ptr<RaftRpcUtil> > otherServers;
    // 进行连接
    for (int i = 0; i < ipPortVt.size(); ++i) {
        if (i == m_me) {
            otherServers.emplace_back(nullptr);
            continue;
        }
        std::string otherNodeIp = ipPortVt[i].first;
        short otherNodePort = ipPortVt[i].second;
        otherServers.emplace_back(std::make_shared<RaftRpcUtil>(otherNodeIp, otherNodePort)); // 与其他节点通信的rpc通道
        std::cout << "node" << m_me << " --> 连接node" << i << " success!" << std::endl;
    }
    // 等待其他节点相互连接
    sleep(ipPortVt.size() - me);

    // 传递给 raft::init 用的 persister
    std::shared_ptr<Persister> persister(new Persister(me));

    // 如果出现重大bug要调试，可以在这里睡眠时间长一点，然后gdb打断点，追踪每一个子进程
    sleep(2);

    // 初始化 raft 节点，也就是raft层
    m_raftNode->init(otherServers, m_me, persister, m_applyChan);

    auto snapshot = persister->ReadSnapshot();
    if (!snapshot.empty()) {
        ReadSnapShotToInstall(snapshot);
    }

    // 这个任务要单开一个独立线程，保证实时性，它是raft层消息的消费者，拿出raft层传过来的commit日志信息
    // 然后真正把命令保存到KVDB，保存成功put操作才会返回(失败的话是超时返回)，get也差不多
    std::thread t2(&KvServer::ReadRaftApplyCommandLoop, this);  
    t2.join();  // 由于ReadRaftApplyCommandLoop是死循环，进程一直卡到这里
}
