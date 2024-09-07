#include "raft.h"
#include <atomic>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <memory>
#include "common/config.h"
#include "common/util.h"

Raft::Raft(sylar::IOManager::ptr _iom)
    : m_iom(std::move(_iom)),
      m_pool(calculate_pool_size()),
      m_lastSnapshotIncludeIndex(),
      m_me(),
      m_commitIndex(),
      m_lastApplied(),
      m_lastSnapshotIncludeTerm(),
      m_currentTerm(),
      m_votedFor(),
      m_status()
{
}
Raft::~Raft() { std::cout << "----------[Raft::~Raft()]---------- \n"; }

/**
 * @brief 服务提供方，日志同步rpc，内部的真正实现
 * @param args 接收的rpc参数
 * @param reply 回复的rpc参数
 */
void Raft::AppendEntries(const raftRpcProctoc::AppendEntriesArgs* args, raftRpcProctoc::AppendEntriesReply* reply) {
    std::lock_guard<std::mutex> lock(m_mtx);
    reply->set_appstate(AppNormal);  // 能接收到代表网络是正常的

    // 不同的人收到 AppendEntries rpc 的反应是不同的，要注意无论什么时候收到rpc请求和响应都要检查term
    if (args->term() < m_currentTerm) {  // 对方落后
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(-100);
        DPrintf("[func-AppendEntries-rf{%d}] 拒绝了 因为Leader{%d}的term{%v}< rf{%d}.term{%d}\n", m_me,
                args->leaderid(), args->term(), m_me, m_currentTerm);
        return;  // 注意从过期的领导人收到消息不要重设定时器
    }
    DEFER{ persist(); };

    if (args->term() > m_currentTerm) {  // 当前落后 可以往下走 ---------------------------------------
        m_status = Follower;
        m_currentTerm = args->term();
        m_votedFor = -1;  
    }
    myAssert(args->term() == m_currentTerm, format("assert {args.Term == rf.currentTerm} fail"));
    m_status = Follower;
    m_lastResetElectionTime = now();  // 重置定时器

    // 比较日志，日志有3种情况
    if (args->prevlogindex() > getLastLogIndex()) {  // 本地日志落后，与发过来的不匹配，就要返回需要接收的idx
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(getLastLogIndex() + 1);
        return;
    } else if (args->prevlogindex() < m_lastSnapshotIncludeIndex) {  // leader 中记录的 prevlogIndex 没跟上本地快照
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(m_lastSnapshotIncludeIndex + 1);
    }

    // -------- args->prevlogindex() <= getLastLogIndex() 就是已经有了这个idx的日志项 --------
    if (matchLog(args->prevlogindex(), args->prevlogterm())) {  // 任期匹配就可以直接进行复制
        for (int i = 0; i < args->entries_size(); i++) {
            const ::raftRpcProctoc::LogEntry& log = args->entries(i);
            if (log.logindex() > getLastLogIndex()) {  // 超过就直接添加日志
                m_logs.push_back(log);
            } else {  // 覆盖
                if (m_logs[getSlicesIndexFromLogIndex(log.logindex())].logterm() == log.logterm() &&
                    m_logs[getSlicesIndexFromLogIndex(log.logindex())].command() != log.command()) {
                    // 相同位置的log，其logTerm相等，但是命令却不相同，不符合raft的前向匹配，异常了！
                    myAssert(
                        false,
                        format("[func-AppendEntries-rf{%d}] 两节点logIndex{%d}和term{%d}相同，但是其command{%d:%d}"
                               " {%d:%d}却不同！！\n",
                               m_me, log.logindex(), log.logterm(), m_me,
                               m_logs[getSlicesIndexFromLogIndex(log.logindex())].command(), args->leaderid(),
                               log.command()));
                }
                if (m_logs[getSlicesIndexFromLogIndex(log.logindex())].logterm() != log.logterm()) {  // 覆盖
                    m_logs[getSlicesIndexFromLogIndex(log.logindex())] = log;
                }
            }
        }

        // 因为可能会收到过期的log！！！ 因此这里是大于等于
        myAssert(
            getLastLogIndex() >= args->prevlogindex() + args->entries_size(),
            format("[func-AppendEntries-rf{%d}]rf.getLastLogIndex(){%d} != args.PrevLogIndex{%d}+len(args.Entries){%d}",
                   m_me, getLastLogIndex(), args->prevlogindex(), args->entries_size()));
        if (args->leadercommit() > m_commitIndex) {  // 因为可能存在args->leadercommit()落后于 getLastLogIndex()的情况
            m_commitIndex = std::min(args->leadercommit(), getLastLogIndex());
        }
        myAssert(getLastLogIndex() >= m_commitIndex,
                 format("[func-AppendEntries-rf{%d}]  rf.getLastLogIndex{%d} < rf.commitIndex{%d}", m_me,
                        getLastLogIndex(), m_commitIndex));
        reply->set_success(true);
        reply->set_term(m_currentTerm);
        return;
    } else {  // --------------------- 日志项存在，但是任期不匹配 ---------------------
        // 优化，减少rpc
        reply->set_updatenextindex(args->prevlogindex());
        int idx = args->prevlogindex();
        while (idx >= m_commitIndex && getLogTermFromLogIndex(idx) == getLogTermFromLogIndex(args->prevlogindex())) {
            --idx;
        }
        reply->set_updatenextindex(idx + 1);
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        return;
    }
}

/**
 * @brief Leader真正发送心跳的函数，调用RPC
 * @param server 回复的结点
 * @param args 回复的参数
 * @param reply 回复的响应
 * @param appendNums 记录回复的结点数量
 * @param return 返回是否成功
 */
