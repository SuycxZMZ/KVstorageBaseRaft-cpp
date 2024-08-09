# sylarbased分支

[集成rpc服务的sylar框架仓库及说明](https://github.com/SuycxZMZ/sylar-from-suycx)

## [原KVstorageBaseRaft-cpp项目简介](docs/README.md)

## 改进

- raft核心代码注释补全，[主干详细代码执行流说明](docs/项目解析.md)
- 使用集成rpc服务的sylar网络框架重构

## 使用

1.库准备

- 安装重构的muduo网络库，[tinymuduo网络库](https://github.com/SuycxZMZ/tiny-muduo)
- boost
- protoc

2.安装说明

- protoc，本地版本为3.12.4，ubuntu22使用`sudo apt-get install protobuf-compiler libprotobuf-dev`默认安装的版本在这个附近，大概率能用，项目的tools-package中也放了3.12.4的源码，编译安装就行，如果报错直接百度，很好解决。ubuntu24.04默认安装的是3.21.1，编译会报错，目前没有解决。
- boost，`sudo apt-get install libboost-dev libboost-test-dev libboost-all-dev`，raft核心代码中使用boost进行日志序列化生成快照。

3.编译

```shell
# 安装tinymuduo
git clone https://github.com/SuycxZMZ/tiny-muduo.git
cd tiny-muduo
# 自动编译安装脚本，安装完成后会打印安装完成信息
sudo bash autobuild.sh

# 注意，在编译KVRaft项目之前，最好执行一下根目录下的`configureproto.sh`脚本，这个脚本会自动生成proto文件对应的.pb.h和.pb.cc文件，覆盖原本的文件
sudo bash configureproto.sh

# KVRaft本项目编译运行
mkdir cmake-build-debug
cd cmake-build-debug
cmake ..
make -j4
```

之后在目录bin就有对应的可执行文件生成：

- `callerMain`(consumer)
- `raftCoreRun`(provider)
  
注意先运行provider，再运行consumer
运行时在bin目录下提供了一个`test.config`配置文件，按照两个可执行文件打印的 help 信息进行加载，启动raft集群和测试客户端代码

## Docs

- [项目解析](docs/项目解析.md)
- [tinymuduo](https://github.com/SuycxZMZ/tiny-muduo)
- [基于tinymuduo的RPC框架](https://github.com/SuycxZMZ/MpRPC-Cpp)
- [协程框架](https://github.com/SuycxZMZ/sylar-from-suycx)

## TODO

**待改善**：

- 目前只是稍微搓一个能正常跑测试的版本，基本已经调试通过
- 之前粘包的问题分析：
  - 正常情况下的长连接，如果调用方单线程调用，基本不会粘包，因为每次发送完都要recv阻塞等待结果，也就是串行调用接收，不存在粘包的情况
  - 在本项目中，使用muduo库的分支上，每一次AE的发送都会起一个线程，这样发送基本上可以立即执行，发送心跳和选举信息基本上也不会积压
  - 使用sylar框架的分支上，一开始是把所有的发送交给协程调度器，但是，这么做的话，每个rpc调用调度器要在send和recv上花费不少时间，随着时间推移，调度器上的任务会越来越多，设计不好先不说，跑一会就会挂。并且如果leader节点连着发了两个AE包，都没有立刻发送，这时如果两个线程同时写socket，直接就未定义了。现在，把发送心跳和选举的任务又交给了临时起的线程，能改善挺多，但是大量临时线程频繁的创建销毁也是个不小的消耗，之后可以让raft节点带一个线程池，可以减轻这个负担。

## 参考&&致谢

https://github.com/chenshuo/muduo
https://programmercarl.com/other/kstar.html
https://github.com/youngyangyang04/KVstorageBaseRaft-cpp
https://blog.csdn.net/T_Solotov/article/details/124044175
https://zhuanlan.zhihu.com/p/636581210
https://github.com/Shangyizhou/A-Tiny-Network-Library
https://www.cnblogs.com/tuilk/p/16793625.html
