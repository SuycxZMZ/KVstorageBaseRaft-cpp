#include "raftRpcUtil.h"
#include <grpcpp/create_channel.h>
#include <grpcpp/impl/codegen/client_context.h>
#include <grpcpp/impl/codegen/status.h>
#include <grpcpp/security/credentials.h>
#include "raftRpcPro/raftRPC.pb.h"
#include "spdlog/spdlog.h"

bool RaftRpcUtil::AppendEntries(raftRpcProctoc::AppendEntriesArgs *args, raftRpcProctoc::AppendEntriesReply *response) {
    grpc::ClientContext context;
    Status status = m_stub->AppendEntries(&context, *args, response);
    if (!status.ok()) {
        std::cerr << "failure " + status.error_message() << std::endl;
        return false;
    }
    return true;
}

bool RaftRpcUtil::InstallSnapshot(raftRpcProctoc::InstallSnapshotRequest *args,
                                  raftRpcProctoc::InstallSnapshotResponse *response) {
    grpc::ClientContext context;
    Status status = m_stub->InstallSnapshot(&context, *args, response);
    if (!status.ok()) {
        std::cerr << "failure " + status.error_message() << std::endl;
        return false;
    }
    return true;
}

bool RaftRpcUtil::RequestVote(raftRpcProctoc::RequestVoteArgs *args, raftRpcProctoc::RequestVoteReply *response) {
    grpc::ClientContext context;
    Status status = m_stub->RequestVote(&context, *args, response);
    if (!status.ok()) {
        std::cerr << "failure " + status.error_message() << std::endl;
        return false;
    }
    return true;
}

RaftRpcUtil::RaftRpcUtil(const std::string& ipPort) :
  m_stub(raftRpc::NewStub(grpc::CreateChannel(ipPort, grpc::InsecureChannelCredentials())))
{
}

RaftRpcUtil::~RaftRpcUtil() {
    spdlog::info("--------------- [RaftRpcUtil::~RaftRpcUtil()] ------------------- ");
}
