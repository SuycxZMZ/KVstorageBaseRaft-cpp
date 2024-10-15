#!/bin/bash

# 启用调试输出
set -x

# 解析命令行参数
BUILD_TYPE="DEBUG"
BUILD_DIR="cmake-build-debug"

if [[ "$1" == "RELEASE" ]]; then
    BUILD_TYPE="RELEASE"
    BUILD_DIR="cmake-build-release"
fi

# 检查是否已经有 spdlog
SPDLOG_DIR="thirdParty/spdlog"
SPDLOG_VERSION="v1.14.0"

if [ ! -d "$SPDLOG_DIR" ]; then
    echo "spdlog not found. Cloning spdlog $SPDLOG_VERSION..."
    git clone --branch $SPDLOG_VERSION --depth 1 https://github.com/gabime/spdlog.git "$SPDLOG_DIR"
    
    # 进入 spdlog 目录并编译
    mkdir -p "$SPDLOG_DIR/build"
    cd "$SPDLOG_DIR/build"
    cmake ..
    make -j2
    cd -
fi


# 调用配置 proto 文件的脚本
cd ./src/raftRpcPro
protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) raftRPC.proto
protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) kvServerRPC.proto
cd ../..


# 检查是否存在构建目录
if [ ! -d "$BUILD_DIR" ]; then
    mkdir "$BUILD_DIR"
else
    rm -rf "$BUILD_DIR/*"
fi

# 进入构建目录
cd "$BUILD_DIR"

# 根据构建类型生成 Makefile
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# 编译项目
make -j2
