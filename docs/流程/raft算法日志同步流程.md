# raft日志同步流程

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
 *        对每个发送的 raft 节点，都起一个线程发送，调用 Raft::sendAppendEntries()
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
            myAssert(m_nextIndex[i] >= 1, format(""));
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
            myAssert(appendEntriesArgs->prevlogindex() + appendEntriesArgs->entries_size() == lastLogIndex, format(""));
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

/**
 * @brief Leader真正发送心跳的函数，调用其他raft节点的RPC方法 Raft::AppendEntries
 * @param server 回复的结点
 * @param args 回复的参数
 * @param reply 回复的响应
 * @param appendNums 记录回复的结点数量
 * @param return 返回是否成功
 */
bool Raft::sendAppendEntries(int server, std::shared_ptr<raftRpcProctoc::AppendEntriesArgs> args,
                             std::shared_ptr<raftRpcProctoc::AppendEntriesReply> reply,
                             std::shared_ptr<int> appendNums) {
    // 如果append失败应该不断的retries ,直到这个log成功的被store
    bool ok = m_peers[server]->AppendEntries(args.get(), reply.get()); // 发送rpc请求
    if (!ok) {
        return ok;
    }
    if (reply->appstate() == Disconnected) {
        return ok;
    }

    std::lock_guard<std::mutex> lg1(m_mtx);
    // 对reply进行处理
    if (reply->term() > m_currentTerm) { // 自己已经过时了
        m_status = Follower;
        m_currentTerm = reply->term();
        m_votedFor = -1;
        return ok;
    } else if (reply->term() < m_currentTerm) {
        return ok;
    }

    if (m_status != Leader) { // 如果不是leader，那么就不要对返回的情况进行处理了
        return ok; 
    }

    // term相等
    myAssert(reply->term() == m_currentTerm, format(""));
    if (!reply->success()) {
        // 日志不匹配，正常来说就是index要往前-1，既然能到这里，第一个日志（idnex =
        // 1）发送后肯定是匹配的，因此不用考虑变成负数 因为真正的环境不会知道是服务器宕机还是发生网络分区了
        if (reply->updatenextindex() != -100) { ////-100只是一个特殊标记而已，没有太具体的含义
            //// 优化日志匹配，让follower决定到底应该下一次从哪一个开始尝试发送
            m_nextIndex[server] = reply->updatenextindex();  // 失败不更新mathIndex
        }
    } else { //到这里代表同意接收了本次心跳或者日志
        *appendNums = *appendNums + 1; 
        //同意了日志，就更新对应的 m_matchIndex 和 m_nextIndex
        m_matchIndex[server] = std::max(m_matchIndex[server], args->prevlogindex() + args->entries_size());
        m_nextIndex[server] = m_matchIndex[server] + 1;
        int lastLogIndex = getLastLogIndex();
        myAssert(m_nextIndex[server] <= lastLogIndex + 1, format("",));
        if (*appendNums >= 1 + m_peers.size() / 2) { // 可以commit了
            *appendNums = 0;
            // leader只有在当前term有日志提交的时候才更新commitIndex，因为raft无法保证之前term的Index是否提交
            // 只有当前term有日志提交，之前term的log才可以被提交，只有这样才能保证“领导人完备性{当选领导人的节点拥有之前被提交的所有log，当然也可能有一些没有被提交的}”
            if (args->entries_size() > 0) {
            }
            if (args->entries_size() > 0 && args->entries(args->entries_size() - 1).logterm() == m_currentTerm) {
                m_commitIndex = std::max(m_commitIndex, args->prevlogindex() + args->entries_size());
            }
            myAssert(m_commitIndex <= lastLogIndex, format(""));
        }
    }
    return ok;
}

/**
 * @brief 重写基类方法, 远程 follower 节点远程被调用
 */
void Raft::AppendEntries(google::protobuf::RpcController* controller,
                         const ::raftRpcProctoc::AppendEntriesArgs* request,
                         ::raftRpcProctoc::AppendEntriesReply* response, ::google::protobuf::Closure* done) {
    AppendEntries1(request, response);
    done->Run();
}