bool Raft::sendAppendEntries(int server, const std::shared_ptr<raftRpcProctoc::AppendEntriesArgs>& args,
                             const std::shared_ptr<raftRpcProctoc::AppendEntriesReply>& reply,
                             const std::shared_ptr<std::atomic_int32_t>& appendNums) {
    DPrintf("[func-Raft::sendAppendEntries-raft{%d}] leader --> {%d} AE rpc start，args->entries_size():{%d}", m_me,
            server, args->entries_size());
    bool ok =
        m_peers[server]->AppendEntries(args.get(), reply.get());  // rpc 调用，这里本节点相当于客户端，调对端rpc方法
    if (!ok) {                                                    // rpc 调用失败
        DPrintf("[func-Raft::sendAppendEntries-raft{%d}] leader --> {%d} AE rpc file", m_me, server);
        return ok;
    }
    DPrintf("[func-Raft::sendAppendEntries-raft{%d}] leader --> {%d} AE rpc success", m_me, server);
    if (reply->appstate() == Disconnected) {
        return ok;
    }

    std::lock_guard<std::mutex> lock(m_mtx);
    // 对reply进行处理
    if (reply->term() > m_currentTerm) {  // 自己已经过时了 或者集群要进入下一个term周期，这里可以思考一下节点网络分区的情况
        m_status = Follower;
        m_currentTerm = reply->term();
        m_votedFor = -1;
        return ok;
    } else if (reply->term() < m_currentTerm) {  // 对端任期比较老，接到心跳包之后会--> 
                                                 // 先同步term，再同步日志，其实这种情况在本项目中不好模拟，但是要考虑到
        DPrintf("[func -sendAppendEntries rf{%d}] node:{%d} term{%d}<rf{%d}term{%d}\n", m_me, server, reply->term(),
                m_me, m_currentTerm);
        return ok;
    }

    if (m_status != Leader) {  // 如果不是leader，那么就不要对返回的情况进行处理了
        return ok;
    }

    // ----------------------- term相等 ----------------------- //
    myAssert(reply->term() == m_currentTerm,
             format("reply.Term{%d} != rf.currentTerm{%d}   ", reply->term(), m_currentTerm));
    if (!reply->success()) {  // 同一个任期，日志不匹配，说明对方的日志有错的，向前匹配
        if (reply->updatenextindex() != -100) {  //-100只是一个特殊标记而已，没有太具体的含义
            DPrintf("[func -sendAppendEntries rf{%d}] 返回的日志term相等，但是不匹配，回缩nextIndex[%d]:{%d}\n", m_me,
                    server, reply->updatenextindex());
            m_nextIndex[server] = reply->updatenextindex();  // 失败不更新mathIndex，但是要更新下次开始发送的索引
        }
    } else {
        /// 到这里代表 在同一个任期内 且 同意接收了本次心跳或者日志
        *appendNums = *appendNums + 1;
        // 同意了日志，就更新对应的 m_matchIndex 和 m_nextIndex
        m_matchIndex[server] = std::max(m_matchIndex[server], args->prevlogindex() + args->entries_size());
        m_nextIndex[server] = m_matchIndex[server] + 1;
        int lastLogIndex = getLastLogIndex();
        myAssert(m_nextIndex[server] <= lastLogIndex + 1,
                 format("error msg:rf.nextIndex[%d] > lastLogIndex+1, len(rf.logs) = %d lastLogIndex{%d} = %d", server,
                        m_logs.size(), server, lastLogIndex));
        if (*appendNums >= 1 + m_peers.size() / 2) {  // 已经复制到大多数节点上了，可以commit了
            *appendNums = 0;
            if (args->entries_size() > 0) {
                DPrintf("args->entries(args->entries_size()-1).logterm(){%d} m_currentTerm{%d}",
                        args->entries(args->entries_size() - 1).logterm(), m_currentTerm);
            }

            /// leader只有在当前term有日志提交的时候才更新commitIndex，
            /// 因为raft无法保证之前term的Index是否提交只有当前term有日志提交，之前term的log才可以被 顺带提交，
            /// 只有这样才能保证“领导人完备性{当选领导人的节点拥有之前被提交的所有log，当然也可能有一些没有被提交的}”
            if (args->entries_size() > 0 && args->entries(args->entries_size() - 1).logterm() == m_currentTerm) {
                DPrintf(
                    "当前term有log成功提交，更新leader的m_commitIndex "
                    "from{%d} to{%d}",
                    m_commitIndex, args->prevlogindex() + args->entries_size());

                m_commitIndex = std::max(m_commitIndex, args->prevlogindex() + args->entries_size());
            }
            myAssert(m_commitIndex <= lastLogIndex,
                     format("[func-sendAppendEntries,rf{%d}] lastLogIndex:%d rf.commitIndex:%d\n", m_me, lastLogIndex,
                            m_commitIndex));
        }
    }
    return ok;
}

void Raft::applierTicker() {
    while (true) {
        std::vector<ApplyMsg> applyMsgs;
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_status == Leader) {
                DPrintf("[Raft::applierTicker() - raft{%d}] m_lastApplied{%d} m_commitIndex{%d}", m_me, m_lastApplied,
                        m_commitIndex);
            }
            applyMsgs = getApplyLogs();
        }
        if (!applyMsgs.empty()) {
            DPrintf("[func- Raft::applierTicker()-raft{%d}] 向kvserver报告的applyMsgs长度为：{%d}", m_me,
                    applyMsgs.size());
        }
        for (auto& message : applyMsgs) {
            m_applyChan->Push(message);
        }
        // 检查间隔 ApplyInterval 毫秒
        sleepNMilliseconds(ApplyInterval);
    }
}

