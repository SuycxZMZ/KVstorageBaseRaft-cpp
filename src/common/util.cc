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


