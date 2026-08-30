#include "MacProcessProvider.hpp"
#include "Terminal.hpp"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <sys/sysctl.h>
#include <unistd.h>

#include <mach/mach.h>
#include <mach/mach_host.h>


// ============================================================
// SYSTEM STATISTICS
// ============================================================

struct SystemStats {

    double cpuPercent = 0.0;
    double ramPercent = 0.0;
    double swapPercent = 0.0;

    uint64_t usedMemory = 0;
    uint64_t totalMemory = 0;

    uint64_t usedSwap = 0;
    uint64_t totalSwap = 0;
};


// ============================================================
// SORTING
// ============================================================

enum class SortMode {

    CPU,
    MEMORY,
    PID,
    NAME
};


// ============================================================
// MEMORY FORMATTING
// ============================================================

std::string formatMemory(uint64_t bytes) {

    double mb =
        static_cast<double>(bytes) /
        (1024.0 * 1024.0);

    std::ostringstream output;

    if (mb >= 1024.0) {

        double gb =
            mb / 1024.0;

        output
            << std::fixed
            << std::setprecision(1)
            << gb
            << " GB";

    } else {

        output
            << std::fixed
            << std::setprecision(1)
            << mb
            << " MB";
    }

    return output.str();
}


// ============================================================
// CPU COUNT
// ============================================================

int getCPUCount() {

    int cpuCount = 1;

    size_t size =
        sizeof(cpuCount);

    sysctlbyname(
        "hw.logicalcpu",
        &cpuCount,
        &size,
        nullptr,
        0
    );

    if (cpuCount < 1) {

        cpuCount = 1;
    }

    return cpuCount;
}


// ============================================================
// PROCESS CPU TRACKER
// ============================================================

class CPUTracker {

private:

    std::unordered_map<int, uint64_t>
        previousCPU;

public:

    void update(
        std::vector<Process>& processes,
        double elapsedSeconds,
        int cpuCount
    ) {

        if (elapsedSeconds <= 0) {

            return;
        }

        for (auto& process : processes) {

            auto found =
                previousCPU.find(
                    process.pid
                );

            // First time seeing this process
            if (found ==
                previousCPU.end()) {

                previousCPU[
                    process.pid
                ] = process.cpuTime;

                process.cpuPercent = 0.0;

                continue;
            }

            uint64_t previous =
                found->second;

            uint64_t current =
                process.cpuTime;

            uint64_t delta = 0;

            if (current >= previous) {

                delta =
                    current - previous;
            }

            previousCPU[
                process.pid
            ] = current;

            // CPU time is measured in nanoseconds.
            double cpuSeconds =
                static_cast<double>(delta) /
                1'000'000'000.0;

            process.cpuPercent =
                (
                    cpuSeconds /
                    elapsedSeconds
                ) *
                100.0 /
                static_cast<double>(cpuCount);
        }
    }
};


// ============================================================
// SYSTEM CPU TRACKER
// ============================================================

class SystemCPUTracker {

private:

    uint64_t previousUser = 0;
    uint64_t previousSystem = 0;
    uint64_t previousIdle = 0;
    uint64_t previousNice = 0;

    bool initialized = false;

public:

