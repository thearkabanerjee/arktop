#pragma once

#include <string>

struct Process {
    int pid = 0;
    std::string name;
    std::string state;

    double cpuPercent = 0.0;
    double memoryPercent = 0.0;
    uint64_t memoryBytes = 0;

    uint64_t cpuTime = 0;
};