bool Raft::CondInstallSnapshot(int lastIncludedTerm, int lastIncludedIndex, const std::string& snapshot) {
    return true;
}

/**
 * @brief 实际发起选举，构造需要发送的rpc,调用sendRequestVote处理rpc及其相应。
 */
void Raft::doElection() {
    std::lock_guard<std::mutex> lock(m_mtx);  // 上锁
    if (m_status == Leader) {
    }

    if (m_status != Leader) {
        DPrintf("[ ticker-func-rf(%d) ]  选举定时器到期且不是leader，开始选举 \n", m_me);
        // 重竞选超时，term也会增加的

        m_status = Candidate;  // 设置状态为candidate
        m_currentTerm += 1;    // 开始新一轮的选举
        m_votedFor = m_me;     // 即是自己给自己投，也避免candidate给同辈的candidate投

        /// Raft算法中的选举机制要求Candidate节点在发起选举请求时，附带其日志的最后一条日志条目的索引和任期号。
        /// 这要求Candidate节点在发起选举前，必须确保其日志是最新的，并且已经被持久化。
        persist();
        std::shared_ptr<int> votedNum = std::make_shared<int>(1);
        m_lastResetElectionTime = now();  // 重新设置选举超时定时器

        // 发送投票 RPC
        for (int i = 0; i < m_peers.size(); i++) {
            if (i == m_me) {
                continue;
            }
            int lastLogIndex = -1, lastLogTerm = -1;
            getLastLogIndexAndTerm(&lastLogIndex, &lastLogTerm);  // 获取上一个log的term和下标

            // 构造RPC请求
            std::shared_ptr<raftRpcProctoc::RequestVoteArgs> requestVoteArgs =
                std::make_shared<raftRpcProctoc::RequestVoteArgs>();
            requestVoteArgs->set_term(m_currentTerm);
            requestVoteArgs->set_candidateid(m_me);
            requestVoteArgs->set_lastlogindex(lastLogIndex);
            requestVoteArgs->set_lastlogterm(lastLogTerm);
            auto requestVoteReply = std::make_shared<raftRpcProctoc::RequestVoteReply>();

            // 在for循环中 --> 候选者可能会起 (n - 1) 个线程发送投票RPC
            m_pool.commit_with_timeout(
                [task = std::bind(&Raft::sendRequestVote, this, i, requestVoteArgs, requestVoteReply, votedNum)]() {
                    task();  // 忽略返回值
                },
                maxRandomizedElectionTime);
        }
    }
}

/**
 * @brief 发起心跳，只有leader才需要发起心跳
 */
void Raft::doHeartBeat() {
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_status == Leader) {
        DPrintf("[func-Raft::doHeartBeat()-Leader: {%d}] Leader的心跳定时器触发了且拿到mutex，开始发送AE\n", m_me);
        auto appendNums = std::make_shared<std::atomic_int32_t>(1);  // 正确返回的节点的数量
        // 对Follower发送心跳
        // TODO 更优雅的实现
        // n - 1 个心跳发送任务
        for (int i = 0; i < m_peers.size(); i++) {
            if (i == m_me) {
                continue;
            }
            // DPrintf("[func-Raft::doHeartBeat()-Leader: {%d}] Leader的心跳定时器触发了 index:{%d}\n", m_me, i);
            myAssert(m_nextIndex[i] >= 1, format("rf.nextIndex[%d] = {%d}", i, m_nextIndex[i]));

            // ----------------------------- 发快照心跳 ----------------------------- //
            if (m_nextIndex[i] <= m_lastSnapshotIncludeIndex) {
                m_pool.commit_with_timeout(std::bind(&Raft::leaderSendSnapShot, this, i), HeartBeatTimeout);
                continue;
            }

            // ----------------------------- 发增量心跳 ----------------------------- //
            // 构造发送值
            int preLogIndex = -1;
            int PrevLogTerm = -1;
            // 获取本次发送的一系列日志的上一条日志的信息
            getPrevLogInfo(i, &preLogIndex, &PrevLogTerm);
            std::shared_ptr<raftRpcProctoc::AppendEntriesArgs> appendEntriesArgs =
                std::make_shared<raftRpcProctoc::AppendEntriesArgs>();

            // 添加双方用来判断日志的信息
            appendEntriesArgs->set_term(m_currentTerm);
            appendEntriesArgs->set_leaderid(m_me);
            appendEntriesArgs->set_prevlogindex(preLogIndex);
            appendEntriesArgs->set_prevlogterm(PrevLogTerm);
            appendEntriesArgs->clear_entries();
            appendEntriesArgs->set_leadercommit(m_commitIndex);

            // 携带对方空缺的日志
            if (preLogIndex != m_lastSnapshotIncludeIndex) {
                for (int j = getSlicesIndexFromLogIndex(preLogIndex) + 1; j < m_logs.size(); ++j) {
                    raftRpcProctoc::LogEntry* sendEntryPtr = appendEntriesArgs->add_entries();
                    *sendEntryPtr = m_logs[j];
                }
            } else {  // 对方在上一次拿到快照之后就没收过心跳，那就把当前的日志都发过去
                for (const auto& item : m_logs) {
                    raftRpcProctoc::LogEntry* sendEntryPtr = appendEntriesArgs->add_entries();
                    *sendEntryPtr = item;
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

            // 提交发送任务到线程池
            m_pool.commit_with_timeout([task = std::bind(&Raft::sendAppendEntries, this, i, appendEntriesArgs,
                                                         appendEntriesReply, appendNums)]() { task(); },
                                       HeartBeatTimeout);
        }

        // leader发送心跳，重置心跳时间，与选举不同
        m_lastResetHearBeatTime = now();
    }
}

/**
 * @brief 负责查看是否该发起选举，如果该发起选举就执行doElection发起选举。
 *        注意，只有follower状态的节点才会执行这个函数。而leader只负责sleep什么也不做
 */
void Raft::electionTimeOutTicker() {
    while (true) {
        while (m_status == Leader) {
            usleep(HeartBeatTimeout * 1000);
        }

        // 计算距离上次重置选举计时器的时间 + 随机的选举超时时间，然后根据这个时间决定是否睡眠
        std::chrono::duration<signed long int, std::ratio<1, 1000000000>> suitableSleepTime{};
        std::chrono::system_clock::time_point wakeTime{};
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            wakeTime = now();
            suitableSleepTime = getRandomizedElectionTimeout() + m_lastResetElectionTime - wakeTime;
        }

        // 若超时时间未到，进入睡眠状态 .count()返回毫秒数
        if (std::chrono::duration<double, std::milli>(suitableSleepTime).count() > 1) {
            auto start = std::chrono::steady_clock::now();
            usleep(std::chrono::duration_cast<std::chrono::microseconds>(suitableSleepTime).count());
            auto end = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            // 使用ANSI控制序列将输出颜色修改为紫色
            std::cout << "\033[1;35m electionTimeOutTicker();函数设置睡眠时间为: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(suitableSleepTime).count()
                      << " 毫秒\033[0m" << std::endl;
            std::cout << "\033[1;35m electionTimeOutTicker();函数实际睡眠时间为: " << duration.count() << " 毫秒\033[0m"
                      << std::endl;
        }

        // 说明睡眠的这段时间有重置定时器，那么就没有超时，再次睡眠
        if (std::chrono::duration<double, std::milli>(m_lastResetElectionTime - wakeTime).count() > 0) {
            continue;
        }
        // 若超时时间已到，调用doElection() 函数启动领导者选举过程。
        doElection();
    }
}

