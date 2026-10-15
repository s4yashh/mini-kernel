#ifndef MINI_KERNEL_PCB_H
#define MINI_KERNEL_PCB_H

#include <iostream>
#include <string>
#include <vector>

enum class State { NEW, READY, RUNNING, WAITING, TERMINATED };

// Convert an internal process state to the word printed in the kernel trace.
inline const char* stateName(State state) {
    switch (state) {
        case State::NEW: return "NEW";
        case State::READY: return "READY";
        case State::RUNNING: return "RUNNING";
        case State::WAITING: return "WAITING";
        case State::TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}

struct PCB {
    int pid = 0;
    std::string name;
    int arrival = 0, burst = 0, remaining = 0, priority = 0;
    State state = State::NEW;
    int finish = -1, response = -1, waiting = 0, turnaround = 0;
};

// Change state once and, when requested, show the exact simulated clock time.
inline void setState(PCB& process, State next, int time, bool verbose = true,
                     std::ostream& out = std::cout) {
    if (process.state == next) return;
    if (verbose)
        out << "[t=" << time << "] P" << process.pid << " (" << process.name
            << "): " << stateName(process.state) << " -> " << stateName(next) << '\n';
    process.state = next;
}

// Return fresh PCBs so each algorithm starts from identical input.
inline std::vector<PCB> sampleProcesses() {
    return {{1, "init", 0, 8, 8, 3}, {2, "editor", 1, 4, 4, 1},
            {3, "compiler", 2, 9, 9, 4}, {4, "browser", 3, 5, 5, 2},
            {5, "daemon", 6, 2, 2, 5}};
}

#endif
