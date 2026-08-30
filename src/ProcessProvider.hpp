#pragma once

#include "Process.hpp"

#include <vector>

class ProcessProvider {
public:
    virtual ~ProcessProvider() = default;

    virtual std::vector<Process> getProcesses() = 0;
};