/**
 * @brief 返回待apply的日志信息
 */
std::vector<ApplyMsg> Raft::getApplyLogs() {
    std::vector<ApplyMsg> applyMsgs;
    myAssert(m_commitIndex <= getLastLogIndex(),
             format("[func-getApplyLogs-rf{%d}] commitIndex{%d} >getLastLogIndex{%d}", m_me, m_commitIndex,
                    getLastLogIndex()));

    while (m_lastApplied < m_commitIndex) {
        m_lastApplied++;
        myAssert(m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].logindex() == m_lastApplied,
                 format("rf.logs[rf.getSlicesIndexFromLogIndex(rf.lastApplied)].LogIndex{%d} != rf.lastApplied{%d} ",
                        m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].logindex(), m_lastApplied));
        ApplyMsg applyMsg;
        applyMsg.CommandValid = true;
        applyMsg.SnapshotValid = false;
        applyMsg.Command = m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].command();
        applyMsg.CommandIndex = m_lastApplied;
        applyMsgs.emplace_back(applyMsg);
    }
    return applyMsgs;
}

// 获取新命令应该分配的Index
int Raft::getNewCommandIndex() {
    //	如果len(logs)==0,就为快照的index+1，否则为log最后一个日志+1
    auto lastLogIndex = getLastLogIndex();
    return lastLogIndex + 1;
}

// getPrevLogInfo
// leader调用，传入：服务器index，传出：发送的AE的preLogIndex和PrevLogTerm
void Raft::getPrevLogInfo(int server, int* preIndex, int* preTerm) {
    // logs长度为0返回0,0，不是0就根据nextIndex数组的数值返回
    if (m_nextIndex[server] == m_lastSnapshotIncludeIndex + 1) {
        // 要发送的日志是第一个日志，因此直接返回m_lastSnapshotIncludeIndex和m_lastSnapshotIncludeTerm
        *preIndex = m_lastSnapshotIncludeIndex;
        *preTerm = m_lastSnapshotIncludeTerm;
        return;
    }
    auto nextIndex = m_nextIndex[server];
    *preIndex = nextIndex - 1;
    *preTerm = m_logs[getSlicesIndexFromLogIndex(*preIndex)].logterm();
}

// GetState return currentTerm and whether this server
// believes it is the Leader.
bool Raft::GetState(int* term) {
    // todo 暂时不清楚会不会导致死锁
    std::lock_guard<std::mutex> lock(m_mtx);
    *term = m_currentTerm;
    return m_status == Raft::Status::Leader;
}

void Raft::InstallSnapshot(const raftRpcProctoc::InstallSnapshotRequest* args,
                           raftRpcProctoc::InstallSnapshotResponse* reply) {
    std::lock_guard<std::mutex> lock(m_mtx);
    if (args->term() < m_currentTerm) {
        reply->set_term(m_currentTerm);
        return;
    }

    // 后面两种情况都要接收日志
    if (args->term() > m_currentTerm) {
        m_currentTerm = args->term();
        m_votedFor = -1;
        m_status = Follower;
        persist();
    }
    m_status = Follower;
    m_lastResetElectionTime = now();  // 重置超时选举定时器

    // outdated snapshot
    if (args->lastsnapshotincludeindex() <= m_lastSnapshotIncludeIndex) {
        return;
    }

    int lastLogIndex = getLastLogIndex();

    // 当前日志比较多，就把有的删掉，留下还没确认commit的
    if (lastLogIndex > args->lastsnapshotincludeindex()) {
        m_logs.erase(m_logs.begin(), m_logs.begin() + getSlicesIndexFromLogIndex(args->lastsnapshotincludeindex()) + 1);
    } else {  // 当前日志比较少，那直接清空就行了，把快照接好，传给server
        m_logs.clear();
    }
    m_commitIndex = std::max(m_commitIndex, args->lastsnapshotincludeindex());
    m_lastApplied = std::max(m_lastApplied, args->lastsnapshotincludeindex());
    m_lastSnapshotIncludeIndex = args->lastsnapshotincludeindex();
    m_lastSnapshotIncludeTerm = args->lastsnapshotincludeterm();

    reply->set_term(m_currentTerm);
    ApplyMsg msg;
    msg.SnapshotValid = true;
    msg.Snapshot = args->data();
    msg.SnapshotTerm = args->lastsnapshotincludeterm();
    msg.SnapshotIndex = args->lastsnapshotincludeindex();

    m_applyChan->Push(msg);
    m_persister->Save(persistData(), args->data());  // 持久化一下状态，并保存
}

