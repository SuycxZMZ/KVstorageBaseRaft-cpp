## raft 算法选举主流程

在 KvServer 构造函数中执行 raft 的构造(默认构造)和初始化(Raft::Init(...) 执行主要的raft节点初始化操作)

```C++
/**
 * @brief Raft 核心类
*/
class Raft : public raftRpcProctoc::raftRpc {
private:
    std::mutex m_mtx; // 互斥锁，用于保护raft状态的修改
    std::vector<std::shared_ptr<RaftRpcUtil>> m_peers; // 保存与其他raft结点通信的rpc入口
    std::shared_ptr<Persister> m_persister; // 持久化层，负责raft数据的持久化

    int m_me; // raft是以集群启动，这个用来标识自己的的编号
    int m_currentTerm; // 记录任期
    int m_votedFor; // 记录当前term给谁投票过
 
    std::vector<raftRpcProctoc::LogEntry> m_logs;   //raft节点保存的全部的日志信息。
    int m_commitIndex;  // 已提交到状态机的日志的index
    int m_lastApplied;  // 已经汇报给状态机（上层应用）的log 的index
    
    std::vector<int> m_nextIndex; // m_nextIndex 保存leader下一次应该从哪一个日志开始发送给follower
    std::vector<int> m_matchIndex; // m_matchIndex表示follower在哪一个日志是已经匹配了的

    enum Status { Follower, Candidate, Leader }; // raft节点身份枚举
    Status m_status; // 节点身份

    std::shared_ptr<LockQueue<ApplyMsg>> applyChan;  // client从这里取日志，client与raft通信的接口

    std::chrono::_V2::system_clock::time_point m_lastResetElectionTime; // 选举超时时间
    std::chrono::_V2::system_clock::time_point m_lastResetHearBeatTime; // 心跳超时，用于leader

    // Snapshot是kvDb的快照，也可以看成是日志，因此:全部的日志 = m_logs + snapshot
    int m_lastSnapshotIncludeIndex; // 最后一个日志的Index
    int m_lastSnapshotIncludeTerm; // 最后一个日志的Term
    std::unique_ptr<monsoon::IOManager> m_ioManager = nullptr; // 协程
};

```