    double getCPUUsage() {

        host_cpu_load_info_data_t cpuInfo;

        mach_msg_type_number_t count =
            HOST_CPU_LOAD_INFO_COUNT;

        kern_return_t result =
            host_statistics(
                mach_host_self(),
                HOST_CPU_LOAD_INFO,
                reinterpret_cast<host_info_t>(
                    &cpuInfo
                ),
                &count
            );

        if (result != KERN_SUCCESS) {

            return 0.0;
        }

        uint64_t user =
            cpuInfo.cpu_ticks[
                CPU_STATE_USER
            ];

        uint64_t system =
            cpuInfo.cpu_ticks[
                CPU_STATE_SYSTEM
            ];

        uint64_t idle =
            cpuInfo.cpu_ticks[
                CPU_STATE_IDLE
            ];

        uint64_t nice =
            cpuInfo.cpu_ticks[
                CPU_STATE_NICE
            ];


        // First measurement
        if (!initialized) {

            previousUser = user;
            previousSystem = system;
            previousIdle = idle;
            previousNice = nice;

            initialized = true;

            return 0.0;
        }


        uint64_t userDelta =
            user - previousUser;

        uint64_t systemDelta =
            system - previousSystem;

        uint64_t idleDelta =
            idle - previousIdle;

        uint64_t niceDelta =
            nice - previousNice;


        uint64_t totalDelta =
            userDelta +
            systemDelta +
            idleDelta +
            niceDelta;


        previousUser = user;
        previousSystem = system;
        previousIdle = idle;
        previousNice = nice;


        if (totalDelta == 0) {

            return 0.0;
        }


        uint64_t busyDelta =
            totalDelta - idleDelta;


        return (
            static_cast<double>(
                busyDelta
            ) /
            static_cast<double>(
                totalDelta
            )
        ) * 100.0;
    }
};


// ============================================================
// GET SYSTEM STATISTICS
// ============================================================

SystemStats getSystemStats(
    SystemCPUTracker& cpuTracker
) {

    SystemStats stats;


    // ========================================================
    // CPU
    // ========================================================

    stats.cpuPercent =
        cpuTracker.getCPUUsage();


    // ========================================================
    // TOTAL RAM
    // ========================================================

    size_t size =
        sizeof(stats.totalMemory);

    sysctlbyname(
        "hw.memsize",
        &stats.totalMemory,
        &size,
        nullptr,
        0
    );


    // ========================================================
    // VM STATISTICS
    // ========================================================

    vm_size_t pageSize;

    host_page_size(
        mach_host_self(),
        &pageSize
    );


    vm_statistics64_data_t vmStats;

    mach_msg_type_number_t count =
        HOST_VM_INFO64_COUNT;


    kern_return_t result =
        host_statistics64(
            mach_host_self(),
            HOST_VM_INFO64,
            reinterpret_cast<host_info64_t>(
                &vmStats
            ),
            &count
        );


    if (result == KERN_SUCCESS) {

        uint64_t active =
            static_cast<uint64_t>(
                vmStats.active_count
            ) *
            pageSize;


        uint64_t wired =
            static_cast<uint64_t>(
                vmStats.wire_count
            ) *
            pageSize;


        uint64_t compressed =
            static_cast<uint64_t>(
                vmStats.compressor_page_count
            ) *
            pageSize;


        stats.usedMemory =
            active +
            wired +
            compressed;


        if (stats.totalMemory > 0) {

            stats.ramPercent =
                (
                    static_cast<double>(
                        stats.usedMemory
                    ) /
                    static_cast<double>(
                        stats.totalMemory
                    )
                ) *
                100.0;
        }
    }


    // ========================================================
    // SWAP
    // ========================================================

    struct xsw_usage swapUsage;

    size =
        sizeof(swapUsage);


    if (
        sysctlbyname(
            "vm.swapusage",
            &swapUsage,
            &size,
            nullptr,
            0
        ) == 0
    ) {

        stats.totalSwap =
            swapUsage.xsu_total;

        stats.usedSwap =
            swapUsage.xsu_used;


        if (stats.totalSwap > 0) {

            stats.swapPercent =
                (
                    static_cast<double>(
                        stats.usedSwap
                    ) /
                    static_cast<double>(
                        stats.totalSwap
                    )
                ) *
                100.0;
        }
    }


    return stats;
}


// ============================================================
// DRAW BAR
// ============================================================

