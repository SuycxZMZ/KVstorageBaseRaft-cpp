//
// Created by swx on 23-6-4.
//

#ifndef SKIP_LIST_ON_RAFT_CLERK_H
#define SKIP_LIST_ON_RAFT_CLERK_H
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <random>
#include <string>
#include <vector>
#include "raftServerRpcUtil.h"

class Clerk {
private:
    std::vector<std::shared_ptr<raftServerRpcUtil>> m_servers;  // 与所有KV节点通信的stub
    std::string m_clientId;
    int m_requestId; 
    int m_curLeaderId;  // 只是有可能是领导

    static std::string Uuid() {
        // 创建一个随机数生成器
        std::random_device rd;                             // 获取随机数种子
        std::mt19937 gen(rd());                            // 使用 Mersenne Twister 算法作为随机数引擎
        std::uniform_int_distribution<> dis(0, RAND_MAX);  // 定义随机数的分布范围

        // 生成并返回 UUID
        return std::to_string(dis(gen)) + std::to_string(dis(gen)) + std::to_string(dis(gen)) +
               std::to_string(dis(gen));
    }

    //    MakeClerk
    void PutAppend(const std::string& key, const std::string& value, const std::string& op);

public:
    // 对外暴露的三个功能和初始化
    void Init(const std::string& configFileName);
    std::string Get(const std::string& key);

    [[maybe_unused]] [[maybe_unused]] void Put(const std::string& key, const std::string& value);
    void Append(const std::string& key, const std::string& value);

public:
    Clerk();
};

#endif  // SKIP_LIST_ON_RAFT_CLERK_H
