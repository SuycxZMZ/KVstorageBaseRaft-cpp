#include "KVrpcchannel.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <string>
#include "common/util.h"
#include "sylar/rpc/rpcchannel.h"
#include "sylar/rpc/rpcheader.pb.h"

// 所有通过stub代理对象调用的rpc方法，都会走到这里了，
// 统一通过rpcChannel来调用方法
// 统一做rpc方法调用的  数据数据序列化  和  网络发送
void KVrpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                              google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                              google::protobuf::Message* response, google::protobuf::Closure* done) {
    std::unique_lock<std::mutex> lock(m_mtx);
    if (m_clientFd == -1) {
        std::string errMsg;
        bool rt = newConnect(m_ip.c_str(), m_port, &errMsg);
        if (!rt) {
            DPrintf("[func-MprpcChannel::CallMethod]重连接ip：{%s} port{%d}失败", m_ip.c_str(), m_port);
            controller->SetFailed(errMsg);
            return;
        } else {
            DPrintf("[func-MprpcChannel::CallMethod]连接ip：{%s} port{%d}成功", m_ip.c_str(), m_port);
        }
    }
    lock.unlock();  // 判断完解个锁，下面的序列化根据接口参数来的，不涉及类内临界资源，不用拿锁

    // 获取服务名和方法名
    const google::protobuf::ServiceDescriptor* service_desc = method->service();
    std::string service_name = service_desc->name();
    const std::string &method_name = method->name();

    // 序列化 request 请求参数
    std::string args_str;
    if (!request->SerializeToString(&args_str))
    {
        controller->SetFailed("Serialize request args_str error !!!");
        // std::cout << "Serialize request args_str error !!!" << std::endl;
        return;
    }
    uint32_t args_size = args_str.size();

    // set rpcheader
    sylar_rpc::RpcHeader rpcheader;
    rpcheader.set_service_name(service_name);
    rpcheader.set_method_name(method_name);
    rpcheader.set_args_size(args_size);

    // 序列化 rpcheader
    std::string rpcheader_str;
    if (!rpcheader.SerializeToString(&rpcheader_str))
    {
        controller->SetFailed("Serialize request rpcheader error !!!");
        // std::cout << "Serialize request rpcheader error !!!" << std::endl;
        return;
    }
    uint32_t send_all_size = SEND_RPC_HEADERSIZE + rpcheader_str.size() + args_str.size();
    uint32_t rpcheader_size = rpcheader_str.size();

    // 组装 rpc 请求帧
    std::string rpc_send_str;
    // 包大小
    rpc_send_str += std::string((char*)&send_all_size, 4);
    // rpcheader_size
    rpc_send_str += std::string((char*)&rpcheader_size, 4);
    // rpcheader
    rpc_send_str += rpcheader_str;
    // args
    rpc_send_str += args_str;

    // // [DEBUG INFO]
    // std::cout << "------------- send info ----------- \n" 
    //          << "header_size : " << rpcheader_size << "\n"
    //          << "service_name : " << service_name << "\n"
    //          << "method_name : " << method_name << "\n"
    //          << "args_size : " << args_size << "\n"
    //          << "args_str : " << args_str << "\n"
    //          << "------------- send info ----------- \n";

    // --------------------------- 加锁保护 --------------------------- //
    lock.lock(); /// 发送接收当成一个原子过程，一个rpc调用要么一次成功，要么失败 
                 /// TODO：这样的写法并发度一般，但是由于节点故障的情况并不常见
                 /// 正常情况下不会有过多的超时选举，所以，
                 /// 同一个socket上连着两次发送使用一把锁隔开还是比较合理的
                 /// 一般不会出现非常严重的锁争用

                 /// TODO: 目前这种写法不能防止串话，可以再rpc请求和发送上加一个序列号标记，
                 /// 对上的才处理，对不上再收几个包

    /// 发送rpc请求
    /// 失败会重试连接再发送，重试连接失败会直接return
    /// TODO 这里要处理信号，如果对端被kill 9，杀掉，或者正常关闭
    /// 则这里再发送会收到SIGPIPE，需要忽略处理，send返回-1时关闭。返回值未-1，错误码为 ECONNRESET
    ///  在非调试版本可以给send 的最后一个参数加上 MSG_NOSIGNAL 忽略信号
    while (send(m_clientFd, rpc_send_str.c_str(), rpc_send_str.size(), 0) < 0) {
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! errno:%d", errno);
        
        // EAGAIN EWOULDBLOCK 代表超时，没超时的情况，现在统一按照断开来处理
        if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
            close(m_clientFd);
            m_clientFd = -1;
            std::string errMsg;
            std::cerr << "尝试重新连接，对方ip：" << m_ip << " 对方端口" << m_port << std::endl;
            bool rt = newConnect(m_ip.c_str(), m_port, &errMsg);
            if (!rt) {
                controller->SetFailed(errMsg);
                return;
            }
        }
        // 发送失败就不能等接收了，应该设置错误立马返回
        controller->SetFailed(errtxt);
        return;
    }

    // 接收rpc请求的响应值
    char recv_buf[1024] = {0};
    ssize_t recv_size;

    /// 如果对端close或者shutdown,那么返回0
    if (-1 == (recv_size = recv(m_clientFd, recv_buf, 1024, 0))) {
        if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
            close(m_clientFd);
            m_clientFd = -1;
        }

        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }
    // 反序列化rpc调用的响应数据
    // bug：出现问题，recv_buf中遇到\0后面的数据就存不下来了，导致反序列化失败 if
    if (!response->ParseFromArray((const void*)recv_buf, static_cast<int> (recv_size))) {
        char errtxt[1050] = {0};
        sprintf(errtxt, "parse error! response_str:%s", recv_buf);
        controller->SetFailed(errtxt);
        return;
    }
}

