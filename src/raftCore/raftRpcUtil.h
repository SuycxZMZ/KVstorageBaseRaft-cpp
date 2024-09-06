//
// Created by swx on 23-12-28.
//

#ifndef RAFTRPC_H
#define RAFTRPC_H

#include "raftRpcPro/raftRPC.pb.h"

/// @brief 维护当前节点对其他某一个结点的所有rpc发送通信的功能
///        对于一个raft节点来说，要和任意其他的节点维护一个rpc连接，即MprpcChannel
class RaftRpcUtil {
private:
    raftRpcProctoc::raftRpc_Stub *stub_;

public:
    // 暴露给rpc调用方内部调用 stub->AppendEntries
    bool AppendEntries(raftRpcProctoc::AppendEntriesArgs *args, raftRpcProctoc::AppendEntriesReply *response);
    // 暴露给rpc调用方内部调用 stub->InstallSnapshot
    bool InstallSnapshot(raftRpcProctoc::InstallSnapshotRequest *args,
                         raftRpcProctoc::InstallSnapshotResponse *response);
    // 暴露给rpc调用方内部调用 stub->RequestVote
    bool RequestVote(raftRpcProctoc::RequestVoteArgs *args, raftRpcProctoc::RequestVoteReply *response);

    /// @brief RaftRpcUtil 构造函数
    /// @param ip 对端ip
    /// @param port 对端端口号
    RaftRpcUtil(const std::string& ip, short port);
    ~RaftRpcUtil();

    RaftRpcUtil(const RaftRpcUtil& other) = delete; // 禁止浅拷贝
    RaftRpcUtil& operator= (const RaftRpcUtil& other) = delete; // 禁止浅拷贝
};

#endif  // RAFTRPC_H
