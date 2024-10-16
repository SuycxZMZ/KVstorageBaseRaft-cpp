#include "clerk.h"
#include "raftServerRpcUtil.h"
#include "common/util.h"
#include "rpc/rpcConfig.h"
#include <string>
#include <vector>
#include "spdlog/spdlog.h"
std::string Clerk::Get(const std::string& key) {
    m_requestId++;
    int requestId = m_requestId;
    int server = m_curLeaderId;

    // 组装请求
    raftKVRpcProctoc::GetArgs args;
    args.set_key(key);
    args.set_clientid(m_clientId);
    args.set_requestid(requestId);

    while (true) {
        // 发送请求
        raftKVRpcProctoc::GetReply reply;

        // RPC调用
        bool ok = m_servers[server]->Get(&args, &reply); 
        if (!ok || 
            reply.err() == ErrWrongLeader) 
        {  // 会一直重试，因为requestId没有改变，因此可能会因为RPC的丢失或者其他情况导致重试，kvserver层来保证不重复执行（线性一致性）
            server = (server + 1) % (int)m_servers.size();
            continue;
        }
        if (reply.err() == ErrNoKey) {
            return "";
        }
        if (reply.err() == OK) {
            m_curLeaderId = server;
            return reply.value();
        }
    }
}

void Clerk::PutAppend(const std::string& key, const std::string& value, const std::string& op) {
    m_requestId++;
    int requestId = m_requestId;
    int server = m_curLeaderId;
    while (true) {
        raftKVRpcProctoc::PutAppendArgs args;
        args.set_key(key);
        args.set_value(value);
        args.set_op(op);
        args.set_clientid(m_clientId);
        args.set_requestid(requestId);
        raftKVRpcProctoc::PutAppendReply reply;

        // RPC调用
        bool ok = m_servers[server]->PutAppend(&args, &reply);
        if (!ok || reply.err() == ErrWrongLeader) {
            SPDLOG_ERROR("Clerk::PutAppend 原以为的leader：{} 请求失败，向新leader重试，操作：{}", server + 1, op.c_str());
            if (!ok) {
                SPDLOG_ERROR("重试原因，rpc失败");
            }
            if (reply.err() == ErrWrongLeader) {
                SPDLOG_ERROR("重试原因：非leader");
            }
            server = (server + 1) % (int)m_servers.size();  // try the next server
            continue;
        }
        if (reply.err() == OK) {
            m_curLeaderId = server;
            return;
        }
    }
}

[[maybe_unused]] void Clerk::Put(const std::string& key, const std::string& value) { PutAppend(key, value, "Put"); }
void Clerk::Append(const std::string& key, const std::string& value) { PutAppend(key, value, "Append"); }

// 初始化客户端
void Clerk::Init(const std::string& configFileName) {
    // 获取所有raft节点ip、port ，并初始化grpc客户端
    // rpcConfig::GetInstance() config;
    rpcConfig::GetInstance().LoadConfigFile(configFileName.c_str());

    for (int i = 0; i < INT_MAX - 1; ++i) {
        std::string node = "node" + std::to_string(i);
        std::string nodeIpPort = rpcConfig::GetInstance().Load(node + "ip");
        if (nodeIpPort == "") break;
        nodeIpPort += ':';
        nodeIpPort += rpcConfig::GetInstance().Load(node + "port");
        m_servers.emplace_back(std::make_shared<raftServerRpcUtil>(nodeIpPort));
    }
}

Clerk::Clerk() : m_clientId(Uuid()), m_requestId(0), m_curLeaderId(0) {}