bool KVrpcChannel::newConnect(const char* ip, uint16_t port, string* errMsg) {
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd) {
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! errno:%d", errno);
        m_clientFd = -1;
        *errMsg = errtxt;
        return false;
    }

    struct timeval timeout = {};
    timeout.tv_sec = 0;
    timeout.tv_usec = m_sendTimeout * 1000;

    // 设置发送超时时间
    if (setsockopt(clientfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "Failed to set socket send timeout\n";
        close(clientfd);
        return false;
    }


    /// TODO 这里其实有一个bug，如果前一个recv超时，则后面的recv可能会收到前一个的旧包
    /// 串行化调用不能很好解决串话问题，可以在每个rpc包里面加一个汇话id，连接初始化时初始化，之后递增
    /// 则在一个recv中接收到一个小的id之后就知道是串话了，需要丢弃。
    /// 但是具体怎么写目前还没想好，涉及到一个简单的收发协议，并且还要有时效性。

    /// 接收超时，没放在一起方便后期拓展，这两个超时可以设为不一样的值
    timeout.tv_usec = m_recvTimeout * 1000;
    if (setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "Failed to set socket send timeout\n";
        close(clientfd);
        return false;
    }

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
    // 连接rpc服务节点
    if (-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "connect fail! errno:%d", errno);
        m_clientFd = -1;
        *errMsg = errtxt;
        return false;
    }

    m_clientFd = clientfd;
    return true;
}

KVrpcChannel::KVrpcChannel(const std::string& ip, short port, bool connectNow, int sendTimeout, int recvTimeout)
        : m_ip(ip), 
        m_port(port), 
        m_clientFd(-1),
        m_sendTimeout(sendTimeout),
        m_recvTimeout(recvTimeout) 
{
    // 没有连接或者连接已经断开，那么就要重新连接呢,会一直不断地重试
    if (!connectNow) {
        return;
    }  // 可以允许延迟连接
    std::string errMsg;
    auto rt = newConnect(ip.c_str(), port, &errMsg);
    int tryCount = 3;
    while (!rt && tryCount--) {
        std::cout << "--------------------- errMsg:" << errMsg << "connect peers error ---------------" << std::endl;
        rt = newConnect(ip.c_str(), port, &errMsg);
    }
}