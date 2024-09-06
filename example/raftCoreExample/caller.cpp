#include <cstdio>
#include "raftClerk/clerk.h"

const int test_count = 500;

int main() {
    signal(SIGPIPE, SIG_IGN);
    Clerk client;
    client.Init("test.conf");
    
    int tmp = test_count;
    while (tmp--) {
        // client.Put("x", std::to_string(tmp));
        client.Append("x", std::to_string(tmp));
        std::string get1 = client.Get("x");
        printf("get return :{%s}\r\n", get1.c_str());
    }
    return 0;
}