#include "kvServer.h"
#include "common/config.h"
#include "rpc/KVrpcprovider.h"
#include "sylar/rpc/rpcconfig.h"

void KvServer::DprintfKVDB() {
    if (!Debug) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mtx);
    m_skipList.display_list();
}

void KvServer::ExecuteGetOpOnKVDB(Op op, std::string *value, bool *exist) {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        *value = "";
        *exist = false;
        if (m_skipList.search_element(op.Key, *value)) {
            *exist = true;
        }
        m_lastRequestClientAndId[op.ClientId] = op.RequestId;
    }
    DprintfKVDB();
}

void KvServer::ExecuteAppendOpOnKVDB(Op op) {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_skipList.insert_set_element(op.Key, op.Value);
        m_lastRequestClientAndId[op.ClientId] = op.RequestId;
    }
    DprintfKVDB();
}

void KvServer::ExecutePutOpOnKVDB(Op op) {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_skipList.insert_set_element(op.Key, op.Value);
        m_lastRequestClientAndId[op.ClientId] = op.RequestId;
    }
    DprintfKVDB();
}

// 处理来自clerk的Get RPC
void KvServer::Get(const raftKVRpcProctoc::GetArgs *args, raftKVRpcProctoc::GetReply *reply) {
    Op op;
    op.Operation = "Get";
    op.Key = args->key();
    op.Value = "";
    op.ClientId = args->clientid();
    op.RequestId = args->requestid();

    int newLogIndex = -1;
    int _ = -1;

    // 发起一个日志
    int isLeader = m_raftNode->Start(op, &newLogIndex, &_);
    if (!isLeader) {
        reply->set_err(ErrWrongLeader);
        return;
    }

    LockQueue<Op> *chFornewLogIndex = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (m_waitApplyCh.find(newLogIndex) == m_waitApplyCh.end()) {
            m_waitApplyCh.emplace(newLogIndex, new LockQueue<Op>());
        }
        chFornewLogIndex = m_waitApplyCh[newLogIndex];
    }

    Op raftCommitOp;
    if (!chFornewLogIndex->timeOutPop(CONSENSUS_TIMEOUT, &raftCommitOp)) {  // 待执行命令 为空
        int _ = -1;
        int isLeader = m_raftNode->GetState(&_);
        if (ifRequestDuplicate(op.ClientId, op.RequestId) && isLeader) {
            std::string value;
            bool exist = false;
            // 操作 Get请求 就是查找kvDB
            ExecuteGetOpOnKVDB(op, &value, &exist);
            if (exist) {  // 如果是已经提交过的get请求，是可以再执行的,不会违反线性一致性
                reply->set_err(OK);
                reply->set_value(value);
            } else {
                reply->set_err(ErrNoKey);
                reply->set_value("");
            }
        } else {
            reply->set_err(ErrWrongLeader);  // 返回这个，其实就是让clerk换一个节点重试
        }
    } else {
        // raft已经提交了该command（op），可以正式开始执行了
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

    /// todo 这里要仔细考虑一下，每次阻塞弹出一个，用完就销毁，是不是不太合理
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto tmp = m_waitApplyCh[newLogIndex];
        m_waitApplyCh.erase(newLogIndex);
        delete tmp;
    }
}

