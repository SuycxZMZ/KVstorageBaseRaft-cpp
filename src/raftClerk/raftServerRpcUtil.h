#ifndef RAFTSERVERRPC_H
#define RAFTSERVERRPC_H

#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/client_context.h>
#include <grpcpp/impl/codegen/status.h>
#include <grpcpp/security/credentials.h>
#include <memory>
#include <string>
#include "raftRpcPro/kvServerRPC.grpc.pb.h"
#include "raftRpcPro/kvServerRPC.pb.h"

using grpc::ClientContext;
using grpc::Status;
using raftKVRpcProctoc::GetArgs;
using raftKVRpcProctoc::GetReply;
using raftKVRpcProctoc::kvServerRpc;
using raftKVRpcProctoc::PutAppendArgs;
using raftKVRpcProctoc::PutAppendReply;

class raftServerRpcUtil {
private:
  std::shared_ptr<kvServerRpc::Stub> m_stub;

public:
  bool Get(raftKVRpcProctoc::GetArgs* args, raftKVRpcProctoc::GetReply* reply);
  bool PutAppend(raftKVRpcProctoc::PutAppendArgs* args, raftKVRpcProctoc::PutAppendReply* reply);

  raftServerRpcUtil(const std::string& ipPort);
  ~raftServerRpcUtil();
};

#endif  // RAFTSERVERRPC_H
