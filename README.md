# grpcbased分支

[集成rpc服务的sylar框架仓库及说明](https://github.com/SuycxZMZ/sylar-from-suycx)

## [原KVstorageBaseRaft-cpp项目简介](docs/README.md)

## [main分支](https://github.com/SuycxZMZ/KVstorageBaseRaft-cpp/tree/main)

## [sylarbased分支](https://github.com/SuycxZMZ/KVstorageBaseRaft-cpp/tree/sylarbased)

每个分支都有改进点，细节见分支主页readme

## 使用gRPC重构版的代码改进

- raft核心代码注释补全，[主干详细代码执行流说明](docs/项目解析.md)
- [main分支](https://github.com/SuycxZMZ/KVstorageBaseRaft-cpp/tree/main)重写muduo网络库，主要组件均已实现，支持更简单好用的异步日志系统。[日志系统可以单独剥离](https://github.com/SuycxZMZ/symlog)。
- [sylarbased分支](https://github.com/SuycxZMZ/KVstorageBaseRaft-cpp/tree/sylarbased)基于sylar网络库的实现，比main分支更优雅，细节可以跳转过去看
- [rpc详细解析](https://github.com/SuycxZMZ/MpRPC-Cpp)，这个我画的是muduo作为底层的结构，其实sylar也差不多，最主要的是重写handleClient，在muduo中是onMessage
- AE和投票信息的的发送使用`boost`线程池处理，避免频繁创建销毁大量线程。
- `electionTimeOutTicker`和`leaderHearBeatTicker`两个定时任务使用`boost::asio`协程执行，节省线程切换的开销
- 优化代码组织结构，移除多余的include文件夹，更清爽的cmake代码结构，减少重复编译和路径污染
- 引入开源库[cpp-channel](https://github.com/andreiavrammsd/cpp-channel)代替原本的手搓阻塞队列，golang风格，更优雅(只在原版代码中添加了超时弹出的接口)
- 引入开源库[Defer-C++](https://github.com/Neargye/scope_guard)代替原本手搓的DEFER，该库主要使用RAII手法封了一套宏，使用简单，写的也比较规范
- gRPC使用Http2.0作为网络传输协议，支持流式处理，多个流可以复用同一条连接
- stub是线程安全的，对于raft层来说，多个线程可以安全调用同一个stub
- 支持超时和重试，代码更加高效和简洁，支持负载均衡和服务发现，配置起来更加优雅
- gitbub这个东西也是个纯**，手机验证不支持中国大陆，最新的代码在[gitee](https://gitee.com/suycx/KVstorageBaseRaft-cpp)

## 使用

1.库准备

- 安装[grpc](https://www.llfc.club/category?catid=225RaiVNI8pFDD5L4m807g7ZwmF#!aid/2TIG572uTKxQxned7LCk8KoulfL)，从官网安装时，子模块非常难下载，这个版本是grpc1.34版，配套的是protobuf3.13
- `boost`：`sudo apt-get install libboost-dev libboost-test-dev libboost-all-dev`

2.编译

```shell
## Debug版
bash build.sh DEBUG

## Release版
bash build.sh RELEASE
```

之后在目录bin就有对应的可执行文件生成：

- `callerMain`(consumer)
- `raftCoreRun`(provider)
  
注意先运行provider，再运行consumer
运行时在bin目录下`./raftCoreRun -n 5 -f test.conf`，再开一个终端`./callerMain`，启动raft集群和测试客户端代码

## 节点故障情况模拟

```shell
# raftCoreRun 跑起来之后可以查看几个节点的子进程
ps -aux
```

![docs/images/raft-fail.png](docs/images/raft-fail.png)

```shell
# 1. 
# 输出如上图所示，一般第一个进程是父进程，在后面几个中随机抽一个，杀掉
kill -9 <pid>
# 在运行 raftCoreRun 的终端还可以看到剩下的节点在继续运行，运行caller还可以正常工作，集群正常

# 2. 
# 也可以通过暂停进程来查看，
# 不过这个caller端的现象现在没改，caller会连接所有节点，暂停的话会有一个阻塞，卡住动不了，但是选举过程还是可以观察的
# 暂停进程
kill -19 <pid>
# 恢复进程
kill -18 <pid>

```

## Docs

- [项目解析](docs/项目解析.md)
- [tinymuduo](https://github.com/SuycxZMZ/tiny-muduo)
- [基于tinymuduo的RPC框架](https://github.com/SuycxZMZ/MpRPC-Cpp)
- [协程框架](https://github.com/SuycxZMZ/sylar-from-suycx)
- [恋恋风尘的boostAsio和grpc讲解](https://llfc.club/category?catid=225RaiVNI8pFDD5L4m807g7ZwmF#!aid/2LfzYBkRCfdEDrtE6hWz8VrCLoS)

## TODO

**待改善**：

- [x] `electionTimeOutTicker`和`leaderHearBeatTicker`任务使用boost协程实现
- [ ] docker运行环境以及对应的运行脚本
- [ ] 切片集群的实现，类似于redis-cluster(可能会做)
  
## 参考&&致谢

https://github.com/chenshuo/muduo
https://programmercarl.com/other/kstar.html
https://github.com/youngyangyang04/KVstorageBaseRaft-cpp
https://blog.csdn.net/T_Solotov/article/details/124044175
https://zhuanlan.zhihu.com/p/636581210
https://github.com/Shangyizhou/A-Tiny-Network-Library
https://www.cnblogs.com/tuilk/p/16793625.html
https://github.com/andreiavrammsd/cpp-channel
https://github.com/Neargye/scope_guard
https://llfc.club/
