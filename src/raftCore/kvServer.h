//
// Created by swx on 23-6-1.
//

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
#include "raft.h"
#include "skipList/skipList.h"

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

    // Your definitions here.
    std::string m_serializedKVData;                      // todo ： 序列化后的kv数据，理论上可以不用，但是目前没有找到特别好的替代方法
    SkipList<std::string, std::string> m_skipList;       // skipList，用于存储kv数据
    std::unordered_map<std::string, std::string> m_kvDB; // kvDB，用unordered_map来替代

    std::unordered_map<int, LockQueue<Op> *> waitApplyCh;//？？？字段含义   waitApplyCh是一个map，键是int，值是Op类型的管道
    // index(raft) -> chan  //？？？字段含义   waitApplyCh是一个map，键是int，值是Op类型的管道

    std::unordered_map<std::string, int> m_lastRequestId;  // clientid -> requestID 一个kV服务器可能连接多个client

    // last SnapShot point , raftIndex
    int m_lastSnapShotRaftLogIndex;

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

    void ExecuteAppendOpOnKVDB(Op op);

    void ExecuteGetOpOnKVDB(Op op, std::string *value, bool *exist);

    void ExecutePutOpOnKVDB(Op op);

    /**
     * @brief rpc 实际调用内部实现
    */
    void Get(const raftKVRpcProctoc::GetArgs *args,
             raftKVRpcProctoc::GetReply *reply);  // 将 GetArgs 改为rpc调用的，因为是远程客户端，即服务器宕机对客户端来说是无感的
    /**
     * @brief 从raft节点获取命令，操作kvDB
     * @param message
     */
    void GetCommandFromRaft(ApplyMsg message);

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

    bool SendMessageToWaitChan(const Op &op, int raftIndex);

    // 检查是否需要制作快照，需要的话就向raft之下制作快照
    void IfNeedToSendSnapShotCommand(int raftIndex, int proportion);

    // Handler the SnapShot from kv.rf.applyCh
    void GetSnapShotFromRaft(ApplyMsg message);

    std::string MakeSnapShot();

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

/////////////////serialiazation start ///////////////////////////////
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
        ar & m_lastRequestId;
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

    /////////////////serialiazation end ///////////////////////////////
};

#endif  // SKIP_LIST_ON_RAFT_KVSERVER_H
