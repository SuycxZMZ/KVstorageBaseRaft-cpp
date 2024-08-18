#ifndef SKIP_LIST_ON_RAFT_KVSERVER_H
#define SKIP_LIST_ON_RAFT_KVSERVER_H

#include <boost/any.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/foreach.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include "raftRpcPro/kvServerRPC.pb.h"
#include "rpc/KVrpcprovider.h"
#include "raft.h"
#include "skipList/skipList.h"

/**
 * @brief kvServer负责与外部客户端和其他raft节点通信
 *        一个外部请求的处理可以简单的看成两步：
 *        1.接收外部请求。
 *        2.本机内部与raft和kvDB协商如何处理该请求。
 *        3.返回外部响应。
*/
class KvServer : raftKVRpcProctoc::kvServerRpc {
private:
    std::mutex m_mtx;
    int m_me;                                         // 节点编号
    std::shared_ptr<LockQueue<ApplyMsg> > m_applyChan;  // kvServer中拿到的消息，server用这些消息与raft打交道，由Raft::applierTicker线程填充
    int m_maxRaftState;                               // snapshot if log grows this big

    // Your definitions here.
    std::string m_serializedKVData;                      // TODO ： 序列化后的kv数据，理论上可以不用，但是目前没有找到特别好的替代方法
    SkipList<std::string, std::string> m_skipList;       // skipList，用于存储kv数据
    std::unordered_map<std::string, std::string> m_kvDB; // kvDB，用unordered_map来替代

    std::unordered_map<int, LockQueue<Op> *> m_waitApplyCh;// 字段含义 waitApplyCh是一个map，键 是int，值 是Op类型的阻塞队列

    std::unordered_map<std::string, int> m_lastRequestClientAndId;  // pair<客户id, 最近一次的请求id> 一个kV服务器可能连接多个client

    // last SnapShot point , raftIndex
    int m_lastSnapShotRaftLogIndex;
    sylar::IOManager::ptr m_iom; // 全局协程调度器指针
    std::shared_ptr<Raft> m_raftNode; // raft节点                
    std::shared_ptr<KVRpcProvider> m_KvRpcProvider; // one kvServer per rpcprovider
public:
    KvServer() = delete;

    /**
     * @brief 构造函数
     * @param me 节点编号
     * @param maxraftstate 快照阈值，raft日志超过这个值时，会触发快照
     * @param nodeInforFileName 节点信息文件名
     * @param port 监听端口
    */
    KvServer(int me, int maxraftstate, std::string nodeInforFileName, short port);

    void StartKVServer();

    void DprintfKVDB();



    void ExecuteGetOpOnKVDB(Op op, std::string *value, bool *exist);    // get操作，查跳表，返回结果
    void ExecuteAppendOpOnKVDB(Op op);  // 操作KVDB，本项目中就是插入跳表，与put操作一样
    void ExecutePutOpOnKVDB(Op op);     // 操作KVDB，本项目中就是插入跳表，与append操作一样

    /**
     * @brief rpc 实际调用内部实现
    */
    void Get(const raftKVRpcProctoc::GetArgs *args,
             raftKVRpcProctoc::GetReply *reply);  

    /**
     * @brief 从raft节点获取命令，操作kvDB
     * @param message 解析raft层传过来的命令
     */
    void GetCommandFromRaft(ApplyMsg message);

    // 检查请求是否被持久化，如果客户id不存在，或者对应的请求id太大，就返flase
    // 客户id存在且请求id小于等于当前最大值，返回true
    bool ifRequestDuplicate(std::string ClientId, int RequestId);

    /**
     * @brief rpc 实际调用内部实现
    */
    void PutAppend(const raftKVRpcProctoc::PutAppendArgs *args, raftKVRpcProctoc::PutAppendReply *reply);

    /**
     * @brief 一直等待raft传来的applyCh
    */
    void ReadRaftApplyCommandLoop();

    void ReadSnapShotToInstall(std::string snapshot);

    /**
     * @brief 将命令发送到kvserver层的 m_waitApplyChan
     * @param op 从raft层解析出来的，要执行apply的命令
     * @param raftIndex 节点序号
     */
    bool SendMessageToWaitChan(const Op &op, int raftIndex);

    // 检查是否需要制作快照，需要的话就向raft之下制作快照
    void IfNeedToSendSnapShotCommand(int raftIndex, int proportion);

    // Handler the SnapShot from kv.rf.applyCh
    void GetSnapShotFromRaft(ApplyMsg message);

    std::string MakeSnapShot();

    /// @brief 初始化本节点的底层rpc,发布远程方法，并开启
    /// @param port 节点端口号
    void InitRpcAndRun(short port);

public:  // for rpc
    /**
     * @brief 与客户打交道，Rpc框架调用
    */
    void PutAppend(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::PutAppendArgs *request,
                   ::raftKVRpcProctoc::PutAppendReply *response, ::google::protobuf::Closure *done) override;
    /**
     * @brief 与客户打交道，Rpc框架调用
    */
    void Get(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::GetArgs *request,
             ::raftKVRpcProctoc::GetReply *response, ::google::protobuf::Closure *done) override;

///////////////// serialiazation ///////////////////////////////
// notice ： func serialize
private:
    friend class boost::serialization::access;

    // When the class Archive corresponds to an output archive, the
    // & operator is defined similar to <<.  Likewise, when the class Archive
    // is a type of input archive the & operator is defined similar to >>.
    template <class Archive>
    void serialize(Archive &ar, const unsigned int version)  // 这里面写需要序列话和反序列化的字段
    {
        ar & m_serializedKVData;

        // ar & m_kvDB;
        ar & m_lastSnapShotRaftLogIndex;
    }

    std::string getSnapshotData() {
        m_serializedKVData = m_skipList.dump_file();
        std::stringstream ss;
        boost::archive::text_oarchive oa(ss);
        oa << *this;
        m_serializedKVData.clear();
        return ss.str();
    }

    void parseFromString(const std::string &str) {
        std::stringstream ss(str);
        boost::archive::text_iarchive ia(ss);
        ia >> *this;
        m_skipList.load_file(m_serializedKVData);
        m_serializedKVData.clear();
    }

    ///////////////// serialiazation ///////////////////////////////
};

#endif  // SKIP_LIST_ON_RAFT_KVSERVER_H
