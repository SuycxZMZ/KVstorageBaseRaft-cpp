#include "util.h"
#include <chrono>
#include <random>
#include "config.h"

std::chrono::_V2::system_clock::time_point now() { return std::chrono::high_resolution_clock::now(); }

std::chrono::milliseconds getRandomizedElectionTimeout() {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(minRandomizedElectionTime, maxRandomizedElectionTime);

    return std::chrono::milliseconds(dist(rng));
}

void DPrintf([[maybe_unused]] const char *format, ...) {
#ifdef Dprintf_Debug
    // 获取当前的日期，然后取日志信息，写入相应的日志文件当中 a+
    time_t now = time(nullptr);
    tm *nowtm = localtime(&now);
    va_list args;
    va_start(args, format);
    // 这个效率极其低下，可以改成线程局部变量存储 [年月日 时分秒] 每次只刷新毫秒
    std::printf("[%d-%d-%d-%d-%d-%d] ", nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday, nowtm->tm_hour,
                nowtm->tm_min, nowtm->tm_sec);
    std::vprintf(format, args);
    std::printf("\n");
    va_end(args);
#endif
}

void myAssert([[maybe_unused]] bool condition, [[maybe_unused]] const std::string &message) {
#ifdef Dprintf_Debug
    if (!condition) {
        std::cerr << "Error: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
#endif
}
