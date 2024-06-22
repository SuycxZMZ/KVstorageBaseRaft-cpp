#include "KVrpcchannel.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <string>
#include "common/util.h"

/*
header_size + service_name method_name args_size + args
*/
// 所有通过stub代理对象调用的rpc方法，都会走到这里了，
// 统一通过rpcChannel来调用方法
// 统一做rpc方法调用的数据数据序列化和网络发送
void KVrpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                              google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                              google::protobuf::Message* response, google::protobuf::Closure* done) {
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

    // 获取服务名和方法名
    const google::protobuf::ServiceDescriptor* service_desc = method->service();
    std::string service_name = service_desc->name();
    std::string method_name = method->name();

    // 序列化 request 请求参数
    uint32_t args_size = 0;
    std::string args_str;
    if (!request->SerializeToString(&args_str))
    {
        controller->SetFailed("Serialize request args_str error !!!");
        // std::cout << "Serialize request args_str error !!!" << std::endl;
        return;
    }
    args_size = args_str.size();

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
    uint32_t rpcheader_size = rpcheader_str.size();

    // 组装 rpc 请求帧
    std::string rpc_send_str;
    // rpcheader_size
    rpc_send_str.insert(0, std::string((char*)&rpcheader_size, 4));
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

    // 发送rpc请求
    // 失败会重试连接再发送，重试连接失败会直接return
    // std::cout << " --------------- before stub send -------------- \n"; 
    // std::cout << "------------------- send : " << method_name << std::endl;
    while (send(m_clientFd, rpc_send_str.c_str(), rpc_send_str.size(), 0) <= 0) {
        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno:%d", errno);
        std::cout << "尝试重新连接，对方ip：" << m_ip << " 对方端口" << m_port << std::endl;
        close(m_clientFd);
        m_clientFd = -1;
        std::string errMsg;
        bool rt = newConnect(m_ip.c_str(), m_port, &errMsg);
        if (!rt) {
            controller->SetFailed(errMsg);
            return;
        }
    }
    /*
    从时间节点来说，这里将请求发送过去之后rpc服务的提供者就会开始处理，返回的时候就代表着已经返回响应了
    */

    // 接收rpc请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(m_clientFd, recv_buf, 1024, 0))) {
        close(m_clientFd);
        m_clientFd = -1;
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }
    // 反序列化rpc调用的响应数据
    // std::string response_str(recv_buf, 0, recv_size); //
    // bug：出现问题，recv_buf中遇到\0后面的数据就存不下来了，导致反序列化失败 if
    // (!response->ParseFromString(response_str))
    if (!response->ParseFromArray(recv_buf, recv_size)) {
        char errtxt[1050] = {0};
        sprintf(errtxt, "parse error! response_str:%s", recv_buf);
        controller->SetFailed(errtxt);
        // std::cout << "----------------- 响应失败 \n"; 
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

    struct sockaddr_in server_addr;
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

KVrpcChannel::KVrpcChannel(string ip, short port, bool connectNow) : m_ip(ip), m_port(port), m_clientFd(-1) {
    // 使用tcp编程，完成rpc方法的远程调用，使用的是短连接，因此每次都要重新连接上去，待改成长连接。
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