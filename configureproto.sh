#!/bin/bash
set -x

cd src/rpc
protoc *.proto --cpp_out=./

cd ../raftRpcPro
protoc *.proto --cpp_out=./

cd ../..
cd example/rpcExample
protoc *.proto --cpp_out=./