void drawBar(
    const char* label,
    double percentage,
    int width
) {

    if (percentage < 0.0) {

        percentage = 0.0;
    }

    if (percentage > 100.0) {

        percentage = 100.0;
    }


    int filled =
        static_cast<int>(
            (
                percentage /
                100.0
            ) *
            width
        );


    std::printf(
        "%s [",
        label
    );


    for (
        int i = 0;
        i < width;
        ++i
    ) {

        if (i < filled) {

            std::printf("█");

        } else {

            std::printf("░");
        }
    }


    std::printf(
        "] %6.1f%%",
        percentage
    );
}


// ============================================================
// SORT PROCESSES
// ============================================================

void sortProcesses(
    std::vector<Process>& processes,
    SortMode mode
) {

    switch (mode) {

        case SortMode::CPU:

            std::sort(
                processes.begin(),
                processes.end(),

                [](const Process& a,
                   const Process& b) {

                    return
                        a.cpuPercent >
                        b.cpuPercent;
                }
            );

            break;


        case SortMode::MEMORY:

            std::sort(
                processes.begin(),
                processes.end(),

                [](const Process& a,
                   const Process& b) {

                    return
                        a.memoryBytes >
                        b.memoryBytes;
                }
            );

            break;


        case SortMode::PID:

            std::sort(
                processes.begin(),
                processes.end(),

                [](const Process& a,
                   const Process& b) {

                    return
                        a.pid <
                        b.pid;
                }
            );

            break;


        case SortMode::NAME:

            std::sort(
                processes.begin(),
                processes.end(),

                [](const Process& a,
                   const Process& b) {

                    return
                        a.name <
                        b.name;
                }
            );

            break;
    }
}


// ============================================================
// DRAW HEADER
// ============================================================

void drawHeader(
    Terminal& terminal,
    const SystemStats& stats,
    SortMode sortMode,
    int processCount
) {

    // ========================================================
    // SORT NAME
    // ========================================================

    const char* sortName;

    switch (sortMode) {

        case SortMode::CPU:

            sortName = "CPU";

            break;


        case SortMode::MEMORY:

            sortName = "Memory";

            break;


        case SortMode::PID:

            sortName = "PID";

            break;


        case SortMode::NAME:

            sortName = "Name";

            break;


        default:

            sortName = "Unknown";

            break;
    }


    // ========================================================
    // TITLE
    // ========================================================

    terminal.moveCursor(
        1,
        1
    );


    std::printf(
        "\033[1;36m"
        " ARKTOP"
        "\033[0m"
    );


    std::printf(
        "   Processes: %d   |   Sort: %s",
        processCount,
        sortName
    );


    std::printf(
        "\033[K"
    );


    // ========================================================
    // CPU BAR
    // ========================================================

    terminal.moveCursor(
        2,
        1
    );


    std::printf(
        "        "
    );


    drawBar(
        "CPU",
        stats.cpuPercent,
        25
    );


    std::printf(
        "\033[K"
    );


    // ========================================================
    // RAM BAR
    // ========================================================

    terminal.moveCursor(
        3,
        1
    );


    std::printf(
        "        "
    );


    drawBar(
        "RAM",
        stats.ramPercent,
        25
    );


    std::printf(
        "  %s / %s",

        formatMemory(
            stats.usedMemory
        ).c_str(),

        formatMemory(
            stats.totalMemory
        ).c_str()
    );


    std::printf(
        "\033[K"
    );


    // ========================================================
    // SWAP BAR
    // ========================================================

    terminal.moveCursor(
        4,
        1
    );


    std::printf(
        "        "
    );


    drawBar(
        "SWP",
        stats.swapPercent,
        25
    );


    std::printf(
        "  %s / %s",

        formatMemory(
            stats.usedSwap
        ).c_str(),

        formatMemory(
            stats.totalSwap
        ).c_str()
    );


    std::printf(
        "\033[K"
    );


    // ========================================================
    // CONTROLS
    // ========================================================

    terminal.moveCursor(
        6,
        1
    );


    std::printf(
        "q:Quit   "
        "j/k:Move   "
        "k:Kill   "
        "c:CPU   "
        "m:Memory   "
        "p:PID   "
        "n:Name   "
        "r:Refresh"
    );


    std::printf(
        "\033[K"
    );


    // ========================================================
    // TABLE HEADER
    // ========================================================

    terminal.moveCursor(
        7,
        1
    );


    std::printf(
        "\033[1m"
        "%-8s %-32s %8s %8s %12s %-8s"
        "\033[0m",

        "PID",
        "NAME",
        "CPU%",
        "MEM%",
        "MEMORY",
        "STATE"
    );


    std::printf(
        "\033[K"
    );


    // ========================================================
    // TABLE DIVIDER
    // ========================================================

    terminal.moveCursor(
        8,
        1
    );


    std::printf(
        "-------- "
        "-------------------------------- "
        "-------- "
        "-------- "
        "------------ "
        "--------"
    );


    std::printf(
        "\033[K"
    );
}


