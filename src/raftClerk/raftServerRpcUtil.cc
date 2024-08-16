//
// Created by swx on 24-1-4.
//
#include "raftServerRpcUtil.h"

raftServerRpcUtil::raftServerRpcUtil(std::string ip, short port) {
    stub = new raftKVRpcProctoc::kvServerRpc_Stub(new KVrpcChannel(ip, port, false));
}

raftServerRpcUtil::~raftServerRpcUtil() { delete stub; }

bool raftServerRpcUtil::Get(raftKVRpcProctoc::GetArgs *GetArgs, raftKVRpcProctoc::GetReply *reply) {
    sylar::rpc::MprpcController controller;
    stub->Get(&controller, GetArgs, reply, nullptr);
    return !controller.Failed();
}

bool raftServerRpcUtil::PutAppend(raftKVRpcProctoc::PutAppendArgs *args, raftKVRpcProctoc::PutAppendReply *reply) {
    sylar::rpc::MprpcController controller;
    stub->PutAppend(&controller, args, reply, nullptr);
    if (controller.Failed()) {
        std::cout << controller.ErrorText() << endl;
    }
    return !controller.Failed();
}
