#ifndef RAFT_H
#define RAFT_H

#include <atomic>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "ApplyMsg.h"
#include "Persister.h"
#include "common/config.h"
#include "common/util.h"
#include "raftRpcUtil.h"
#include "sylar/iomanager.h"
#include "threadPool/threadpool.h"
#include "msd/channel.hpp"

/// @brief //////////// 网络状态表示  todo：可以在rpc中删除该字段，实际生产中是用不到的.
/// 方便网络分区的时候debug，网络异常的时候为disconnected，只要网络正常就为AppNormal，防止matchIndex[]数组异常减小
constexpr int Disconnected = 0;
constexpr int AppNormal = 1;

///////////////投票状态
constexpr int Killed = 0;
constexpr int Voted = 1;   // 本轮已经投过票了
constexpr int Expire = 2;  // 投票（消息、竞选者）过期
constexpr int Normal = 3;

/**
 * @brief 工具函数，计算线程池的大小 编译期计算
 *
 * @return constexpr int
 */
constexpr int calculate_pool_size() { return static_cast<int>(MAX_NODE_NUM * 1.5); }

/**
 * @brief Raft 核心类
 */
class Raft : public raftRpcProctoc::raftRpc {
   public:
    using applyCh = msd::channel<ApplyMsg>;
   private:
    std::mutex m_mtx;                                   // 互斥锁，用于保护raft状态的修改
    std::vector<std::shared_ptr<RaftRpcUtil>> m_peers;  // 保存与其他raft结点通信的rpc入口
    std::shared_ptr<Persister> m_persister;             // 持久化层，负责raft数据的持久化

    int m_me;           // raft是以集群启动，这个用来标识自己的的编号
    int m_currentTerm;  // 记录任期
    int m_votedFor;     // 记录当前term给谁投票过

    std::vector<raftRpcProctoc::LogEntry> m_logs;  // raft节点保存的全部的日志信息。
    int m_commitIndex;                             // 已提交到状态机的日志的index
    int m_lastApplied;                             // 已经汇报给状态机（上层应用）的log 的index
    // 这两个状态的下标1开始，因为通常commitIndex和lastApplied从0开始，应该是一个无效的index，因此下标从1开始

    std::vector<int> m_nextIndex;   // m_nextIndex 保存leader下一次应该从哪一个日志开始发送给follower
    std::vector<int> m_matchIndex;  // m_matchIndex表示follower在哪一个日志是已经匹配了的

    enum Status { Follower, Candidate, Leader };  // raft节点身份枚举
    Status m_status;                              // 节点身份

    std::shared_ptr<applyCh> m_applyChan;  // applyTicker会写，Installsnapshot也会写

    std::chrono::_V2::system_clock::time_point m_lastResetElectionTime;  // 选举超时时间
    std::chrono::_V2::system_clock::time_point m_lastResetHearBeatTime;  // 心跳超时，用于leader

    // Snapshot是kvDb的快照，也可以看成是日志，因此:全部的日志 = m_logs + snapshot
    int m_lastSnapshotIncludeIndex;  // 最新的一个快照中包含的日志条目最大索引
    int m_lastSnapshotIncludeTerm;   // 最新的一个快照中日志条目的任期号

    // 协程调度器
    sylar::IOManager::ptr m_iom;
    // 工作线程池，主要用来发送AE和投票请求
    sylar::threadpool m_pool;

   public:
    Raft() = delete;

    /// @brief raft节点构造函数
    /// @param iom 指向全局调度器的智能指针
    explicit Raft(sylar::IOManager::ptr iom);

    ~Raft();

    /**
     * @brief 日志同步 + 心跳 rpc ，重点关注。follow 节点执行的操作
     * @param args 接收的rpc参数
     * @param reply 回复的rpc参数
     */
    void AppendEntries(const raftRpcProctoc::AppendEntriesArgs *args, raftRpcProctoc::AppendEntriesReply *reply);

    /**
     * @brief 单起一个线程，定期将已提交但未 apply 的日志加入 m_applyChan , KVserver会取出这些命令应用到KVDB
     */
    void applierTicker();

    /**
     * @brief 安装快照 TODO，还没实现好，现在直接返回true
     */
    static bool CondInstallSnapshot(int lastIncludedTerm, int lastIncludedIndex, const std::string& snapshot);

