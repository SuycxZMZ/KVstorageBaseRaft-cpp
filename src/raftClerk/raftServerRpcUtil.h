#ifndef RAFTSERVERRPC_H
#define RAFTSERVERRPC_H

#include "raftRpcPro/kvServerRPC.pb.h"

/// @brief 维护当前节点对其他某一个结点的所有rpc通信，包括接收其他节点的rpc和发送
class raftServerRpcUtil {
private:
    std::shared_ptr<raftKVRpcProctoc::kvServerRpc_Stub> stub;
public:
    bool Get(raftKVRpcProctoc::GetArgs* GetArgs, raftKVRpcProctoc::GetReply* reply);
    bool PutAppend(raftKVRpcProctoc::PutAppendArgs* args, raftKVRpcProctoc::PutAppendReply* reply);

    raftServerRpcUtil(const std::string& ip, short port);
    ~raftServerRpcUtil();
};

#endif  // RAFTSERVERRPC_H
