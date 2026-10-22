#ifndef MINI_KERNEL_PCB_H
#define MINI_KERNEL_PCB_H

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

enum class State { NEW, READY, RUNNING, WAITING, TERMINATED };

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

// The legal transition graph of this simulator. Used to reject invalid state
// changes instead of silently producing an inconsistent process table.
// A process may also be killed (TERMINATED) from any non-terminated state, which
// models an administrator kill or deadlock recovery.
inline bool canTransition(State from, State to) {
    if (from == to) return false;
    if (to == State::TERMINATED) return from != State::TERMINATED;
    switch (from) {
        case State::NEW: return to == State::READY;
        case State::READY: return to == State::RUNNING;
        case State::RUNNING: return to == State::READY || to == State::WAITING;
        case State::WAITING: return to == State::READY;
        case State::TERMINATED: return false;
    }
    return false;
}

inline void setState(PCB& process, State next, int time, bool verbose = true,
                     std::ostream& out = std::cout) {
    if (process.state == next) return;
    if (!canTransition(process.state, next))
        throw std::logic_error("Illegal process transition for P" +
                               std::to_string(process.pid) + ": " +
                               stateName(process.state) + " -> " + stateName(next));
    if (verbose)
        out << "[t=" << time << "] P" << process.pid << " (" << process.name
            << "): " << stateName(process.state) << " -> " << stateName(next) << '\n';
    process.state = next;
}

inline std::vector<PCB> sampleProcesses() {
    return {{1, "init", 0, 8, 8, 3}, {2, "editor", 1, 4, 4, 1},
            {3, "compiler", 2, 9, 9, 4}, {4, "browser", 3, 5, 5, 2},
            {5, "daemon", 6, 2, 2, 5}};
}

#endif
