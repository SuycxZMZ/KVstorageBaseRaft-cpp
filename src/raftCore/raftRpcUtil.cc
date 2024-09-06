//
// Created by swx on 23-12-28.
//

#include "raftRpcUtil.h"
#include "rpc/KVrpcchannel.h"
#include "sylar/rpc/rpccontroller.h"

bool RaftRpcUtil::AppendEntries(raftRpcProctoc::AppendEntriesArgs *args, raftRpcProctoc::AppendEntriesReply *response) {
    sylar::rpc::MprpcController controller;
    stub_->AppendEntries(&controller, args, response, nullptr);
    return !controller.Failed();
}

bool RaftRpcUtil::InstallSnapshot(raftRpcProctoc::InstallSnapshotRequest *args,
                                  raftRpcProctoc::InstallSnapshotResponse *response) {
    sylar::rpc::MprpcController controller;
    stub_->InstallSnapshot(&controller, args, response, nullptr);
    return !controller.Failed();
}

bool RaftRpcUtil::RequestVote(raftRpcProctoc::RequestVoteArgs *args, raftRpcProctoc::RequestVoteReply *response) {
    sylar::rpc::MprpcController controller;
    stub_->RequestVote(&controller, args, response, nullptr);
    return !controller.Failed();
}

// 先开启服务器，再尝试连接其他的节点，中间给一个间隔时间，等待其他的rpc服务器节点启动

RaftRpcUtil::RaftRpcUtil(const std::string& ip, short port) {
    // 发送rpc设置
    // KVrpcChannel 里面完成连接
    stub_ = new raftRpcProctoc::raftRpc_Stub(
        new KVrpcChannel(ip, port, true, HeartBeatTimeout / 2, HeartBeatTimeout / 2));
}

RaftRpcUtil::~RaftRpcUtil() { 
    std::cout << "--------------- [RaftRpcUtil::~RaftRpcUtil()] ------------------- \n";
    delete stub_; 
}