// ============================================================
// DRAW PROCESSES
// ============================================================

void drawProcesses(
    Terminal& terminal,
    const std::vector<Process>& processes,
    int selected
) {

    int height =
        terminal.getHeight();


    // Rows 1-8 are occupied by
    // the system dashboard.

    const int firstProcessRow = 9;


    int availableRows =
        height -
        firstProcessRow;


    if (availableRows < 1) {

        return;
    }


    int visibleProcesses =
        std::min(
            availableRows,
            static_cast<int>(
                processes.size()
            )
        );


    // ========================================================
    // DRAW PROCESSES
    // ========================================================

    for (
        int i = 0;
        i < visibleProcesses;
        ++i
    ) {

        const Process& process =
            processes[i];


        int row =
            firstProcessRow +
            i;


        terminal.moveCursor(
            row,
            1
        );


        // Highlight selected process

        if (i == selected) {

            std::printf(
                "\033[7m"
            );
        }


        std::string name =
            process.name;


        if (name.length() > 31) {

            name =
                name.substr(
                    0,
                    31
                );
        }


        std::printf(
            "%-8d %-32s %7.1f%% %7.2f%% %12s %-8s",

            process.pid,

            name.c_str(),

            process.cpuPercent,

            process.memoryPercent,

            formatMemory(
                process.memoryBytes
            ).c_str(),

            process.state.c_str()
        );


        // Clear anything left over
        // from the previous frame.

        std::printf(
            "\033[K"
        );


        if (i == selected) {

            std::printf(
                "\033[0m"
            );
        }
    }


    // ========================================================
    // CLEAR UNUSED ROWS
    // ========================================================

    for (
        int i = visibleProcesses;
        i < availableRows;
        ++i
    ) {

        terminal.moveCursor(
            firstProcessRow + i,
            1
        );


        std::printf(
            "\033[K"
        );
    }
}


// ============================================================
// KILL PROCESS
// ============================================================

void killProcess(
    const std::vector<Process>& processes,
    int selected
) {

    if (
        selected < 0 ||
        selected >=
        static_cast<int>(
            processes.size()
        )
    ) {

        return;
    }


    int pid =
        processes[selected].pid;


    // Never allow Arktop to kill itself.

    if (pid == getpid()) {

        return;
    }


    kill(
        pid,
        SIGTERM
    );
}


// ============================================================
// MAIN
// ============================================================

