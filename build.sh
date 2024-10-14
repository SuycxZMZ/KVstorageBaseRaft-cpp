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

# 调用配置 proto 文件的脚本
sh configureproto.sh

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
