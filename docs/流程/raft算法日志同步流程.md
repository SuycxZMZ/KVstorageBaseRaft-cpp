## raft日志同步流程

同步的过程可以看作 leader 一直向 follower 发送心跳，如果有新的日志条目要同步，leader 就会将新的日志信息带上发过去

follower 给 leader 回复日志在自己节点上的添加情况，leader 在收到多数节点的回复后，就认为日志同步成功，不回复的follower不管

日志同步主要在 leaderHearBeatTicker 协程

```C++
/**
 * @brief 1.检查是否需要发起心跳（leader）如果该发起就执行doHeartBeat。
 *        2.doHeartBeat:实际发送心跳，判断到底是构造需要发送的rpc，并多线程调用sendRequestVote处理rpc及其相应。
 *        3.sendAppendEntries:负责发送日志的RPC，在发送完rpc后还需要负责接收并处理对端发送回来的响应。 
 *        4.leaderSendSnapShot:负责发送快照的RPC，在发送完rpc后还需要负责接收并处理对端发送回来的响应。
 *        5.AppendEntries:接收leader发来的日志请求，主要检验用于检查当前日志是否匹配并同步leader的日志到本机。
 *        6.InstallSnapshot:接收leader发来的快照请求，同步快照到本机。
 */
void Raft::leaderHearBeatTicker() {
    while (true) {
        // 不是leader的话就没有必要进行后续操作，况且还要拿锁，很影响性能。
        while (m_status != Leader) {
            usleep(1000 * HeartBeatTimeout);
        }
        static std::atomic<int32_t> atomicCount = 0;

        std::chrono::duration<signed long int, std::ratio<1, 1000000000>> suitableSleepTime{};
        std::chrono::system_clock::time_point wakeTime{};
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            wakeTime = now();
            suitableSleepTime = std::chrono::milliseconds(HeartBeatTimeout) + m_lastResetHearBeatTime - wakeTime;
        }

        // sleep
        if (std::chrono::duration<double, std::milli>(suitableSleepTime).count() > 1) {
            usleep(std::chrono::duration_cast<std::chrono::microseconds>(suitableSleepTime).count());
        }

        if (std::chrono::duration<double, std::milli>(m_lastResetHearBeatTime - wakeTime).count() > 0) {
            // 睡眠的这段时间有重置定时器，没有超时，再次睡眠
            continue;
        }
        // 执行心跳
        doHeartBeat();
    }
}

/**
 * @brief 发起心跳，只有leader才需要发起心跳
 *        每隔一段时间检查睡眠时间内有没有重置定时器，没有则说明超时了
 *        如果有则设置合适睡眠时间：睡眠到重置时间+超时时间
 */
void Raft::doHeartBeat() {
    std::lock_guard<std::mutex> g(m_mtx);
    if (m_status == Leader) {
        auto appendNums = std::make_shared<int>(1);  // 正确返回的节点的数量
        // 对Follower发送心跳
        // 最少要单独写一个函数来管理，而不是在这一坨
        for (int i = 0; i < m_peers.size(); i++) {
            if (i == m_me) {
                continue;
            }
            myAssert(m_nextIndex[i] >= 1, format("rf.nextIndex[%d] = {%d}", i, m_nextIndex[i]));
            // 日志压缩加入后要判断是发送快照还是发送心跳
            if (m_nextIndex[i] <= m_lastSnapshotIncludeIndex) {
                std::thread t(&Raft::leaderSendSnapShot, this, i);  // 创建新线程并执行b函数，并传递参数
                t.detach();
                continue;
            }

            // 心跳
            // 构造发送值
            int preLogIndex = -1;
            int PrevLogTerm = -1;
            //获取本次发送的一系列日志的上一条日志的信息，以判断是否匹配
            getPrevLogInfo(i, &preLogIndex, &PrevLogTerm);
            std::shared_ptr<raftRpcProctoc::AppendEntriesArgs> appendEntriesArgs =
                std::make_shared<raftRpcProctoc::AppendEntriesArgs>();
            appendEntriesArgs->set_term(m_currentTerm);
            appendEntriesArgs->set_leaderid(m_me);
            appendEntriesArgs->set_prevlogindex(preLogIndex);
            appendEntriesArgs->set_prevlogterm(PrevLogTerm);
            appendEntriesArgs->clear_entries();
            appendEntriesArgs->set_leadercommit(m_commitIndex);

            // 作用是携带上prelogIndex的下一条日志及其之后的所有日志
            // leader对每个节点发送的日志长短不一，但是都保证从prevIndex发送直到最后
            if (preLogIndex != m_lastSnapshotIncludeIndex) {
                for (int j = getSlicesIndexFromLogIndex(preLogIndex) + 1; j < m_logs.size(); ++j) {
                    raftRpcProctoc::LogEntry* sendEntryPtr = appendEntriesArgs->add_entries();
                    *sendEntryPtr = m_logs[j];  //=是可以点进去的，可以点进去看下protobuf如何重写这个的
                }
            } else {
                for (const auto& item : m_logs) {
                    raftRpcProctoc::LogEntry* sendEntryPtr = appendEntriesArgs->add_entries();
                    *sendEntryPtr = item;  //=是可以点进去的，可以点进去看下protobuf如何重写这个的
                }
            }
            int lastLogIndex = getLastLogIndex();
            // leader对每个节点发送的日志长短不一，但是都保证从prevIndex发送直到最后
            myAssert(appendEntriesArgs->prevlogindex() + appendEntriesArgs->entries_size() == lastLogIndex,
                     format("appendEntriesArgs.PrevLogIndex{%d}+len(appendEntriesArgs.Entries){%d} != lastLogIndex{%d}",
                            appendEntriesArgs->prevlogindex(), appendEntriesArgs->entries_size(), lastLogIndex));
            // 构造返回值
            const std::shared_ptr<raftRpcProctoc::AppendEntriesReply> appendEntriesReply =
                std::make_shared<raftRpcProctoc::AppendEntriesReply>();
            appendEntriesReply->set_appstate(Disconnected);

            std::thread t(&Raft::sendAppendEntries, this, i, appendEntriesArgs, appendEntriesReply,
                          appendNums);  // 创建新线程并执行b函数，并传递参数
            t.detach();
        }

        // leader发送心跳，重置心跳时间，
        // 与选举不同的是 m_lastResetHearBeatTime 是一个固定的时间，而选举超时时间是一定范围内的随机值。
        // 这个的具体原因是为了避免很多节点一起发起选举而导致一直选不出leader 的情况。
        // 为何选择随机时间而不选择其他的解决冲突的方法具体可见raft论文。
        m_lastResetHearBeatTime = now();  
    }
}
```