    /**
     * @brief 实际发起选举，构造需要发送的rpc，并多线程调用sendRequestVote处理rpc及其相应。
     */
    void doElection();

    /**
     * @brief 发起心跳，只有leader才需要发起心跳
     *        每隔一段时间检查睡眠时间内有没有重置定时器，没有则说明超时了
     *        如果有则设置合适睡眠时间：睡眠到重置时间+超时时间
     */
    void doHeartBeat();

    /**
     * @brief 1.负责查看是否该发起选举，如果该发起选举就执行doElection发起选举。
     *        2.doElection：实际发起选举，构造需要发送的rpc，并多线程调用sendRequestVote处理rpc及其相应。
     *        3.sendRequestVote：负责发送选举中的RPC，在发送完rpc后还需要负责接收并处理对端发送回来的响应。
     *        4.RequestVote：远端执行，接收别人发来的选举请求，主要检验是否要给对方投票。
     */
    void electionTimeOutTicker();

    /**
     * @brief 返回待apply的日志信息
     */
    std::vector<ApplyMsg> getApplyLogs();
    int getNewCommandIndex();
    void getPrevLogInfo(int server, int *preIndex, int *preTerm);

    /**
     * @brief 看当前节点是否是leader
     */
    // void GetState(int *term, int *isLeader);
    bool GetState(int *term);

    /**
     * @brief 这是真正被调用到的rpc方法，接收leader发来的快照请求，同步快照到本机，
     *        其实也是把信息塞到 m_applyChan里面，让server层应用到kvDB
     * @param args 接收参数
     * @param reply 填写reply
     */
    void InstallSnapshot(const raftRpcProctoc::InstallSnapshotRequest *args,
                         raftRpcProctoc::InstallSnapshotResponse *reply);

    /**
     * @brief 检查是否需要发起心跳（leader）如果该发起就执行doHeartBeat。
     */
    void leaderHearBeatTicker();
    void leaderSendSnapShot(int server);

    /**
     * @brief leader更新commitIndex，在项目中没调用
     */
    void leaderUpdateCommitIndex();

    /**
     * @brief leader传过来的对应Index的日志是否匹配，只需要Index和Term就可以知道是否匹配
     * @param logIndex leader传过来的日志的索引
     * @param logTerm leader传过来的日志的Term
     */
    bool matchLog(int logIndex, int logTerm);

    /**
     * @brief 持久化
     */
    void persist();

    /**
     * @brief 变成 candidate 之后需要让其他结点给自己投票，
     *        candidate通过该函数远程调用远端节点的投票函数，非常重要！！！！
     * @param args 请求投票的参数
     * @param reply 请求投票的响应
     */
    void RequestVote(const raftRpcProctoc::RequestVoteArgs *args, raftRpcProctoc::RequestVoteReply *reply);

    /**
     * @brief 判断传入的日志参数是否比自身的新，判断是否要给对方投票的时候调用
     * @param index 传入要比较的日志index
     * @param term 传入要比较的日志的任期号
     * @return 传入的日志参数比自身的新返回true 否则返回false
     */
    bool UpToDate(int index, int term);
    int getLastLogIndex();
    int getLastLogTerm();
    void getLastLogIndexAndTerm(int *lastLogIndex, int *lastLogTerm);

    /**
     * @brief 返回该节点的 logIndex 对应的任期
     * @param logIndex log的逻辑index。注意区别于m_logs的物理index
     * @return int 节点的 logIndex 对应的任期
     */
    int getLogTermFromLogIndex(int logIndex);
    long long GetRaftStateSize();

    /**
     * @brief 设计快照之后logIndex不能与再日志中的数组下标相等了，根据logIndex找到其在日志数组中的位置
     */
    int getSlicesIndexFromLogIndex(int logIndex);

    /**
     * @brief 请求其他结点的投票
     * @param server 请求投票的结点索引
     * @param args 请求投票的参数
     * @param reply 请求投票的响应
     * @param votedNum 记录投票的结点数量
     * @param return 返回是否成功
     */
    bool sendRequestVote(int server, const std::shared_ptr<raftRpcProctoc::RequestVoteArgs>& args,
                        const std::shared_ptr<raftRpcProctoc::RequestVoteReply>& reply, const std::shared_ptr<int>& votedNum);

