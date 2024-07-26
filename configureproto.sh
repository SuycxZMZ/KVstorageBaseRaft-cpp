## 重新生成protobuf C++代码的脚本，其实也可以手动生成
## 不同版本的protobuf生成的C++代码稍有不同
## ubuntu24默认安装的版本较高，估计改了一些动态库和头文件，编译是通不过的，还没解决
#!/bin/bash
set -x

cd src/rpc
protoc *.proto --cpp_out=./

cd ../raftRpcPro
protoc *.proto --cpp_out=./

cd ../..
cd example/rpcExample
protoc *.proto --cpp_out=./

