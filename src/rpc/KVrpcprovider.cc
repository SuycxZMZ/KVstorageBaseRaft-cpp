#include "KVrpcprovider.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <string>

#include "sylar/rpc/rpcheader.pb.h"
// #include "common/util.h"
#include "sylar/macro.h"

void KVRpcProvider::KVRpcProviderRunInit(int nodeIndex, short port) {
    // 获取可用ip
    char *ipC;
    char hname[128];
    struct hostent *hent;
    gethostname(hname, sizeof(hname));
    hent = gethostbyname(hname);
    for (int i = 0; hent->h_addr_list[i]; i++) {
        ipC = inet_ntoa(*(struct in_addr *)(hent->h_addr_list[i]));  // IP地址
    }
    std::string ip = std::string(ipC);
    // 写入文件 "test.conf"
    std::string node = "node" + std::to_string(nodeIndex);
    std::ofstream outfile;
    outfile.open("test.conf", std::ios::app);  // 打开文件并追加写入
    if (!outfile.is_open()) {
        std::cout << "打开文件失败！" << std::endl;
        exit(EXIT_FAILURE);
    }
    outfile << node + "ip=" + ip << std::endl;
    outfile << node + "port=" + std::to_string(port) << std::endl;
    outfile.close();
    m_nodeIndex = nodeIndex;
    m_ipPort = ip + ":" + std::to_string(port);
}

void KVRpcProvider::ToRun() {
    sylar::TcpServer::ptr server(new sylar::rpc::RpcTcpServer(this));
    auto addr = sylar::Address::LookupAny(m_ipPort);
    SYLAR_ASSERT(addr);
    std::vector<sylar::Address::ptr> addrs;
    addrs.push_back(addr);
    std::vector<sylar::Address::ptr> fails;
    while(!server->bind(addrs, fails)) {
        sleep(2);
    }
    std::cout << "bind success, " << m_ipPort << std::endl;

    // 开启 tcpserver
    server->start();
    
    // [FIXME]
    // 由于 tcpserver 是基于协程的，本函数如果不sleep，执行完就退出了，
    // 创建的临时server就会被销毁，这里先sleep一下
    // 其实这个sleep是被hook的，协程会被切走干正事，5秒切一下相当于心跳了
    // 也不会有太大消耗，还没想到特别好的办法
    while (m_isrunning) {
        sleep(5);
    }
}

void KVRpcProvider::Run() {
    m_isrunning = true;
    m_iom.schedule(std::bind(&KVRpcProvider::ToRun, this));
}

KVRpcProvider::~KVRpcProvider() {
    std::cout << "[func - RpcProvider::~RpcProvider()]: ip和port信息：" << m_ipPort << std::endl;
    m_isrunning = false;
    m_iom.stop();
}
