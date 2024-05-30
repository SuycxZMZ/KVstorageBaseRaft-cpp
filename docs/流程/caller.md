## caller

主要是作为调用方，远程调用kvServer的rpc方法，这里 raft 与 kvServer 的所有细节对 caller 都是透明的。
caller 要做的就是根据配置文件初始化好raftServerRpcUtil 对象，然后调用相应的rpc方法。
关于caller更多关注的是rpc流程的调用 [RPC流程](https://github.com/SuycxZMZ/MpRPC-Cpp)
```C++
// caller.cpp
int main() {
    Clerk client;
    client.Init("test.conf");
    auto start = now();
    int count = 500;
    int tmp = count;
    while (tmp--) {
        client.Put("x", std::to_string(tmp));
        std::string get1 = client.Get("x");
        std::printf("get return :{%s}\r\n", get1.c_str());
    }
    return 0;
}

// clerk.h
class Clerk {
private:
    std::vector<std::shared_ptr<raftServerRpcUtil>> m_servers;  // 保存所有raft节点的fd 
    std::string m_clientId;
    int m_requestId; 
    int m_recentLeaderId;  // 只是有可能是领导

    std::string Uuid() {
        return randstring; // 用于返回随机的clientId
    }  
    void PutAppend(std::string key, std::string value, std::string op);
public:
    // 对外暴露的三个功能和初始化
    void Init(std::string configFileName);
    std::string Get(std::string key);
    void Put(std::string key, std::string value);
    void Append(std::string key, std::string value);
public:
    Clerk();
};

// clerk.cpp
std::string Clerk::Get(std::string key) {
    m_requestId++;
    auto requestId = m_requestId;
    int server = m_recentLeaderId;
    // 组请求
    raftKVRpcProctoc::GetArgs args;
    args.set_key(key);
    args.set_clientid(m_clientId);
    args.set_requestid(requestId);

    while (true) {
        // 发送请求，即rpc调用
        raftKVRpcProctoc::GetReply reply;
        bool ok = m_servers[server]->Get(&args, &reply); 
        if (!ok ||
            reply.err() ==
                ErrWrongLeader) {  // 会一直重试，因为requestId没有改变，因此可能会因为RPC的丢失或者其他情况导致重试，kvserver层来保证不重复执行（线性一致性）
            server = (server + 1) % m_servers.size();
            continue;
        }
        if (reply.err() == ErrNoKey) {
            return "";
        }
        if (reply.err() == OK) {
            m_recentLeaderId = server;
            return reply.value();
        }
    }
    return "";
}
```