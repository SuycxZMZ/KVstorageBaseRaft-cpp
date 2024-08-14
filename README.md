# KVstorageBaseRaft-cpp

## [原KVstorageBaseRaft-cpp项目简介](docs/README.md)

## 分支说明

- 1.main分支是基于重写tinymuduo网络库为底层rpc实现的项目，把原本的项目稍微重构了一下，已经调试通过，后续慢慢优化。
- 2.sylarbased分支是基于sylar服务器框架重构的项目，rpc服务已经集成到sylar服务器框架中，重构的项目只留核心代码，简洁清爽，bug待调试。[集成rpc服务的sylar服务器框架](https://github.com/SuycxZMZ/sylar-from-suycx)。

## 改进

- raft核心代码注释补全，[主干详细代码执行流说明](docs/项目解析.md)
- 重写muduo网络库，主要组件均已实现，支持更简单好用的异步日志系统。[日志系统可以单独剥离](https://github.com/SuycxZMZ/symlog)。
- 使用重写的muduo网络库实现rpc，[详细解析](https://github.com/SuycxZMZ/MpRPC-Cpp)
- AE和投票信息的的发送使用线程池处理，避免频繁创建销毁大量线程。
- 移除fiber文件夹，直接使用sylar协程调度器调度 发送AE 和 超时选举 两个任务(注意，底层的服务端连接与接收还是muduo)
- 优化代码组织结构，移除多余的include文件夹，更清爽的cmake代码结构，减少重复编译和路径污染

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

- 添加线程池的一点思路(已经添加完毕)：
  - 线程池要支持动态伸缩。
  - leader 节点上高负荷跑的有 (n - 1) 个AE发送线程，外部客户端访问是交给协程框架来做的
  - candidate 节点上 (n - 1) 个投票线程，在失败之后重新回归 follower，线程销毁; 成功之后，这几个线程应该被 AE线程复用
  - 另外对于 follower 可能会临时起 InstallSnapshot 线程，发送快照应属于不常见现象，健康的状态是发送增量日志，可以单独起线程
  - 每个节点上跑 applierTicker， 定期向状态机写入日志，将已提交但未应用的日志应用，加入到 applyChan，活跃度一般，可以单独起线程
  - 所以正常网络情况下，每个节点上线程池最大限制为 2n 是足够充裕的
- 在网络很差的情况下，如果对端长时间不可写，可能会导致 AE 任务积压，多个线程发送一个socket，造成未定义(目前只做了任务队列中的超时处理。如果已经开始，但卡住了，这个还没想好怎么写)
  - 所以，对于加入线程池的任务，AE 投票 应该有一个超时时间限制，如果在一定的时间内执行不完，就直接退出
  - 对于AE任务，这个时间应该与心跳间隔相当
  - 对于投票任务，这个时间可以在 心跳间隔与选举超时时间之间

## 参考&&致谢

https://github.com/chenshuo/muduo
https://programmercarl.com/other/kstar.html
https://github.com/youngyangyang04/KVstorageBaseRaft-cpp
https://blog.csdn.net/T_Solotov/article/details/124044175
https://zhuanlan.zhihu.com/p/636581210
https://github.com/Shangyizhou/A-Tiny-Network-Library
https://www.cnblogs.com/tuilk/p/16793625.html