/**
 * @brief 日志同步 + 心跳 rpc ，重点关注。follow 节点执行的操作
 * @param args 接收的rpc参数
 * @param reply 回复的rpc参数
 */
void Raft::AppendEntries1(const raftRpcProctoc::AppendEntriesArgs* args,                            raftRpcProctoc::AppendEntriesReply* reply) {
    std::lock_guard<std::mutex> locker(m_mtx);
    reply->set_appstate(AppNormal);  // 能接收到代表网络是正常的

    // 不同的人收到AppendEntries的反应是不同的，要注意无论什么时候收到rpc请求和响应都要检查term
    if (args->term() < m_currentTerm) {
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(-100);
        return;  // 注意从过期的领导人收到消息不要重设定时器
    }
    // 执行persist的时候也是拿到锁的
    DEFER { persist(); };  
    if (args->term() > m_currentTerm) { // 三变,防止遗漏，无论什么时候都是三变
        m_status = Follower;
        m_currentTerm = args->term();
        m_votedFor = -1;  // 这里设置成-1有意义，如果突然宕机然后上线理论上是可以投票的
    }
    myAssert(args->term() == m_currentTerm, format(""));
    m_status = Follower;  // 这里是有必要的，因为如果candidate收到同一个term的leader的AE，需要变成follower
    m_lastResetElectionTime = now();
    
    // 不能无脑的从prevlogIndex开始阶段日志，因为rpc可能会延迟，导致发过来的log是很久之前的
    // 那么就比较日志，日志有3种情况
    if (args->prevlogindex() > getLastLogIndex()) { // 本地日志落后
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(getLastLogIndex() + 1);
        return;
    } else if (args->prevlogindex() < m_lastSnapshotIncludeIndex) { // leader 中记录的 prevlogIndex 没跟上本地快照
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        // 如果想直接弄到最新不对，因为是从后慢慢往前匹配的，这里不匹配说明后面的都不匹配
        reply->set_updatenextindex(m_lastSnapshotIncludeIndex + 1);  
    }
    
    if (matchLog(args->prevlogindex(), args->prevlogterm())) { // 日志匹配，复制
        for (int i = 0; i < args->entries_size(); i++) {
            auto log = args->entries(i);
            if (log.logindex() > getLastLogIndex()) { // 超过就直接添加日志
                m_logs.push_back(log);
            } else { // 没超过就比较是否匹配，不匹配再更新，而不是直接截断
                if (m_logs[getSlicesIndexFromLogIndex(log.logindex())].logterm() == log.logterm() &&
                    m_logs[getSlicesIndexFromLogIndex(log.logindex())].command() != log.command()) {
                    // 相同位置的log，其logTerm相等，但是命令却不相同，不符合raft的前向匹配，异常了！
                    myAssert(false, format("",));
                }
                if (m_logs[getSlicesIndexFromLogIndex(log.logindex())].logterm() != log.logterm()) { // 不匹配就覆盖更新
                    m_logs[getSlicesIndexFromLogIndex(log.logindex())] = log;
                }
            }
        }

        // 因为可能会收到过期的log！！！ 因此这里是大于等于
        myAssert(getLastLogIndex() >= args->prevlogindex() + args->entries_size(),format(""));
        if (args->leadercommit() > m_commitIndex) { // 因为可能存在args->leadercommit()落后于 getLastLogIndex()的情况
            m_commitIndex = std::min(args->leadercommit(), getLastLogIndex());
        }
        myAssert(getLastLogIndex() >= m_commitIndex, format(""));
        reply->set_success(true);
        reply->set_term(m_currentTerm);
        return;
    } else { // 不匹配
        // 优化，减少rpc
        reply->set_updatenextindex(args->prevlogindex());
        for (int index = args->prevlogindex(); index >= m_lastSnapshotIncludeIndex; --index) {
            if (getLogTermFromLogIndex(index) != getLogTermFromLogIndex(args->prevlogindex())) {
                reply->set_updatenextindex(index + 1);
                break;
            }
        }
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        return;
    }
}
```
