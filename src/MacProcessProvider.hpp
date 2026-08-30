#pragma once

#include "ProcessProvider.hpp"

class MacProcessProvider : public ProcessProvider {
public:
    std::vector<Process> getProcesses() override;

private:
    double getMemoryPercent(uint64_t memoryBytes) const;
};