/**
 * @brief 检查是否需要发起心跳（leader）如果该发起就执行doHeartBeat。
 *        注意:只有leader才可以发起心跳。follower在这个函数中什么也不做。
 */
void Raft::leaderHearBeatTicker() {
    while (true) {
        // follower和candidate在这里循环睡觉
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
            std::cout << atomicCount << "\033[1;35m leaderHearBeatTicker();函数设置睡眠时间为: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(suitableSleepTime).count()
                      << " 毫秒\033[0m" << std::endl;
            auto start = std::chrono::steady_clock::now();

            // 真正的心跳包发送间隔
            usleep(std::chrono::duration_cast<std::chrono::microseconds>(suitableSleepTime).count());
            auto end = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            // 使用ANSI控制序列将输出颜色修改为紫色
            std::cout << atomicCount << "\033[1;35m leaderHearBeatTicker();函数实际睡眠时间为: " << duration.count()
                      << " 毫秒\033[0m" << std::endl;
            ++atomicCount;
        }

        if (std::chrono::duration<double, std::milli>(m_lastResetHearBeatTime - wakeTime).count() > 0) {
            // 睡眠的这段时间有重置定时器，没有超时，再次睡眠
            continue;
        }

        doHeartBeat();
    }
}

void Raft::leaderSendSnapShot(int server) {
    raftRpcProctoc::InstallSnapshotRequest args;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        args.set_leaderid(m_me);
        args.set_term(m_currentTerm);
        args.set_lastsnapshotincludeindex(m_lastSnapshotIncludeIndex);
        args.set_lastsnapshotincludeterm(m_lastSnapshotIncludeTerm);
        args.set_data(m_persister->ReadSnapshot());
    }
    raftRpcProctoc::InstallSnapshotResponse reply;

    // 远程调用
    bool ok = m_peers[server]->InstallSnapshot(&args, &reply);
    std::lock_guard<std::mutex> lock(m_mtx);
    if (!ok) {
        return;
    }
    if (m_status != Leader || m_currentTerm != args.term()) {
        return;  // 状态已经改变了
    }
    //	无论什么时候都要判断term
    if (reply.term() > m_currentTerm) {
        // 三变
        m_currentTerm = reply.term();
        m_votedFor = -1;
        m_status = Follower;
        persist();
        m_lastResetElectionTime = now();
        return;
    }
    m_matchIndex[server] = args.lastsnapshotincludeindex();
    m_nextIndex[server] = m_matchIndex[server] + 1;
}

void Raft::leaderUpdateCommitIndex() {
    m_commitIndex = m_lastSnapshotIncludeIndex;
    for (int index = getLastLogIndex(); index >= m_lastSnapshotIncludeIndex + 1; index--) {
        int sum = 0;
        for (int i = 0; i < m_peers.size(); i++) {
            if (i == m_me) {
                sum += 1;
                continue;
            }
            if (m_matchIndex[i] >= index) {
                sum += 1;
            }
        }

        // !!!只有当前term有新提交的，才会更新commitIndex！！！！
        if (sum >= m_peers.size() / 2 + 1 && getLogTermFromLogIndex(index) == m_currentTerm) {
            m_commitIndex = index;
            break;
        }
    }
}

/**
 * @brief leader传过来的对应Index的日志是否匹配，只需要Index和Term就可以知道是否匹配
 *        进来前要保证logIndex是存在的，即 ≥rf.lastSnapshotIncludeIndex，而且小于等于rf.getLastLogIndex()
 * @param logIndex leader 传过来的日志的索引
 * @param logTerm leader 传过来的日志的Term
 */
bool Raft::matchLog(int logIndex, int logTerm) {
    myAssert(logIndex >= m_lastSnapshotIncludeIndex && logIndex <= getLastLogIndex(),
             format("不满足：logIndex{%d}>=rf.lastSnapshotIncludeIndex{%d}&&logIndex{%d}<=rf.getLastLogIndex{%d}",
                    logIndex, m_lastSnapshotIncludeIndex, logIndex, getLastLogIndex()));
    return logTerm == getLogTermFromLogIndex(logIndex);
}
// so i'm your daddy,please rember this
void Raft::persist() {
    auto data = persistData();
    m_persister->SaveRaftState(data);
}

/**
 * @brief 变成 candidate 之后需要让其他结点给自己投票，
 *        candidate通过该函数远程调用远端节点的投票函数，非常重要！！！！
 * @param args 请求投票的参数
 * @param reply 请求投票的响应
 */
