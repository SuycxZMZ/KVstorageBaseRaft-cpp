#!/bin/bash
set -x

cd src/rpc
protoc *.proto --cpp_out=./
mv *.h include/

cd ../raftRpcPro
protoc *.proto --cpp_out=./
mv *.h include/

cd ../..
cd example/rpcExample
protoc *.proto --cpp_out=./

