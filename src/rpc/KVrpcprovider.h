#ifndef KVRPCPROVIDER_H
#define KVRPCPROVIDER_H

#include <google/protobuf/descriptor.h>
#include <string>
#include "sylar/rpc/rpcprovider.h"

// 框架提供的专门发布rpc服务的网络对象类
// todo:现在rpc客户端变成了 长连接，因此rpc服务器这边最好提供一个定时器，用以断开很久没有请求的连接。
// todo：为了配合这个，那么rpc客户端那边每次发送之前也需要真正的
class KVRpcProvider : public sylar::rpc::RpcProvider {
public:
    KVRpcProvider(sylar::IOManager::ptr iom);

    /// @brief 开启 tcpserver
    virtual void InnerStart() override;

    /// @brief 初始化rpc节点信息
    /// @param nodeIndex 节点索引
    /// @param port 端口号
    void KVRpcProviderRunInit(int nodeIndex, short port);
public:
    ~KVRpcProvider();
private:
    // 节点索引
    int m_nodeIndex;
    // 节点ip
    std::string m_ipPort;
};


#endif