void Raft::RequestVote(const raftRpcProctoc::RequestVoteArgs* args, raftRpcProctoc::RequestVoteReply* reply) {
    std::lock_guard<std::mutex> lock(m_mtx);

    DEFER { persist(); };

    // 对args的term的三种情况分别进行处理，大于小于等于自己的term都是不同的处理
    // *** 竞选者已经过时，发送拒绝reply并不过多处理该请求
    if (args->term() < m_currentTerm) {
        reply->set_term(m_currentTerm);
        reply->set_votestate(Expire);
        reply->set_votegranted(false);
        return;
    }

    // *** 如果任何时候rpc请求或者响应的term大于自己的term，更新term，并变成follower
    if (args->term() > m_currentTerm) {
        m_status = Follower;
        m_currentTerm = args->term();
        m_votedFor = -1;
    }
    myAssert(args->term() == m_currentTerm,
             format("[func--rf{%d}] 前面校验过args.Term==rf.currentTerm，这里却不等", m_me));

    // 现在节点任期都是相同的(任期小的也已经更新到新的args的term了)，还需要检查log的term和index是不是匹配的
    int lastLogTerm = getLastLogTerm();

    // 只有 没投过票，且candidate的日志的新的程度 ≥ 接受者的日志新的程度 才会给请求方投票
    //  这里，新的程度判断为，如果当前节点的最新的一条日志所在的任期号大于远端节点，就不给他投
    //  如果这个关系是小于，就要考虑给对方投票，这个if进不来
    //  如果这个关系是等于，就要比较看谁的日志比较长，如果对方比较长，就要考虑给他投票，这个if也进不来
    //  其他情况就是对方的日志比较旧，直接进这个if，不给他投票
    if (!UpToDate(args->lastlogindex(), args->lastlogterm())) {
        reply->set_term(m_currentTerm);
        reply->set_votestate(Voted);
        reply->set_votegranted(false);
        return;
    }

    // ------------------------------ 对方的日志较新 ------------------------------ //
    // 当因为网络质量不好导致的请求丢失重发就有可能！！！！因此需要避免重复投票
    if (m_votedFor != -1 && m_votedFor != args->candidateid()) {  // 已经投过票了
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

bool Raft::UpToDate(int index, int term) {
    int lastIndex = -1;
    int lastTerm = -1;
    getLastLogIndexAndTerm(&lastIndex, &lastTerm);
    return term > lastTerm || (term == lastTerm && index >= lastIndex);
}

void Raft::getLastLogIndexAndTerm(int* lastLogIndex, int* lastLogTerm) {
    if (m_logs.empty()) {
        *lastLogIndex = m_lastSnapshotIncludeIndex;
        *lastLogTerm = m_lastSnapshotIncludeTerm;
        return;
    } else {
        *lastLogIndex = m_logs[m_logs.size() - 1].logindex();
        *lastLogTerm = m_logs[m_logs.size() - 1].logterm();
        return;
    }
}
/**
 *
 * @return 最新的log的logindex，即log的逻辑index。区别于log在m_logs中的物理index
 * 可见：getLastLogIndexAndTerm()
 */
int Raft::getLastLogIndex() {
    int lastLogIndex = -1;
    int _ = -1;
    getLastLogIndexAndTerm(&lastLogIndex, &_);
    return lastLogIndex;
}

int Raft::getLastLogTerm() {
    int _ = -1;
    int lastLogTerm = -1;
    getLastLogIndexAndTerm(&_, &lastLogTerm);
    return lastLogTerm;
}

/**
 * @brief 返回该节点的 logIndex 对应的任期
 * @param logIndex log的逻辑index。注意区别于m_logs的物理index
 * @return int 节点的 logIndex 对应的任期
 */
int Raft::getLogTermFromLogIndex(int logIndex) {
    myAssert(logIndex >= m_lastSnapshotIncludeIndex,
             format("[func-getSlicesIndexFromLogIndex-rf{%d}]  index{%d} < rf.lastSnapshotIncludeIndex{%d}", m_me,
                    logIndex, m_lastSnapshotIncludeIndex));

    int lastLogIndex = getLastLogIndex();

    myAssert(logIndex <= lastLogIndex,
             format("[func-getSlicesIndexFromLogIndex-rf{%d}]  logIndex{%d} > lastLogIndex{%d}", m_me, logIndex,
                    lastLogIndex));

    if (logIndex == m_lastSnapshotIncludeIndex) {
        return m_lastSnapshotIncludeTerm;
    } else {
        return m_logs[getSlicesIndexFromLogIndex(logIndex)].logterm();
    }
}

long long Raft::GetRaftStateSize() { return m_persister->RaftStateSize(); }

// 找到index对应的真实下标位置！！！
// 限制，输入的logIndex必须保存在当前的logs里面（不包含snapshot）
int Raft::getSlicesIndexFromLogIndex(int logIndex) {
    myAssert(logIndex > m_lastSnapshotIncludeIndex,
             format("[func-getSlicesIndexFromLogIndex-rf{%d}]  index{%d} <= rf.lastSnapshotIncludeIndex{%d}", m_me,
                    logIndex, m_lastSnapshotIncludeIndex));
    int lastLogIndex = getLastLogIndex();
    myAssert(logIndex <= lastLogIndex,
             format("[func-getSlicesIndexFromLogIndex-rf{%d}]  logIndex{%d} > lastLogIndex{%d}", m_me, logIndex,
                    lastLogIndex));
    int SliceIndex = logIndex - m_lastSnapshotIncludeIndex - 1;
    return SliceIndex;
}

/**
 * @brief 请求其他结点的投票
 * @param server 请求投票的结点
 * @param args 请求投票的参数
 * @param reply 请求投票的响应
 * @param votedNum 记录投票的结点数量
 * @param return 返回是否成功
 */
bool Raft::sendRequestVote(int server, const std::shared_ptr<raftRpcProctoc::RequestVoteArgs>& args,
                          const  std::shared_ptr<raftRpcProctoc::RequestVoteReply>& reply, const std::shared_ptr<int>& votedNum) {
    auto start = now();
    DPrintf("[func-sendRequestVote rf{%d}] --> server{%d} send RequestVote start", m_me, server, getLastLogIndex());
    bool ok = m_peers[server]->RequestVote(args.get(), reply.get());  // 发送请求投票，远程调用raft节点的投票
    DPrintf("[func-sendRequestVote rf{%d}] --> server{%d} send RequestVote finish，cost:{%d} ms", m_me, server,
            getLastLogIndex(), now() - start);
    // 没有 ok 为true的回复就是发送失败，也就不能处理回复，直接返回
    if (!ok) {
        return ok;
    }

    // 对回应进行处理，要记得无论什么时候收到回复就要检查term
    std::lock_guard<std::mutex> lock(m_mtx);
    if (reply->term() > m_currentTerm) {  // 三变：身份，term，和投票
        m_status = Follower;
        m_currentTerm = reply->term();
        m_votedFor = -1;
        persist();
        return true;
    } else if (reply->term() < m_currentTerm) {
        return true;
    }

    myAssert(reply->term() == m_currentTerm, format("assert {reply.Term==rf.currentTerm} fail"));
    if (!reply->votegranted()) {  // 如果投票不成功，直接返回
        return true;
    }

    // ---------------- 投票成功的逻辑 ---------------- //
    // follower如果投过来一票，它会把自己的任期改为目前candidate的任期
    *votedNum = *votedNum + 1;  // 如果投票成功，记录投票的结点数量，这里在lock锁的保护范围内，操作*votedNum是线程安全的
    if (*votedNum >= m_peers.size() / 2 + 1) {
        *votedNum = 0;
        if (m_status == Leader) {  // 如果已经是leader了，那么是就是了，不会进行下一步处理了
            myAssert(false, format("[func-sendRequestVote-rf{%d}]  term:{%d} 同一个term当两次领导，error", m_me,
                                   m_currentTerm));
        }
        m_status = Leader;  // 第一次变成leader，初始化状态和 nextIndex、matchIndex
        DPrintf("[func-sendRequestVote rf{%d}] elect success  ,current term:{%d} ,lastLogIndex:{%d}\n", m_me,
                m_currentTerm, getLastLogIndex());

        int lastLogIndex = getLastLogIndex();
        for (int i = 0; i < m_nextIndex.size(); i++) {  // 只有leader才需要维护 m_nextIndex 和 m_matchIndex
            m_nextIndex[i] = lastLogIndex + 1;
            m_matchIndex[i] = 0;  // 每换一个领导都是从0开始 ？
        }
        // 马上向其他节点宣告自己就是leader
        m_pool.commit_with_timeout(std::bind(&Raft::doHeartBeat, this), maxRandomizedElectionTime);
        persist();
    }
    return true;
}

/**
 * @brief 重写基类方法, 远程 follower 节点远程被调用
 */
void Raft::AppendEntries(google::protobuf::RpcController* controller,
                         const ::raftRpcProctoc::AppendEntriesArgs* request,
                         ::raftRpcProctoc::AppendEntriesReply* response, ::google::protobuf::Closure* done) {
    AppendEntries(request, response);
    done->Run();
}

void Raft::InstallSnapshot(google::protobuf::RpcController* controller,
                           const ::raftRpcProctoc::InstallSnapshotRequest* request,
                           ::raftRpcProctoc::InstallSnapshotResponse* response, ::google::protobuf::Closure* done) {
    InstallSnapshot(request, response);
    done->Run();
}

void Raft::RequestVote(google::protobuf::RpcController* controller, const ::raftRpcProctoc::RequestVoteArgs* request,
                       ::raftRpcProctoc::RequestVoteReply* response, ::google::protobuf::Closure* done) {
    RequestVote(request, response);
    done->Run();
}

bool Raft::Start(Op command, int* newLogIndex, int* newLogTerm) {
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_status != Leader) {
        *newLogIndex = -1;
        *newLogTerm = -1;
        return false;
    }

    raftRpcProctoc::LogEntry newLogEntry;
    newLogEntry.set_command(command.asString());
    newLogEntry.set_logterm(m_currentTerm);
    newLogEntry.set_logindex(getNewCommandIndex());
    m_logs.emplace_back(newLogEntry);  // 加入日志队列，等待复制

    int lastLogIndex = getLastLogIndex();

    DPrintf("[func-Start-rf{%d}]  lastLogIndex:%d,command:%s\n", m_me, lastLogIndex, &command);
    /// 原项目:todo 接收到命令后马上给follower发送,改成这样不知为何会出现问题，待修正
    /// leader应该不停的向各个Follower发送AE来维护心跳和保持日志同步，目前的做法是新的命令来了不会直接执行，而是等待leader的心跳触发
    /// 关于这个问题，有可能上一次的心跳包还没发(就本项目的架构而言，基本上是必然触发)，
    /// 再次发送给一个发送任务，两个线程同时写一个socket，未定义 ！！！！！
    persist();
    *newLogIndex = newLogEntry.logindex();
    *newLogTerm = newLogEntry.logterm();
    return true;
}

/**
 * @brief 初始化
 * @param peers 与其他raft节点通信的channel
 * @param me 自身raft节点在peers中的索引
 * @param persister 持久化类的 shared_ptr
 * @param applyCh 与kv-server沟通的channel，Raft层负责装填
 */
void Raft::init(std::vector<std::shared_ptr<RaftRpcUtil>> peers, int me, std::shared_ptr<Persister> persister,
                std::shared_ptr<LockQueue<ApplyMsg>> applyCh) {
    m_peers = std::move(peers);          // 需要与其他raft节点通信类
    m_persister = std::move(persister);  // 持久化类
    m_me = me;                // 标记自己，不能给自己发送rpc
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        this->m_applyChan = std::move(applyCh);  // 与kv-server沟通

        m_currentTerm = 0;    // 初始化当前任期为0
        m_status = Follower;  // 初始化状态为follower
        m_commitIndex = 0;    // 初始化commitIndex为0
        m_lastApplied = 0;    // 初始化lastApplied为0
        m_logs.clear();

        for (int i = 0; i < m_peers.size(); i++) {  // 这两个初始化参数 leader 才用得到
            m_matchIndex.push_back(0);
            m_nextIndex.push_back(0);
        }

        m_votedFor = -1;  // 当前term没有给其他人投过票就用-1表示
        m_lastSnapshotIncludeIndex = 0;
        m_lastSnapshotIncludeTerm = 0;
        m_lastResetElectionTime = now();
        m_lastResetHearBeatTime = now();

        readPersist(m_persister->ReadRaftState());  // initialize from state persisted before a crash
        if (m_lastSnapshotIncludeIndex > 0) {
            m_lastApplied = m_lastSnapshotIncludeIndex;
        }

        DPrintf("[Init&ReInit] Sever %d, term %d, lastSnapshotIncludeIndex {%d} , lastSnapshotIncludeTerm {%d}", m_me,
                m_currentTerm, m_lastSnapshotIncludeIndex, m_lastSnapshotIncludeTerm);
    }
    m_iom->schedule(std::bind(&Raft::leaderHearBeatTicker, this));  // leader 心跳定时器
    m_iom->schedule(std::bind(&Raft::electionTimeOutTicker, this));  // 选举超时定时器，触发就开始发起选举

    std::thread t3(std::bind(&Raft::applierTicker, this));  // apply 定时器，单起一个线程
    t3.detach();
}

