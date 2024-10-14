#ifndef RAFTRPC_H
#define RAFTRPC_H

#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/impl/codegen/client_context.h>
#include <grpcpp/impl/codegen/status.h>
#include <memory>
#include "raftRpcPro/raftRPC.pb.h"
#include "raftRpcPro/raftRPC.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using raftRpcProctoc::AppendEntriesArgs;
using raftRpcProctoc::AppendEntriesReply;
using raftRpcProctoc::InstallSnapshotRequest;
using raftRpcProctoc::InstallSnapshotResponse;
using raftRpcProctoc::LogEntry;
using raftRpcProctoc::raftRpc;
using raftRpcProctoc::RequestVoteArgs;
using raftRpcProctoc::RequestVoteReply;


class RaftRpcUtil {
private:
    std::shared_ptr<raftRpc::Stub> m_stub;
public:

    bool AppendEntries(raftRpcProctoc::AppendEntriesArgs *args, raftRpcProctoc::AppendEntriesReply *response);

    bool InstallSnapshot(raftRpcProctoc::InstallSnapshotRequest *args,
                         raftRpcProctoc::InstallSnapshotResponse *response);

    bool RequestVote(raftRpcProctoc::RequestVoteArgs *args, raftRpcProctoc::RequestVoteReply *response);

    explicit RaftRpcUtil(const std::string& ipPort);
    ~RaftRpcUtil();

    RaftRpcUtil(const RaftRpcUtil& other) = delete;
    RaftRpcUtil& operator= (const RaftRpcUtil& other) = delete;
};

#endif  // RAFTRPC_H
