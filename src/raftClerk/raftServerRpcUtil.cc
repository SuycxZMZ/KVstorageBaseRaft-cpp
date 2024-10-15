#include "raftServerRpcUtil.h"
#include <grpcpp/create_channel.h>
#include <grpcpp/impl/codegen/client_context.h>
#include <grpcpp/impl/codegen/status.h>
#include <grpcpp/security/credentials.h>
#include <iostream>
#include <string>
#include "raftRpcPro/kvServerRPC.pb.h"
#include "spdlog/spdlog.h"

raftServerRpcUtil::raftServerRpcUtil(const std::string &ipPort)
    : m_stub(kvServerRpc::NewStub(grpc::CreateChannel(ipPort, grpc::InsecureChannelCredentials()))) {
}

raftServerRpcUtil::~raftServerRpcUtil() {
    spdlog::info("--------------- [raftServerRpcUtil::~raftServerRpcUtil] a clerk created -------------");
}

bool raftServerRpcUtil::Get(raftKVRpcProctoc::GetArgs *args, raftKVRpcProctoc::GetReply *reply) {
    grpc::ClientContext context;
    Status status = m_stub->Get(&context, *args, reply);
    if (!status.ok()) {
        std::cerr << "failure " + status.error_message() << std::endl;
        return false;
    }
    return true;
}

bool raftServerRpcUtil::PutAppend(raftKVRpcProctoc::PutAppendArgs *args, raftKVRpcProctoc::PutAppendReply *reply) {
    grpc::ClientContext context;
    Status status = m_stub->PutAppend(&context, *args, reply);
    if (!status.ok()) {
        std::cerr << "failure " + status.error_message() << std::endl;
        return false;
    }
    return true;
}