    /**
     * @brief Leader真正发送心跳的函数，执行RPC
     * @param server 回复的结点
     * @param args 回复的参数
     * @param reply 回复的响应
     * @param appendNums 记录回复的结点数量
     * @param return 返回是否成功
     */
    bool sendAppendEntries(int server, const std::shared_ptr<raftRpcProctoc::AppendEntriesArgs>& args,
                           const std::shared_ptr<raftRpcProctoc::AppendEntriesReply>& reply,
                           const std::shared_ptr<std::atomic_int32_t>& appendNums);

    /**
     * @brief 给上层的kvserver层发送消息
     */
    void pushMsgToKvServer(ApplyMsg msg);
    void readPersist(const std::string& data);

    /**
     * @brief 将当前raft节点的状态序列化为字符串
     *
     * @return std::string 序列化的字符流状态
     */
    std::string persistData();

    /**
     * @brief kvServer收到客户端命令之后发起，客户端发布发一个新日志
     *
     * @param command 操作信息 Op 包括请求的 k, v 请求id，客户端id等
     * @param newLogIndex 输入输出值，正常情况下返回一个新的日志index
     * @param newLogTerm 输入输出值，正常情况下返回一个新的日志 所在 raft主节点的任期
     * @return true 发起成功
     * @return false 发起失败，不是leader
     */
    bool Start(Op command, int *newLogIndex, int *newLogTerm);

    /**
     * @brief 根据kvServer层传过来的 newLogIndex 和 KVDB快照进行压缩日志
     *
     * @param newLogIndex kvServer层传过来的新的日志索引
     * @param snapshot kvServer层传过来的KVDB数据快照
     */
    void Snapshot(int newLogIndex, const std::string& snapshot);

   public:
    /**
     * @brief 重写基类方法, 远程 follower 节点远程被调用。动态多态，框架会自己调用
     */
    void AppendEntries(google::protobuf::RpcController *controller,
                               const ::raftRpcProctoc::AppendEntriesArgs *request,
                               ::raftRpcProctoc::AppendEntriesReply *response,
                               ::google::protobuf::Closure *done) override;

    /**
     * @brief 重写基类方法,因为rpc远程调用真正调用的是这个方法
     *        序列化，反序列化等操作rpc框架都已经做完了，因此这里只需要获取值然后真正调用本地方法即可。
     */
    void InstallSnapshot(google::protobuf::RpcController *controller,
                                 const ::raftRpcProctoc::InstallSnapshotRequest *request,
                                 ::raftRpcProctoc::InstallSnapshotResponse *response,
                                 ::google::protobuf::Closure *done) override;

    /**
     * @brief 重写基类方法,因为rpc远程调用真正调用的是这个方法
     *        序列化，反序列化等操作rpc框架都已经做完了，因此这里只需要获取值然后真正调用本地方法即可。
     */
    void RequestVote(google::protobuf::RpcController *controller,
                             const ::raftRpcProctoc::RequestVoteArgs *request,
                             ::raftRpcProctoc::RequestVoteReply *response, ::google::protobuf::Closure *done) override;

   public:
    /**
     * @brief 初始化
     * @param peers 与其他raft节点通信的channel
     * @param me 自身raft节点在peers中的索引
     * @param persister 持久化类的 shared_ptr
     * @param applyCh 与kv-server沟通的channel，Raft层负责装填
     */
    void init(std::vector<std::shared_ptr<RaftRpcUtil>> peers, int me, std::shared_ptr<Persister> persister,
              std::shared_ptr<applyCh> applych);

   private:
    /**
     * @brief 对Raft节点的状态进行序列化和反序列化
     */
    class BoostPersistRaftNode {
       public:
        friend class boost::serialization::access;

        /**
         * @brief When the class Archive corresponds to an output archive, the
         *        & operator is defined similar to <<.  Likewise, when the class Archive
         *        is a type of input archive the & operator is defined similar to >>.
         */
        template <class Archive>
        void serialize(Archive &ar, const unsigned int version) {
            ar & m_currentTerm;
            ar & m_votedFor;
            ar & m_lastSnapshotIncludeIndex;
            ar & m_lastSnapshotIncludeTerm;
            ar & m_logs;
        }
        int m_currentTerm;
        int m_votedFor;
        int m_lastSnapshotIncludeIndex;
        int m_lastSnapshotIncludeTerm;
        std::vector<std::string> m_logs;
        std::unordered_map<std::string, int> umap;
    };
};

// typedef sylar::SingletonPtr<sylar::threadpool> t_pool;

#endif  // RAFT_H