std::string Raft::persistData() {
    BoostPersistRaftNode boostPersistRaftNode;
    boostPersistRaftNode.m_currentTerm = m_currentTerm;
    boostPersistRaftNode.m_votedFor = m_votedFor;
    boostPersistRaftNode.m_lastSnapshotIncludeIndex = m_lastSnapshotIncludeIndex;
    boostPersistRaftNode.m_lastSnapshotIncludeTerm = m_lastSnapshotIncludeTerm;
    for (auto& item : m_logs) {
        boostPersistRaftNode.m_logs.push_back(item.SerializeAsString());
    }

    std::stringstream ss;
    boost::archive::text_oarchive oa(ss);  // 关联 ss
    oa << boostPersistRaftNode;
    return ss.str();
}

void Raft::readPersist(const std::string& data) {
    if (data.empty()) {
        return;
    }
    std::stringstream iss(data);
    boost::archive::text_iarchive ia(iss);  // 关联 ss
    // read class state from archive
    BoostPersistRaftNode boostPersistRaftNode;
    ia >> boostPersistRaftNode;

    m_currentTerm = boostPersistRaftNode.m_currentTerm;
    m_votedFor = boostPersistRaftNode.m_votedFor;
    m_lastSnapshotIncludeIndex = boostPersistRaftNode.m_lastSnapshotIncludeIndex;
    m_lastSnapshotIncludeTerm = boostPersistRaftNode.m_lastSnapshotIncludeTerm;
    m_logs.clear();
    for (auto& item : boostPersistRaftNode.m_logs) {
        raftRpcProctoc::LogEntry logEntry;
        logEntry.ParseFromString(item);
        m_logs.emplace_back(logEntry);
    }
}

