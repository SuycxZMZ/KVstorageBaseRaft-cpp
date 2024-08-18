#include "clerk.h"
#include "raftServerRpcUtil.h"
#include "common/util.h"

#include <string>
#include <vector>
std::string Clerk::Get(std::string key) {
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
            server = (server + 1) % m_servers.size();
            continue;
        }
        if (reply.err() == ErrNoKey) {
            return "";
        }
        if (reply.err() == OK || ok) {
            m_curLeaderId = server;
            return reply.value();
        }
    }
    return "";
}

void Clerk::PutAppend(std::string key, std::string value, std::string op) {
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
            DPrintf("[Clerk::PutAppend]原以为的leader：{%d}请求失败，向新leader{%d}重试  ，操作：{%s}", server,
                    server + 1, op.c_str());
            if (!ok) {
                DPrintf("重试原因 ，rpc失敗 ，");
            }
            if (reply.err() == ErrWrongLeader) {
                DPrintf("重试原因：非leader");
            }
            server = (server + 1) % m_servers.size();  // try the next server
            continue;
        }
        if (reply.err() == OK || ok) {
            m_curLeaderId = server;
            return;
        }
    }
}

void Clerk::Put(std::string key, std::string value) { PutAppend(key, value, "Put"); }
void Clerk::Append(std::string key, std::string value) { PutAppend(key, value, "Append"); }

// 初始化客户端
void Clerk::Init(std::string configFileName) {
    // 获取所有raft节点ip、port ，并进行连接
    sylar::rpc::MprpcConfig config; 
    config.LoadConfigFile(configFileName.c_str());
    std::vector<std::pair<std::string, short>> ipPortVt;
    for (int i = 0; i < INT_MAX - 1; ++i) {
        std::string node = "node" + std::to_string(i);
        std::string nodeIp = config.Load(node + "ip");
        std::string nodePortStr = config.Load(node + "port");
        if (nodeIp.empty()) {
            break;
        }
        ipPortVt.emplace_back(nodeIp, std::stoi(nodePortStr)); 
    }
    // 进行连接
    for (const auto& item : ipPortVt) {
        std::string ip = item.first;
        short port = item.second;
        m_servers.emplace_back(std::make_shared<raftServerRpcUtil>(ip, port));
    }
}

Clerk::Clerk() : m_clientId(Uuid()), m_requestId(0), m_curLeaderId(0) {}
