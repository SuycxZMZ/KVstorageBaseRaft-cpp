#include "raftServerRpcUtil.h"
#include "sylar/rpc/rpccontroller.h"
#include "rpc/KVrpcchannel.h"

raftServerRpcUtil::raftServerRpcUtil(const std::string& ip, short port) {
    stub = std::make_shared<raftKVRpcProctoc::kvServerRpc_Stub>(
        new KVrpcChannel(ip, port, false, CLERK_REQUEST_TIMEOUT / 2, CLERK_REQUEST_TIMEOUT / 2));
}

raftServerRpcUtil::~raftServerRpcUtil() { 
    std::cout << "--------------- [raftServerRpcUtil::~raftServerRpcUtil] a clerk released -------------\n"; 
}

bool raftServerRpcUtil::Get(raftKVRpcProctoc::GetArgs *GetArgs, raftKVRpcProctoc::GetReply *reply) {
    sylar::rpc::MprpcController controller;
    stub->Get(&controller, GetArgs, reply, nullptr);
    return !controller.Failed();
}

bool raftServerRpcUtil::PutAppend(raftKVRpcProctoc::PutAppendArgs *args, raftKVRpcProctoc::PutAppendReply *reply) {
    sylar::rpc::MprpcController controller;
    stub->PutAppend(&controller, args, reply, nullptr);
    if (controller.Failed()) {
        std::cout << controller.ErrorText() << std::endl;
    }
    return !controller.Failed();
}