```C++
// raft.cpp
/**
 * @brief 初始化
 * @param peers 与其他raft节点通信的channel
 * @param me 自身raft节点在peers中的索引
 * @param persister 持久化类的 shared_ptr
 * @param applyCh 与kv-server沟通的channel，Raft层负责装填
 */
void Raft::init(std::vector<std::shared_ptr<RaftRpcUtil>> peers, int me, std::shared_ptr<Persister> persister,
                std::shared_ptr<LockQueue<ApplyMsg>> applyCh) {
    m_peers = peers; // 需要与其他raft节点通信类
    m_persister = persister; // 持久化类
    m_me = me; // 标记自己，不能给自己发送rpc
    m_mtx.lock(); // 上锁
    this->applyChan = applyCh; // kvServer 传进来，raft节点负责填充，kvServer后台线程会阻塞等待处理

    m_currentTerm = 0; // 初始化当前任期为0
    m_status = Follower; // 初始化状态为follower
    m_commitIndex = 0; // 初始化commitIndex为0
    m_lastApplied = 0; // 初始化lastApplied为0
    m_logs.clear();

    for (int i = 0; i < m_peers.size(); i++) { // 这两个初始化参数 leader 才用得到
        m_matchIndex.push_back(0);
        m_nextIndex.push_back(0);
    }

    m_votedFor = -1; // 当前term没有给其他人投过票就用-1表示
    m_lastSnapshotIncludeIndex = 0;
    m_lastSnapshotIncludeTerm = 0;
    m_lastResetElectionTime = now();
    m_lastResetHearBeatTime = now();

    readPersist(m_persister->ReadRaftState()); // initialize from state persisted before a crash
    if (m_lastSnapshotIncludeIndex > 0) {
        m_lastApplied = m_lastSnapshotIncludeIndex;
    }

    m_mtx.unlock();
    m_ioManager = std::make_unique<monsoon::IOManager>(FIBER_THREAD_NUM, FIBER_USE_CALLER_THREAD);
    m_ioManager->scheduler([this]() -> void { this->leaderHearBeatTicker(); }); // leader 心跳定时器
    m_ioManager->scheduler([this]() -> void { this->electionTimeOutTicker(); }); // 选举超时定时器，触发就开始发起选举

    // 定期向状态机写入日志。
    // applierTicker时间受到数据库响应延迟和两次apply之间请求数量的影响,
    // 这个随着数据量增多可能不太合理，最好其还是启用一个线程。
    std::thread t3(&Raft::applierTicker, this); 
    t3.detach();
}

// 初始化之后先选举出leader 走 electionTimeOutTicker 协程
/**
 * @brief 1.负责查看是否该发起选举，如果该发起选举就执行doElection发起选举。
 *        2.doElection：实际发起选举，构造需要发送的rpc，并多线程调用sendRequestVote处理rpc及其相应。
 *        3.sendRequestVote：负责发送选举中的RPC，在发送完rpc后还需要负责接收并处理对端发送回来的响应。
 *        4.RequestVote：远端执行，接收别人发来的选举请求，主要检验是否要给对方投票。
 */
void Raft::electionTimeOutTicker() {
    while (true) {
        while (m_status == Leader) { // 如果不睡眠，那么对于leader，这个函数会一直空转。在协程中，会切走
            usleep(HeartBeatTimeout);  // 定时时间没有严谨设置，因为HeartBeatTimeout比选举超时一般小一个数量级
        } 

        //计算距离上次重置选举计时器的时间 + 随机的选举超时时间，然后根据这个时间决定是否睡眠
        std::chrono::duration<signed long int, std::ratio<1, 1000000000>> suitableSleepTime{};
        std::chrono::system_clock::time_point wakeTime{};
        {
            m_mtx.lock();
            wakeTime = now();
            suitableSleepTime = getRandomizedElectionTimeout() + m_lastResetElectionTime - wakeTime;
            m_mtx.unlock();
        }

        //若超时时间未到，进入睡眠状态 .count()返回毫秒数
        if (std::chrono::duration<double, std::milli>(suitableSleepTime).count() > 1) {
            usleep(std::chrono::duration_cast<std::chrono::microseconds>(suitableSleepTime).count());
        }

        // 说明睡眠的这段时间有重置定时器，那么就没有超时，再次睡眠
        if (std::chrono::duration<double, std::milli>(m_lastResetElectionTime - wakeTime).count() > 0) {
            continue;
        }
        //若超时时间已到，调用doElection() 函数启动领导者选举过程。
        doElection();
    }
}

/**
 * @brief 实际发起选举，构造需要发送的rpc，并多线程调用sendRequestVote处理rpc及其相应。
*/
void Raft::doElection() {
    std::lock_guard<std::mutex> g(m_mtx); // 上锁
    if (m_status == Leader) {
    }
    if (m_status != Leader) {
        // 当选举的时候定时器超时就必须重新选举，不然没有选票就会一直卡住
        // 重竞选超时，term也会增加的

        m_status = Candidate; // 设置状态为candidate
        m_currentTerm += 1; // 开始新一轮的选举
        m_votedFor = m_me; // 即是自己给自己投，也避免candidate给同辈的candidate投

        persist(); // 持久化一下
        std::shared_ptr<int> votedNum = std::make_shared<int>(1);  // 使用 make_shared 函数初始化
        m_lastResetElectionTime = now(); //重新设置选举超时定时器

        //发送投票 RPC
        for (int i = 0; i < m_peers.size(); i++) {
            if (i == m_me) {
                continue;
            }
            int lastLogIndex = -1, lastLogTerm = -1;
            getLastLogIndexAndTerm(&lastLogIndex, &lastLogTerm);  // 获取最后一个log的term和下标

            // 构造RPC请求
            std::shared_ptr<raftRpcProctoc::RequestVoteArgs> requestVoteArgs =
                std::make_shared<raftRpcProctoc::RequestVoteArgs>();
            requestVoteArgs->set_term(m_currentTerm);
            requestVoteArgs->set_candidateid(m_me);
            requestVoteArgs->set_lastlogindex(lastLogIndex);
            requestVoteArgs->set_lastlogterm(lastLogTerm);
            auto requestVoteReply = std::make_shared<raftRpcProctoc::RequestVoteReply>();

            // 发送，使用匿名函数执行避免其拿到锁
            std::thread t(&Raft::sendRequestVote, this,
                          i, requestVoteArgs, requestVoteReply, votedNum);
            t.detach();
        }
    }
}

// Raft::sendRequestVote
/**
 * @brief 请求其他结点的投票
 * @param server 请求投票的结点
 * @param args 请求投票的参数
 * @param reply 请求投票的响应
 * @param votedNum 记录投票的结点数量
 * @param return 返回是否成功
*/
bool Raft::sendRequestVote(int server, std::shared_ptr<raftRpcProctoc::RequestVoteArgs> args,
                           std::shared_ptr<raftRpcProctoc::RequestVoteReply> reply, std::shared_ptr<int> votedNum) {
    auto start = now();
    bool ok = m_peers[server]->RequestVote(args.get(), reply.get()); // 发送请求投票，远程调用raft节点的投票
    if (!ok) {
        return ok;  // 不知道为什么不加这个的话如果服务器宕机会出现问题的
    }

    // 对回应进行处理，要记得无论什么时候收到回复就要检查term
    std::lock_guard<std::mutex> lg(m_mtx);
    if (reply->term() > m_currentTerm) { // 三变：身份，term，和投票
        m_status = Follower;  
        m_currentTerm = reply->term();
        m_votedFor = -1;
        persist();
        return true;
    } else if (reply->term() < m_currentTerm) {
        return true;
    }
    if (!reply->votegranted()) { // 如果投票不成功，直接返回
        return true;
    }
    // ---------------- term 相等且投票成功的逻辑 ---------------- //
    *votedNum = *votedNum + 1; // 如果投票成功，记录投票的结点数量，这里在lg锁的保护范围内，操作*votedNum是线程安全的
    if (*votedNum >= m_peers.size() / 2 + 1) {  // 如果投票成功，并且投票数量大于等于一半，那么就变成leader
        *votedNum = 0;
        if (m_status == Leader) { // 如果已经是leader了，那么是就是了，不会进行下一步处理了
            myAssert(false, format("[func-sendRequestVote-rf{%d}]  term:{%d} 同一个term当两次领导，error", m_me,
                                   m_currentTerm));
        }
        m_status = Leader; // 第一次变成leader，初始化状态和 nextIndex、matchIndex
        DPrintf("[func-sendRequestVote rf{%d}] elect success  ,current term:{%d} ,lastLogIndex:{%d}\n", m_me,
                m_currentTerm, getLastLogIndex());

        int lastLogIndex = getLastLogIndex();
        for (int i = 0; i < m_nextIndex.size(); i++) { // 只有leader才需要维护 m_nextIndex 和 m_matchIndex
            m_nextIndex[i] = lastLogIndex + 1;  
            m_matchIndex[i] = 0;                // 每换一个领导都是从0开始 ？
        }
        std::thread t(&Raft::doHeartBeat, this);  // 马上向其他节点宣告自己就是leader
        t.detach();
        persist();
    }
    return true;
}

// 其他节点进行投票的逻辑
/**
 * @brief 变成 candidate 之后需要让其他结点给自己投票，candidate通过该函数远程调用远端节点的投票函数
 * @param args 请求投票的参数
 * @param reply 请求投票的响应
 */
void Raft::RequestVote(const raftRpcProctoc::RequestVoteArgs* args, raftRpcProctoc::RequestVoteReply* reply) {
    std::lock_guard<std::mutex> lg(m_mtx);
    DEFER { // 应该先持久化，再撤销lock
        persist();
    };
    // 对args的term的三种情况分别进行处理，大于小于等于自己的term都是不同的处理
    //  reason: 出现网络分区，该竞选者已经OutOfDate(过时）
    if (args->term() < m_currentTerm) {
        reply->set_term(m_currentTerm);
        reply->set_votestate(Expire);
        reply->set_votegranted(false);
        return;
    }

    // 如果任何时候rpc请求或者响应的term大于自己的term，更新term，并变成follower
    if (args->term() > m_currentTerm) {
        m_status = Follower;
        m_currentTerm = args->term();
        m_votedFor = -1;

        // 重置定时器：收到leader的ae，开始选举，透出票
        // 这时候更新了term之后，votedFor也要置为-1
    }

    // 现在节点任期都是相同的(任期小的也已经更新到新的args的term了)，还需要检查log的term和index是不是匹配的了
    int lastLogTerm = getLastLogTerm();
    // 只有没投票，且candidate的日志的新的程度 ≥ 接受者的日志新的程度 才会授票
    if (!UpToDate(args->lastlogindex(), args->lastlogterm())) {
        // 日志太旧了
        if (args->lastlogterm() < lastLogTerm) {
        } else {
        }
        reply->set_term(m_currentTerm);
        reply->set_votestate(Voted);
        reply->set_votegranted(false);
        return;
    }

    // 当因为网络质量不好导致的请求丢失重发就有可能！！！！
    // 因此需要避免重复投票
    if (m_votedFor != -1 && m_votedFor != args->candidateid()) {
        reply->set_term(m_currentTerm);
        reply->set_votestate(Voted);
        reply->set_votegranted(false);
        return;
    } else {
        m_votedFor = args->candidateid();
        m_lastResetElectionTime = now();  // 认为必须要在投出票的时候才重置定时器，
        reply->set_term(m_currentTerm);
        reply->set_votestate(Normal);
        reply->set_votegranted(true);
        return;
    }
}
```