/**
 * @brief 从raft节点获取命令，操作kvDB，单次调用执行一个命令
 *        如果命令有效，就放入 m_waitApplyChan
 * @param message 解析raft层传过来的命令
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
    }
    // 到这里kvDB已经制作了快照
    if (m_maxRaftState != -1) {
        IfNeedToSendSnapShotCommand(message.CommandIndex, 9);
    }

    // Send message to the chan of op.ClientId
    SendMessageToWaitChan(op, message.CommandIndex);
}

bool KvServer::ifRequestDuplicate(std::string ClientId, int RequestId) {
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_lastRequestClientAndId.find(ClientId) == m_lastRequestClientAndId.end()) {
        return false;
        // todo :不存在这个client就创建
    }
    return RequestId <= m_lastRequestClientAndId[ClientId];
}

// get和put//append执行的具体细节是不一样的
void KvServer::PutAppend(const raftKVRpcProctoc::PutAppendArgs *args, raftKVRpcProctoc::PutAppendReply *reply) {
    Op op;
    op.Operation = args->op();
    op.Key = args->key();
    op.Value = args->value();
    op.ClientId = args->clientid();
    op.RequestId = args->requestid();
    int newLogIndex = -1;
    int _ = -1;
    int isleader = m_raftNode->Start(op, &newLogIndex, &_);

    if (0 == isleader) {
        DPrintf(
            "[func -KvServer::PutAppend -kvserver{%d}]From Client %s (Request %d) To Server %d, key %s, newLogIndex %d "
            ", "
            "but "
            "not leader",
            m_me, &args->clientid(), args->requestid(), m_me, &op.Key, newLogIndex);

        reply->set_err(ErrWrongLeader);
        return;
    }
    DPrintf(
        "[func -KvServer::PutAppend -kvserver{%d}]From Client %s (Request %d) To Server %d, key %s, newLogIndex %d , "
        "is "
        "leader ",
        m_me, &args->clientid(), args->requestid(), m_me, &op.Key, newLogIndex);
    LockQueue<Op> *chFornewLogIndex = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (m_waitApplyCh.find(newLogIndex) == m_waitApplyCh.end()) {
            m_waitApplyCh.emplace(newLogIndex, new LockQueue<Op>());
        }
        chFornewLogIndex = m_waitApplyCh[newLogIndex];
    }

    // timeout
    Op raftCommitOp;

    if (!chFornewLogIndex->timeOutPop(CONSENSUS_TIMEOUT, &raftCommitOp)) {
        DPrintf(
            "[func -KvServer::PutAppend -kvserver{%d}]TIMEOUT PUTAPPEND !!!! Server %d , get Command <-- Index:%d , "
            "ClientId %s, RequestId %d, Opreation %s Key :%s, Value :%s",
            m_me, m_me, newLogIndex, op.ClientId.c_str(), op.RequestId, op.Operation.c_str(), op.Key.c_str(),
            op.Value.c_str());

        if (ifRequestDuplicate(op.ClientId, op.RequestId)) {
            reply->set_err(
                OK);  // 超时了,但因为是重复的请求，返回ok，实际上就算没有超时，在真正执行的时候也要判断是否重复
        } else {
            reply->set_err(ErrWrongLeader);  /// 这里返回这个的目的让clerk重新尝试
        }
    } else {
        DPrintf(
            "[func -KvServer::PutAppend -kvserver{%d}]WaitChanGetRaftApplyMessage<--Server %d , get Command <-- "
            "Index:%d , "
            "ClientId %s, RequestId %d, Opreation %s, Key :%s, Value :%s",
            m_me, m_me, newLogIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);
        if (raftCommitOp.ClientId == op.ClientId && raftCommitOp.RequestId == op.RequestId) {
            // 可能发生leader的变更导致日志被覆盖，因此必须检查
            reply->set_err(OK);
        } else {
            reply->set_err(ErrWrongLeader);
        }
    }

    /// todo 这里要仔细考虑一下，每次阻塞弹出一个，用完就销毁，是不是不太合理
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto tmp = m_waitApplyCh[newLogIndex];
        m_waitApplyCh.erase(newLogIndex);
        delete tmp;
    }
}

/**
 * @brief 一直等待 raft 传来的m_applyChan待应用日志消息
 */
void KvServer::ReadRaftApplyCommandLoop() {
    while (true) {
        auto message = m_applyChan->Pop();  // 阻塞弹出
        // 应用到 KVDB
        if (message.CommandValid) {
            GetCommandFromRaft(message);
        }

        // 一般 这个分支是从节点才能走到的，主节点把数据库快照发过来
        if (message.SnapshotValid) {
            GetSnapShotFromRaft(message);
        }
    }
}

void KvServer::ReadSnapShotToInstall(std::string snapshot) {
    if (snapshot.empty()) {
        return;
    }
    parseFromString(snapshot);
}

bool KvServer::SendMessageToWaitChan(const Op &op, int newLogIndex) {
    std::lock_guard<std::mutex> lock(m_mtx);
    DPrintf(
        "[RaftApplyMessageSendToWaitChan--> raftserver{%d}] , Send Command --> Index:{%d} , ClientId {%d}, RequestId "
        "{%d}, Opreation {%v}, Key :{%v}, Value :{%v}",
        m_me, newLogIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);

    if (m_waitApplyCh.find(newLogIndex) == m_waitApplyCh.end()) {
        return false;
    }
    m_waitApplyCh[newLogIndex]->Push(op);
    DPrintf(
        "[RaftApplyMessageSendToWaitChan--> raftserver{%d}] , Send Command --> Index:{%d} , ClientId {%d}, RequestId "
        "{%d}, Opreation {%v}, Key :{%v}, Value :{%v}",
        m_me, newLogIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);
    return true;
}

void KvServer::IfNeedToSendSnapShotCommand(int newLogIndex, int proportion) {
    // 触发快照才发给raft层
    if (m_raftNode->GetRaftStateSize() > m_maxRaftState / 10.0) {
        // Send SnapShot Command
        auto snapshot = MakeSnapShot();
        m_raftNode->Snapshot(newLogIndex, snapshot);
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
KvServer::KvServer(int me, int maxraftstate, std::string nodeInforFileName, short port)
    : m_me(me),
      m_applyChan(std::make_shared<LockQueue<ApplyMsg> >()),
      m_maxRaftState(maxraftstate),
      m_skipList(6),
      m_lastSnapShotRaftLogIndex(0),
      m_iom(std::make_shared<sylar::IOManager>(FIBER_THREAD_NUM, false)),
      m_raftNode(std::make_shared<Raft>(m_iom)),
      m_KvRpcProvider(std::make_shared<KVRpcProvider>(m_iom)) {
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
        otherServers.emplace_back(
            std::make_shared<RaftRpcUtil>(otherNodeIp, otherNodePort));  // 与其他节点通信的rpc通道
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
