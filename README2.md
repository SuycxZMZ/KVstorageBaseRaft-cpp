# 主要流程

## 1. 从使用开始

### KvServer

执行 raftKvDB main函数中起若干子进程，每个进程根据配置文件参数负责一个 raftServer --> KvServer

```C++
/**
 * @brief kvServer负责与外部clerk通信
 *        一个外部请求的处理可以简单的看成两步：
 *        1.接收外部请求。
 *        2.本机内部与raft和kvDB协商如何处理该请求。
 *        3.返回外部响应。
*/
class KvServer : raftKVRpcProctoc::kvServerRpc {
private:
    std::mutex m_mtx;
    int m_me;
    std::shared_ptr<Raft> m_raftNode;                 // raft节点
    std::shared_ptr<LockQueue<ApplyMsg> > applyChan;  // kvServer中拿到的消息，server用这些消息与raft打交道，由Raft::applierTicker线程填充
    int m_maxRaftState;                               // snapshot if log grows this big
    std::string m_serializedKVData;                      // 序列化后的kv数据，理论上可以不用是目前没有找到特别好的替代方法
    SkipList<std::string, std::string> m_skipList;       // skipList，用于存储kv数据
    std::unordered_map<std::string, std::string> m_kvDB; // kvDB，用unordered_map来替代
    std::unordered_map<int, LockQueue<Op> *> waitApplyCh;// 字段含义 waitApplyCh是一个map，键是int，值是Op类型的管道
    std::unordered_map<std::string, int> m_lastRequestId;  // clientid -> requestID 一个kV服务器可能连接多个client
    int m_lastSnapShotRaftLogIndex;
public:  // for rpc
    /**
     * @brief 与客户打交道，Rpc框架调用
    */
    void PutAppend(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::PutAppendArgs *request,
                   ::raftKVRpcProctoc::PutAppendReply *response, ::google::protobuf::Closure *done) override;
    void Get(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::GetArgs *request,
             ::raftKVRpcProctoc::GetReply *response, ::google::protobuf::Closure *done) override;
};

/**
 * @brief 构造函数
 * @details 1. 初始化成员函数
 *          2. 初始化本server代表的raft节点，发布server和raft节点的rpc方法
 *          3. 获取其他节点信息，并进行连接
 *          4. kvDB初始化
 * @param me 节点编号
 * @param maxraftstate 快照阈值，raft日志超过这个值时，会触发快照
 * @param nodeInforFileName 节点信息文件名
 * @param port 监听端口
*/
KvServer::KvServer(int me, int maxraftstate, std::string nodeInforFileName, short port) : m_skipList(6) {
    std::shared_ptr<Persister> persister = std::make_shared<Persister>(me);
    m_me = me;
    m_maxRaftState = maxraftstate;
    applyChan = std::make_shared<LockQueue<ApplyMsg> >();
    m_raftNode = std::make_shared<Raft>();
    // clerk层面 kvserver开启rpc接受功能
    // 同时raft与raft节点之间也要开启rpc功能，因此有两个注册
    std::thread t([this, port]() -> void {
        RpcProvider provider; // provider是一个rpc网络服务对象。把UserService对象发布到rpc节点上
        provider.NotifyService(this); // 发布给客户端调用的rpc方法，下面则是发布供其他raft节点调用的rpc方法
        provider.NotifyService(this->m_raftNode.get());  // 这里获取了原始指针，后面检查一下有没有泄露
        provider.Run(m_me, port); // 启动一个rpc服务发布节点Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    });
    t.detach();

    //开启rpc远程调用能力，需要注意必须要保证所有节点都开启rpc接受功能之后才能开启rpc远程调用能力
    //这里使用睡眠来保证
    sleep(6);
    // 获取所有raft节点ip、port ，并进行连接  ,要排除自己
    MprpcConfig config;
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
        auto *rpc = new RaftRpcUtil(otherNodeIp, otherNodePort);
        servers.push_back(std::shared_ptr<RaftRpcUtil>(rpc)); 
    }
    sleep(ipPortVt.size() - me);  // 等待所有节点相互连接成功，再启动raft
    m_raftNode->init(servers, m_me, persister, applyChan);
    // kv的server直接与raft通信，但kv不直接与raft通信，所以需要把ApplyMsg的chan传递下去用于通信，两者的persist也是共用的

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

```