int main() {

    // ========================================================
    // TERMINAL
    // ========================================================

    Terminal terminal;

    terminal.setup();


    // ========================================================
    // TERMINAL RESTORE GUARD
    // ========================================================

    struct TerminalGuard {

        Terminal& terminal;


        ~TerminalGuard() {

            terminal.restore();
        }

    } guard{
        terminal
    };


    // ========================================================
    // PROCESS PROVIDER
    // ========================================================

    MacProcessProvider provider;


    // ========================================================
    // CPU TRACKERS
    // ========================================================

    CPUTracker cpuTracker;

    SystemCPUTracker systemCPUTracker;


    // ========================================================
    // SETTINGS
    // ========================================================

    SortMode sortMode =
        SortMode::CPU;


    int selected = 0;


    bool running = true;


    int cpuCount =
        getCPUCount();


    auto lastUpdate =
        std::chrono::steady_clock::now();


    // ========================================================
    // INITIAL SCREEN CLEAR
    // ========================================================

    terminal.clear();


    // ========================================================
    // MAIN LOOP
    // ========================================================

    while (running) {

        // ====================================================
        // GET PROCESSES
        // ====================================================

        std::vector<Process> processes =
            provider.getProcesses();


        // ====================================================
        // ELAPSED TIME
        // ====================================================

        auto now =
            std::chrono::steady_clock::now();


        double elapsed =
            std::chrono::duration<double>(
                now -
                lastUpdate
            ).count();


        lastUpdate =
            now;


        // ====================================================
        // PROCESS CPU
        // ====================================================

        cpuTracker.update(
            processes,
            elapsed,
            cpuCount
        );


        // ====================================================
        // SYSTEM STATISTICS
        // ====================================================

        SystemStats systemStats =
            getSystemStats(
                systemCPUTracker
            );


        // ====================================================
        // SORT
        // ====================================================

        sortProcesses(
            processes,
            sortMode
        );


        // ====================================================
        // KEEP SELECTION VALID
        // ====================================================

        if (processes.empty()) {

            selected = 0;

        } else {

            if (
                selected >=
                static_cast<int>(
                    processes.size()
                )
            ) {

                selected =
                    static_cast<int>(
                        processes.size()
                    ) - 1;
            }
        }


        // ====================================================
        // DRAW HEADER
        // ====================================================

        drawHeader(
            terminal,
            systemStats,
            sortMode,
            processes.size()
        );


        // ====================================================
        // DRAW PROCESSES
        // ====================================================

        drawProcesses(
            terminal,
            processes,
            selected
        );


        std::fflush(
            stdout
        );


        // ====================================================
        // KEYBOARD
        // ====================================================

        if (
            terminal.keyAvailable()
        ) {

            char key =
                terminal.readKey();


            switch (key) {

                // --------------------------------------------
                // QUIT
                // --------------------------------------------

                case 'q':
                case 'Q':

                    running = false;

                    break;


                // --------------------------------------------
                // MOVE DOWN
                // --------------------------------------------

                case 'j':
                case 'J':

                    if (
                        selected + 1 <
                        static_cast<int>(
                            processes.size()
                        )
                    ) {

                        selected++;
                    }

                    break;


                // --------------------------------------------
                // KILL
                // --------------------------------------------

                case 'k':
                case 'K':

                    killProcess(
                        processes,
                        selected
                    );

                    break;


                // --------------------------------------------
                // CPU SORT
                // --------------------------------------------

                case 'c':
                case 'C':

                    sortMode =
                        SortMode::CPU;

                    break;


                // --------------------------------------------
                // MEMORY SORT
                // --------------------------------------------

                case 'm':
                case 'M':

                    sortMode =
                        SortMode::MEMORY;

                    break;


                // --------------------------------------------
                // PID SORT
                // --------------------------------------------

                case 'p':
                case 'P':

                    sortMode =
                        SortMode::PID;

                    break;


                // --------------------------------------------
                // NAME SORT
                // --------------------------------------------

                case 'n':
                case 'N':

                    sortMode =
                        SortMode::NAME;

                    break;


                // --------------------------------------------
                // REFRESH
                // --------------------------------------------

                case 'r':
                case 'R':

                    // The loop refreshes automatically.

                    break;
            }
        }


        // ====================================================
        // SMALL SLEEP
        // ====================================================

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                100
            )
        );
    }


    // ========================================================
    // EXIT
    // ========================================================

    terminal.clear();


    std::printf(
        "Thanks for using Arktop.\n"
    );


    return 0;
}