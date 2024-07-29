#include <iostream>

// #include "mprpcapplication.h"
#include "friend.pb.h"

#include "rpc/mprpcchannel.h"
#include "rpc/mprpccontroller.h"
#include "rpc/rpcprovider.h"

int main(int argc, char **argv) {
    std::string ip = "127.0.1.1";
    short port = 7788;

    // 调用者通过stub类来调用远程方法
    fixbug::FiendServiceRpc_Stub stub(new MprpcChannel(ip, port, true)); 

    // rpc方法的请求参数
    fixbug::GetFriendsListRequest request;
    request.set_userid(1000);
    fixbug::GetFriendsListResponse response;
    MprpcController controller;

    // 长连接测试，1000次请求
    int count = 1000;
    while (count--) {
        // std::cout << " 倒数" << count << "次发起RPC请求" << std::endl;
        stub.GetFriendsList(&controller, &request, &response, nullptr);
        // RpcChannel->RpcChannel::callMethod 集中来做所有rpc方法调用的参数序列化和网络发送

        if (controller.Failed()) {
            std::cout << controller.ErrorText() << std::endl;
        } else {
            if (0 == response.result().errcode()) {
                // std::cout << "rpc GetFriendsList response success!" << std::endl;
                // int size = response.friends_size();
                // for (int i = 0; i < size; i++) {
                //   std::cout << "index:" << (i + 1) << " name:" << response.friends(i) << std::endl;
                // }
            } else {
                //  这里不是rpc失败，能过来就说明rpc收到了回发
                //  而是业务逻辑的返回值是失败
                std::cout << "rpc GetFriendsList response error : " << response.result().errmsg() << std::endl;
            }
        }
    }
    return 0;
}