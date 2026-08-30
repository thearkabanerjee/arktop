#include "MacProcessProvider.hpp"

#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/sysctl.h>

#include <algorithm>
#include <cstring>
#include <unistd.h>

double MacProcessProvider::getMemoryPercent(uint64_t memoryBytes) const {
    uint64_t totalMemory = 0;
    size_t size = sizeof(totalMemory);

    if (sysctlbyname("hw.memsize", &totalMemory, &size, nullptr, 0) != 0) {
        return 0.0;
    }

    if (totalMemory == 0) {
        return 0.0;
    }

    return (static_cast<double>(memoryBytes) /
            static_cast<double>(totalMemory)) * 100.0;
}

std::vector<Process> MacProcessProvider::getProcesses() {

    std::vector<Process> processes;

    // Ask macOS how many bytes are needed to store all PIDs.
    int bytes = proc_listpids(
        PROC_ALL_PIDS,
        0,
        nullptr,
        0
    );

    if (bytes <= 0) {
        return processes;
    }

    int count = bytes / static_cast<int>(sizeof(pid_t));

    std::vector<pid_t> pids(count);

    int actualBytes = proc_listpids(
        PROC_ALL_PIDS,
        0,
        pids.data(),
        bytes
    );

    if (actualBytes <= 0) {
        return processes;
    }

    int actualCount =
        actualBytes / static_cast<int>(sizeof(pid_t));

    for (int i = 0; i < actualCount; ++i) {

        pid_t pid = pids[i];

        if (pid <= 0) {
            continue;
        }

        Process process;
        process.pid = pid;

        // --------------------------------------------------
        // Process name
        // --------------------------------------------------

        char nameBuffer[PROC_PIDPATHINFO_MAXSIZE];

        int nameLength = proc_name(
            pid,
            nameBuffer,
            sizeof(nameBuffer)
        );

        if (nameLength > 0) {
            process.name = nameBuffer;
        } else {
            process.name = "?";
        }

        // --------------------------------------------------
        // Process task information
        // --------------------------------------------------

        proc_taskinfo taskInfo;

        int taskInfoSize = proc_pidinfo(
            pid,
            PROC_PIDTASKINFO,
            0,
            &taskInfo,
            sizeof(taskInfo)
        );

        if (taskInfoSize == sizeof(taskInfo)) {

            process.memoryBytes =
                taskInfo.pti_resident_size;

            process.cpuTime =
                taskInfo.pti_total_user +
                taskInfo.pti_total_system;

            process.memoryPercent =
                getMemoryPercent(process.memoryBytes);
        }

        // --------------------------------------------------
        // Process BSD information
        // --------------------------------------------------

        proc_bsdinfo bsdInfo;

        int bsdInfoSize = proc_pidinfo(
            pid,
            PROC_PIDTBSDINFO,
            0,
            &bsdInfo,
            sizeof(bsdInfo)
        );

        if (bsdInfoSize == sizeof(bsdInfo)) {

            switch (bsdInfo.pbi_status) {

                case SIDL:
                    process.state = "IDLE";
                    break;

                case SRUN:
                    process.state = "RUN";
                    break;

                case SSLEEP:
                    process.state = "SLEEP";
                    break;

                case SSTOP:
                    process.state = "STOP";
                    break;

                case SZOMB:
                    process.state = "ZOMB";
                    break;

                default:
                    process.state = "?";
                    break;
            }
        }

        processes.push_back(process);
    }

    return processes;
}
