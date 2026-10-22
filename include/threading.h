#ifndef MINI_KERNEL_THREADING_H
#define MINI_KERNEL_THREADING_H

#include <iostream>
#include <string>
#include <vector>

namespace threadsim {

enum class State { READY, RUNNING, BLOCKED, TERMINATED };
inline const char* stateName(State state) {
    switch (state) {
        case State::READY: return "READY";
        case State::RUNNING: return "RUNNING";
        case State::BLOCKED: return "BLOCKED";
        case State::TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}

struct Thread { int tid; int pid; State state; };

// Contrasts thread-to-kernel mapping models with a deterministic blocking example.
inline void modelDemo(std::ostream& out = std::cout) {
    out << "\nThread model comparison\n";
    std::vector<Thread> manyToOne = {{1, 10, State::RUNNING}, {2, 10, State::READY}};
    out << "Many-to-one: T1 and T2 map to one kernel thread. T1 blocks on I/O;\n";
    manyToOne[0].state = State::BLOCKED;
    manyToOne[1].state = State::BLOCKED;
    out << "  T1=" << stateName(manyToOne[0].state) << ", T2="
        << stateName(manyToOne[1].state) << " (the whole process cannot run)\n";
    out << "One-to-one: each user thread maps to a kernel thread. T1 blocks on I/O;\n";
    std::vector<Thread> oneToOne = {{1, 10, State::BLOCKED}, {2, 10, State::RUNNING}};
    out << "  T1=" << stateName(oneToOne[0].state) << ", T2="
        << stateName(oneToOne[1].state) << " (T2 can continue)\n";
    out << "This is a mapping illustration; no real OS threads are created.\n";
}

} // namespace threadsim

#endif
