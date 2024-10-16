#include "kvServer.h"
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <memory>
#include <string>
#include <thread>
#include "common/config.h"
#include "rpc/rpcConfig.h"
#include "spdlog/spdlog.h"

void KvServer::DprintfKVDB() {
#ifdef Dprintf_Debug
    std::lock_guard<std::mutex> lock(m_mtx);
    m_skipList.display_list();
#endif
}

void KvServer::ExecuteGetOpOnKVDB(const Op &op, std::string *value, bool *exist) {
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
    bool isLeader = m_raftNode->Start(op, &newLogIndex, &_);
    if (!isLeader) {
        reply->set_err(ErrWrongLeader);
        return;
    }

    std::shared_ptr<waitCh> chFornewLogIndex;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (m_waitApplyCh.find(newLogIndex) == m_waitApplyCh.end()) {
            m_waitApplyCh.emplace(newLogIndex, std::make_shared<waitCh>(2));
        }
        chFornewLogIndex = m_waitApplyCh[newLogIndex];
    }

    Op raftCommitOp;
    if (!chFornewLogIndex->try_pop_with_timeout(raftCommitOp, CONSENSUS_TIMEOUT)) {  // 待执行命令 为空
        int _ = -1;
        isLeader = m_raftNode->GetState(&_);
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
    std::lock_guard<std::mutex> lock(m_mtx);
    m_waitApplyCh.erase(newLogIndex);
}

/**
 * @brief 从raft节点获取命令，操作kvDB，单次调用执行一个命令
 *        如果命令有效，就放入 m_waitApplyChan
 * @param message 解析raft层传过来的命令
 */
