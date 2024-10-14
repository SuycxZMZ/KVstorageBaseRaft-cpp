#ifndef SKIP_LIST_ON_RAFT_CLERK_H
#define SKIP_LIST_ON_RAFT_CLERK_H
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cerrno>
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
        boost::uuids::uuid uid = boost::uuids::random_generator()();
        return boost::uuids::to_string(uid);
    }

    // MakeClerk
    void PutAppend(const std::string& key, const std::string& value, const std::string& op);

public:
    // 对外暴露的三个功能和初始化
    void Init(const std::string& configFileName);
    std::string Get(const std::string& key);

    [[maybe_unused]] void Put(const std::string& key, const std::string& value);
    void Append(const std::string& key, const std::string& value);

public:
    Clerk();
};

#endif  // SKIP_LIST_ON_RAFT_CLERK_H
