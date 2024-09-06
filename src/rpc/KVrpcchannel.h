#ifndef KVRPCCHANNEL_H
#define KVRPCCHANNEL_H

#include <google/protobuf/service.h>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <string>
#include <mutex>
#include "common/config.h"

using namespace std;

// 真正负责发送和接受的前后处理工作
//  如消息的组织方式，向哪个节点发送等等
class KVrpcChannel : public google::protobuf::RpcChannel {

public:
    // 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据数据序列化和网络发送 那一步
    void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request, google::protobuf::Message *response,
                    google::protobuf::Closure *done) override;
    KVrpcChannel(const std::string& ip, short port, bool connectNow = true, 
                 int sendTimeout = HeartBeatTimeout, int recvTimeout = HeartBeatTimeout);

private:
    int m_clientFd;
    const std::string m_ip;  // 保存ip和端口，如果断了可以尝试重连
    const uint16_t m_port;

    int m_sendTimeout;
    int m_recvTimeout;

    std::mutex m_mtx;

    /// @brief 连接ip和端口,并设置m_clientFd
    /// @param ip ip地址，本机字节序
    /// @param port 端口，本机字节序
    /// @return 成功返回空字符串，否则返回失败信息
    bool newConnect(const char *ip, uint16_t port, string *errMsg);
};

#endif  // KVRPCCHANNEL_H