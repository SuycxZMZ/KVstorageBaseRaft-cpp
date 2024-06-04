#!/bin/bash
set -x

sh configureproto.sh

# 检查是否有 cmake-build-debug 目录存在，如果没有，则创建该目录。如果存在，则清除其中的内容
if [ ! -d "cmake-build-debug" ]; then
    mkdir cmake-build-debug
else
    rm -rf cmake-build-debug/*
fi

# 进入 cmake-build-debug 目录
cd cmake-build-debug

# 生成 Makefile
cmake ..

# 编译项目
make