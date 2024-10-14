#include "raftCore/kvServer.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <sys/wait.h>

// SIGCHLD signal handler
void handle_sigchld(int sig) {
    (void)sig;  // silence unused parameter warning
    while (waitpid(-1, nullptr, WNOHANG) > 0);
}

void ShowArgsHelp() {
    std::cout << "format: command -n <nodeNum> -f <configFileName>" << std::endl;
}

int main(int argc, char **argv) {
    // Set up the SIGCHLD handler to reap zombie processes
    struct sigaction sa = {};
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // ---------------------- 读取命令参数：节点数量、写入raft节点节点信息到哪个文件 ----------------------
    if (argc < 2) {
        ShowArgsHelp();
        exit(EXIT_FAILURE);
    }
    int c = 0;
    int nodeNum = 0;
    std::string configFileName;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 29999);
    unsigned short startPort = dis(gen);
    while ((c = getopt(argc, argv, "n:f:")) != -1) {
        switch (c) {
            case 'n':
                nodeNum = std::stoi(optarg);
                break;
            case 'f':
                configFileName = optarg;
                break;
            default:
                ShowArgsHelp();
                exit(EXIT_FAILURE);
        }
    }
    std::ofstream file = std::ofstream(configFileName, std::ios::trunc);
    if (file.is_open()) {
        file.close();
        std::cout << configFileName << " 已清空" << std::endl;
    } else {
        std::cout << "无法打开 " << configFileName << std::endl;
        exit(EXIT_FAILURE);
    }

    std::ofstream outfile;
    outfile.open("test.conf", std::ios::app);  // 打开文件并追加写入
    std::string basicTestIP = "127.0.0.1";
    std::vector<short> portVec(nodeNum);
    for (int i = 0; i < nodeNum; ++i) {
        portVec[i] = startPort + static_cast<short>(i);
        outfile << "node" << std::to_string(i) << "ip=" << basicTestIP << std::endl;
        outfile << "node" << std::to_string(i) << "port=" << std::to_string(portVec[i]) << std::endl;
    }

    // ---------------------- 进程创建，测试 ----------------------
    for (int i = 0; i < nodeNum; i++) {
        short port = portVec[i];
        std::cout << "start to create raftkv node:" << i << " port:" << port << " pid:" << getpid() << std::endl;
        pid_t pid = fork();  // 创建新进程
        if (pid == 0) {
            // 如果是子进程
            signal(SIGPIPE, SIG_IGN);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            // 子进程的代码
            auto kvServer = std::make_unique<KvServer>(i, 500, configFileName, port);
            pause();  // 子进程进入等待状态
        } else if (pid > 0) {
            // 如果是父进程
            // 父进程的代码
            sleep(1);
        } else {
            // 如果创建进程失败
            std::cerr << "Failed to create child process." << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    pause();  // 父进程进入等待状态
    return 0;
}
