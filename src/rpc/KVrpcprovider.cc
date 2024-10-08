#include "KVrpcprovider.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fstream>
#include <string>
#include <utility>
#include "sylar/macro.h"

KVRpcProvider::KVRpcProvider([[maybe_unused]] sylar::IOManager::ptr _iom) :
    sylar::rpc::RpcProvider(std::move(_iom)),
    m_nodeIndex() {}

void KVRpcProvider::KVRpcProviderRunInit(int nodeIndex, short port) {
    // 获取可用ip
    [[maybe_unused]] char *ipC;
    char hname[128];
    struct hostent *hent;
    gethostname(hname, sizeof(hname));
    hent = gethostbyname(hname);
    for (int i = 0; hent->h_addr_list[i]; i++) {
        inet_ntoa(*(struct in_addr *)(hent->h_addr_list[i]));  // IP地址
    }
    // 测试[FIXME]
    // std::string ip = std::string(ipC);
    std::string ip = "0.0.0.0";
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

void KVRpcProvider::InnerStart() {
    m_isRunning = true;
    std::cout << " --------------- 绑定到子类 InnerStart() --------------- \n";
    auto addr = sylar::Address::LookupAny(m_ipPort);
    std::vector<sylar::Address::ptr> addrs;
    addrs.push_back(addr);
    std::vector<sylar::Address::ptr> fails;
    while(!m_tcpServer->bind(addrs, fails)) {
        sleep(2);
    }
    std::cout << " --------------- bind success, " << m_ipPort << " at:" << getpid() << "---------------" << std::endl;

    // 开启 tcpserver
    m_tcpServer->start();
}

KVRpcProvider::~KVRpcProvider() {
    std::cout << "[func - RpcProvider::~RpcProvider()]: ip和port信息：" << m_ipPort << std::endl;
    m_isRunning = false;
    m_iom->stop();
}
