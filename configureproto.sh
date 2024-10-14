#!/bin/bash
set -x

cd ./src/raftRpcPro
protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) raftRPC.proto
protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) kvServerRPC.proto

cd ../..