void KvServer::GetCommandFromRaft(ApplyMsg message) {
    Op op;
    op.parseFromString(message.Command);
    SPDLOG_INFO(
        "KvServer::GetCommandFromRaft-kvserver:{}, Got Command --> Index:{}, ClientId {}, RequestId {}, Operation {}, "
        "Key :{}, Value :{}",
        m_me, message.CommandIndex, op.ClientId, op.RequestId, op.Operation, op.Key, op.Value);
    if (message.CommandIndex <= m_lastSnapShotRaftLogIndex) {
        return;
    }

    if (!ifRequestDuplicate(op.ClientId, op.RequestId)) {  // 没操作过再操作KVDB
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

bool KvServer::ifRequestDuplicate(const std::string &ClientId, int RequestId) {
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
    bool isleader = m_raftNode->Start(op, &newLogIndex, &_);

    if (!isleader) {
        SPDLOG_ERROR(
            "KvServer::PutAppend kvserver:{} From Client {} (Request {}) To Server:{}, key {}, newLogIndex {} but not "
            "leader",
            m_me, args->clientid(), args->requestid(), m_me, args->key(), newLogIndex);
        reply->set_err(ErrWrongLeader);
        return;
    }
    SPDLOG_DEBUG("KvServer::PutAppend kvserver:{} From Client {} (Request {}) To Server:{}, key {}, newLogIndex {}",
                 m_me, args->clientid(), args->requestid(), m_me, args->key(), newLogIndex);
    std::shared_ptr<waitCh> chFornewLogIndex;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (m_waitApplyCh.find(newLogIndex) == m_waitApplyCh.end()) {
            m_waitApplyCh.emplace(newLogIndex, std::make_shared<waitCh>(2));
        }
        chFornewLogIndex = m_waitApplyCh[newLogIndex];
    }

    // timeout
    Op raftCommitOp;
    if (!chFornewLogIndex->try_pop_with_timeout(raftCommitOp, CONSENSUS_TIMEOUT)) {
        SPDLOG_ERROR(
            "KvServer::PutAppend -> kvserver:{} timeout putappend !!! server:{} , get Command <-- Index:{}, ClientId "
            "{}, RequestId {}, Opreation {}, Key :{}, Value :{}",
            m_me, m_me, newLogIndex, op.ClientId, op.RequestId, op.Operation, op.Key, op.Value);
        if (ifRequestDuplicate(op.ClientId, op.RequestId)) {
            reply->set_err(
                OK);  // 超时了,但因为是重复的请求，返回ok，实际上就算没有超时，在真正执行的时候也要判断是否重复
        } else {
            reply->set_err(ErrWrongLeader);  // 让clerk重新尝试
        }
    } else {
        SPDLOG_DEBUG(
            "KvServer::PutAppend -> kvserver:{} WaitChanGetRaftApplyMessage<--Server:{},get Command <-- Index:{}, "
            "ClientId {},RequestId {},Opreation {},Key :{},Value :{}",
            m_me, m_me, newLogIndex, op.ClientId, op.RequestId, op.Operation, op.Key, op.Value);
        if (raftCommitOp.ClientId == op.ClientId && raftCommitOp.RequestId == op.RequestId) {
            // 可能发生leader的变更导致日志被覆盖，因此必须检查
            reply->set_err(OK);
        } else {
            reply->set_err(ErrWrongLeader);
        }
    }
    std::lock_guard<std::mutex> lock(m_mtx);
    m_waitApplyCh.erase(newLogIndex);
}

/**
 * @brief 一直等待 raft 传来的m_applyChan待应用日志消息
 */
[[noreturn]] void KvServer::ReadRaftApplyCommandLoop() {
    while (true) {
        ApplyMsg message;
        *m_applyChan >> message;
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

void KvServer::ReadSnapShotToInstall(const std::string &snapshot) {
    if (snapshot.empty()) {
        return;
    }
    parseFromString(snapshot);
}

bool KvServer::SendMessageToWaitChan(const Op &op, int newLogIndex) {
    std::lock_guard<std::mutex> lock(m_mtx);
    SPDLOG_DEBUG(
        "KvServer::RaftApplyMessageSendToWaitChan--> raftserver:{}, Send Command --> Index:{}, ClientId {}, RequestId "
        "{}, Opreation {}, Key :{}, Value :{}",
        m_me, newLogIndex, &op.ClientId, op.RequestId, &op.Operat, op.Key, op.Value);
    if (m_waitApplyCh.find(newLogIndex) == m_waitApplyCh.end()) {
        return false;
    }
    *m_waitApplyCh[newLogIndex] << op;
    return true;
}

void KvServer::IfNeedToSendSnapShotCommand(int newLogIndex, [[maybe_unused]] int proportion) {
    // 触发快照才发给raft层
    if (m_raftNode->GetRaftStateSize() > m_maxRaftState / 10) {
        // Send SnapShot Command
        auto snapshot = MakeSnapShot();
        m_raftNode->Snapshot(newLogIndex, snapshot);
    }
}

void KvServer::GetSnapShotFromRaft(const ApplyMsg &message) {
    std::lock_guard<std::mutex> lock(m_mtx);

    if (Raft::CondInstallSnapshot(message.Snapshot)) {
        ReadSnapShotToInstall(message.Snapshot);
        m_lastSnapShotRaftLogIndex = message.SnapshotIndex;
    }
}

std::string KvServer::MakeSnapShot() {
    std::lock_guard<std::mutex> lock(m_mtx);
    std::string snapshotData = getSnapshotData();
    return snapshotData;
}

::grpc::Status KvServer::PutAppend(::grpc::ServerContext *context, const ::raftKVRpcProctoc::PutAppendArgs *request,
                                   ::raftKVRpcProctoc::PutAppendReply *response) {
    KvServer::PutAppend(request, response);
    // TODO 先暂时返回OK
    return grpc::Status::OK;
}

::grpc::Status KvServer::Get(::grpc::ServerContext *context, const ::raftKVRpcProctoc::GetArgs *request,
                             ::raftKVRpcProctoc::GetReply *response) {
    KvServer::Get(request, response);
    // TODO 先暂时返回OK
    return grpc::Status::OK;
}

void KvServer::GrpcBuilderInit(const std::string &nodeInforFileName) {
    // grpcServer初始化
    // 获取 ip和端口号
    // rpcConfig config;
    rpcConfig::GetInstance().LoadConfigFile(nodeInforFileName.c_str());
    std::string self_node = "node" + std::to_string(m_me);
    std::string nodeIpPort = rpcConfig::GetInstance().Load(self_node + "ip") + ':';
    nodeIpPort += rpcConfig::GetInstance().Load(self_node + "port");

    m_grpcBuilder.AddListeningPort(nodeIpPort, ::grpc::InsecureServerCredentials());
    // 注册 kvServer和raft层的rpc方法
    m_grpcBuilder.RegisterService(this);
    m_grpcBuilder.RegisterService(m_raftNode.get());
}

/**
 * @brief 构造函数
 * @param me 节点编号
 * @param maxraftstate 快照阈值，raft日志超过这个值时，会触发快照
 * @param nodeInforFileName 节点信息文件名
 * @param port 监听端口
 */
KvServer::KvServer(int me, int maxraftstate, const std::string &nodeInforFileName, short port)
    : m_me(me),
      m_applyChan(std::make_shared<applyCh>(2)),
      m_maxRaftState(maxraftstate),
      m_skipList(6),
      m_lastSnapShotRaftLogIndex(0),
      m_raftNode(std::make_shared<Raft>()),
      m_grpcBuilder() {
    spdlog::set_level(spdlog::level::debug);
    sleep(1);

    GrpcBuilderInit(nodeInforFileName);
    m_grpcServer = m_grpcBuilder.BuildAndStart();
    // 手动异步开启grpcServer
    std::thread grpc_server_thread([this]() { m_grpcServer->Wait(); });
    grpc_server_thread.detach();

    spdlog::info("raftServer node:{} start to sleep to wait all ohter raftnode start!!!", m_me);
    sleep(2);
    spdlog::info("raftServer node:{} wake up!!!! start to connect other raftnode", m_me);

    // 获取所有raft节点ip、port,并进grpc客户端的初始化,并排除自己
    std::vector<std::shared_ptr<RaftRpcUtil>> otherServers;
    for (int i = 0; i < INT_MAX - 1; ++i) {
        if (i == m_me) {
            otherServers.emplace_back(nullptr);
            continue;
        }
        std::string node = "node" + std::to_string(i);
        std::string nodeIpPort = rpcConfig::GetInstance().Load(node + "ip");
        if (nodeIpPort.empty()) {
            break;
        }
        nodeIpPort += ':';
        nodeIpPort += rpcConfig::GetInstance().Load(node + "port");
        otherServers.emplace_back(std::make_shared<RaftRpcUtil>(nodeIpPort));
    }

    sleep(otherServers.size() - me);

    // 传递给 raft::init 用的 persister
    auto persister = std::make_shared<Persister>(me);

    // 如果出现重大bug要调试，可以在这里睡眠时间长一点，然后gdb打断点，追踪每一个子进程
    sleep(2);

    // 初始化 raft 节点，也就是raft层
    m_raftNode->init(otherServers, m_me, persister, m_applyChan);

    // 可能是异常重启，如果之前有快照，就恢复快照的内容
    auto snapshot = persister->ReadSnapshot();
    if (!snapshot.empty()) {
        ReadSnapShotToInstall(snapshot);
    }
    // 这个任务要单开一个独立线程，保证实时性，它是raft层消息的消费者，拿出raft层传过来的commit日志信息
    // 然后真正把命令保存到KVDB，保存成功put操作才会返回(失败的话是超时返回)，get也差不多
    std::thread apply_thread(&KvServer::ReadRaftApplyCommandLoop, this);
    apply_thread.join();  // 由于ReadRaftApplyCommandLoop是死循环，进程一直卡到这里
}
