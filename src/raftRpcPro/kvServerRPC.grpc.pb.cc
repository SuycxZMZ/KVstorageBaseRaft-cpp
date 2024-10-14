// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: kvServerRPC.proto

#include "kvServerRPC.pb.h"
#include "kvServerRPC.grpc.pb.h"

#include <functional>
#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/async_unary_call.h>
#include <grpcpp/impl/codegen/channel_interface.h>
#include <grpcpp/impl/codegen/client_unary_call.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/impl/codegen/message_allocator.h>
#include <grpcpp/impl/codegen/method_handler.h>
#include <grpcpp/impl/codegen/rpc_service_method.h>
#include <grpcpp/impl/codegen/server_callback.h>
#include <grpcpp/impl/codegen/server_callback_handlers.h>
#include <grpcpp/impl/codegen/server_context.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/impl/codegen/sync_stream.h>
namespace raftKVRpcProctoc {

static const char* kvServerRpc_method_names[] = {
  "/raftKVRpcProctoc.kvServerRpc/PutAppend",
  "/raftKVRpcProctoc.kvServerRpc/Get",
};

std::unique_ptr< kvServerRpc::Stub> kvServerRpc::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< kvServerRpc::Stub> stub(new kvServerRpc::Stub(channel));
  return stub;
}

kvServerRpc::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_PutAppend_(kvServerRpc_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_Get_(kvServerRpc_method_names[1], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status kvServerRpc::Stub::PutAppend(::grpc::ClientContext* context, const ::raftKVRpcProctoc::PutAppendArgs& request, ::raftKVRpcProctoc::PutAppendReply* response) {
  return ::grpc::internal::BlockingUnaryCall< ::raftKVRpcProctoc::PutAppendArgs, ::raftKVRpcProctoc::PutAppendReply, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_PutAppend_, context, request, response);
}

void kvServerRpc::Stub::experimental_async::PutAppend(::grpc::ClientContext* context, const ::raftKVRpcProctoc::PutAppendArgs* request, ::raftKVRpcProctoc::PutAppendReply* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::raftKVRpcProctoc::PutAppendArgs, ::raftKVRpcProctoc::PutAppendReply, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_PutAppend_, context, request, response, std::move(f));
}

void kvServerRpc::Stub::experimental_async::PutAppend(::grpc::ClientContext* context, const ::raftKVRpcProctoc::PutAppendArgs* request, ::raftKVRpcProctoc::PutAppendReply* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_PutAppend_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::raftKVRpcProctoc::PutAppendReply>* kvServerRpc::Stub::PrepareAsyncPutAppendRaw(::grpc::ClientContext* context, const ::raftKVRpcProctoc::PutAppendArgs& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::raftKVRpcProctoc::PutAppendReply, ::raftKVRpcProctoc::PutAppendArgs, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_PutAppend_, context, request);
}

::grpc::ClientAsyncResponseReader< ::raftKVRpcProctoc::PutAppendReply>* kvServerRpc::Stub::AsyncPutAppendRaw(::grpc::ClientContext* context, const ::raftKVRpcProctoc::PutAppendArgs& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncPutAppendRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status kvServerRpc::Stub::Get(::grpc::ClientContext* context, const ::raftKVRpcProctoc::GetArgs& request, ::raftKVRpcProctoc::GetReply* response) {
  return ::grpc::internal::BlockingUnaryCall< ::raftKVRpcProctoc::GetArgs, ::raftKVRpcProctoc::GetReply, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_Get_, context, request, response);
}

void kvServerRpc::Stub::experimental_async::Get(::grpc::ClientContext* context, const ::raftKVRpcProctoc::GetArgs* request, ::raftKVRpcProctoc::GetReply* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::raftKVRpcProctoc::GetArgs, ::raftKVRpcProctoc::GetReply, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Get_, context, request, response, std::move(f));
}

void kvServerRpc::Stub::experimental_async::Get(::grpc::ClientContext* context, const ::raftKVRpcProctoc::GetArgs* request, ::raftKVRpcProctoc::GetReply* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Get_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::raftKVRpcProctoc::GetReply>* kvServerRpc::Stub::PrepareAsyncGetRaw(::grpc::ClientContext* context, const ::raftKVRpcProctoc::GetArgs& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::raftKVRpcProctoc::GetReply, ::raftKVRpcProctoc::GetArgs, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_Get_, context, request);
}

::grpc::ClientAsyncResponseReader< ::raftKVRpcProctoc::GetReply>* kvServerRpc::Stub::AsyncGetRaw(::grpc::ClientContext* context, const ::raftKVRpcProctoc::GetArgs& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncGetRaw(context, request, cq);
  result->StartCall();
  return result;
}

kvServerRpc::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      kvServerRpc_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< kvServerRpc::Service, ::raftKVRpcProctoc::PutAppendArgs, ::raftKVRpcProctoc::PutAppendReply, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](kvServerRpc::Service* service,
             ::grpc::ServerContext* ctx,
             const ::raftKVRpcProctoc::PutAppendArgs* req,
             ::raftKVRpcProctoc::PutAppendReply* resp) {
               return service->PutAppend(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      kvServerRpc_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< kvServerRpc::Service, ::raftKVRpcProctoc::GetArgs, ::raftKVRpcProctoc::GetReply, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](kvServerRpc::Service* service,
             ::grpc::ServerContext* ctx,
             const ::raftKVRpcProctoc::GetArgs* req,
             ::raftKVRpcProctoc::GetReply* resp) {
               return service->Get(ctx, req, resp);
             }, this)));
}

kvServerRpc::Service::~Service() {
}

::grpc::Status kvServerRpc::Service::PutAppend(::grpc::ServerContext* context, const ::raftKVRpcProctoc::PutAppendArgs* request, ::raftKVRpcProctoc::PutAppendReply* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status kvServerRpc::Service::Get(::grpc::ServerContext* context, const ::raftKVRpcProctoc::GetArgs* request, ::raftKVRpcProctoc::GetReply* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace raftKVRpcProctoc