void Raft::Snapshot(int newLogIndex, const std::string& snapshot) {
    std::lock_guard<std::mutex> lock(m_mtx);

    // 没必要制作旧快照，也不能制作没提交的快照
    if (m_lastSnapshotIncludeIndex >= newLogIndex || newLogIndex > m_commitIndex) {
        DPrintf(
            "[func-Snapshot-rf{%d}] rejects replacing log with snapshotIndex %d as current snapshotIndex %d is larger "
            "or "
            "smaller ",
            m_me, newLogIndex, m_lastSnapshotIncludeIndex);
        return;
    }
    auto lastLogIndex = getLastLogIndex();  // 为了检查snapshot前后日志是否一样，防止多截取或者少截取日志

    // 制造完此快照后剩余的所有日志还保存在 m_logs 中
    int newLastSnapshotIncludeIndex = newLogIndex;
    int newLastSnapshotIncludeTerm = m_logs[getSlicesIndexFromLogIndex(newLogIndex)].logterm();
    std::vector<raftRpcProctoc::LogEntry> trunckedLogs;
    // todo :这种写法有点笨，待改进，而且有内存泄漏的风险
    for (int i = newLogIndex + 1; i <= getLastLogIndex(); i++) {
        // 注意有=，因为要拿到最后一个日志
        trunckedLogs.push_back(m_logs[getSlicesIndexFromLogIndex(i)]);
    }
    m_lastSnapshotIncludeIndex = newLastSnapshotIncludeIndex;
    m_lastSnapshotIncludeTerm = newLastSnapshotIncludeTerm;
    m_logs = trunckedLogs;
    m_commitIndex = std::max(m_commitIndex, newLogIndex);
    m_lastApplied = std::max(m_lastApplied, newLogIndex);

    // rf.lastApplied = index //lastApplied 和 commit应不应该改变呢？？？ 为什么 不应该改变吧
    m_persister->Save(persistData(), snapshot);

    DPrintf("[SnapShot]Server %d snapshot snapshot index {%d}, term {%d}, loglen {%d}", m_me, newLogIndex,
            m_lastSnapshotIncludeTerm, m_logs.size());
    myAssert(m_logs.size() + m_lastSnapshotIncludeIndex == lastLogIndex,
             format("len(rf.logs){%d} + rf.lastSnapshotIncludeIndex{%d} != lastLogjInde{%d}", m_logs.size(),
                    m_lastSnapshotIncludeIndex, lastLogIndex));
}