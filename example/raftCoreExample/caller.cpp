#include <cstdio>
#include "raftClerk/clerk.h"
#include "spdlog/spdlog.h"

const int test_count = 500;

int main() {
    signal(SIGPIPE, SIG_IGN);
    Clerk client;
    client.Init("test.conf");
    
    int tmp = test_count;
    while (tmp--) {
        client.Append("x", std::to_string(tmp));
        std::string get1 = client.Get("x");
        spdlog::info("get return : {}", get1);
    }
    return 0;
}