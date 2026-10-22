#ifndef MINI_KERNEL_DEADLOCK_H
#define MINI_KERNEL_DEADLOCK_H

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace deadlock {
using Matrix = std::vector<std::vector<int>>;

struct State {
    std::vector<int> available;
    Matrix allocation;
    Matrix request;
};

struct Detection {
    std::vector<int> deadlocked;
    std::vector<int> completionOrder;
};

inline void printState(const State& state, std::ostream& out, const std::string& title) {
    out << title << "\nAvailable:";
    for (int value : state.available) out << ' ' << value;
    out << "\nPID  Allocation  Request\n";
    for (std::size_t i = 0; i < state.allocation.size(); ++i) {
        out << std::right << std::setw(3) << ('P' + std::to_string(i)) << "  ";
        for (std::size_t j = 0; j < state.available.size(); ++j)
            out << (j ? " " : "") << state.allocation[i][j];
        out << "         ";
        for (std::size_t j = 0; j < state.available.size(); ++j)
            out << (j ? " " : "") << state.request[i][j];
        out << '\n';
    }
}

enum class VictimPolicy { FIRST_DEADLOCKED, LEAST_ALLOCATED };

inline const char* victimPolicyName(VictimPolicy policy) {
    return policy == VictimPolicy::FIRST_DEADLOCKED
        ? "terminate the lowest-numbered deadlocked process"
        : "terminate the deadlocked process holding the fewest resources";
}

inline void validate(const State& state) {
    if (state.available.empty() || state.allocation.empty() ||
        state.allocation.size() != state.request.size())
        throw std::invalid_argument("Invalid deadlock-detection matrices");
    for (int value : state.available)
        if (value < 0) throw std::invalid_argument("Available resources cannot be negative");
    for (std::size_t i = 0; i < state.allocation.size(); ++i) {
        if (state.allocation[i].size() != state.available.size() ||
            state.request[i].size() != state.available.size())
            throw std::invalid_argument("Deadlock matrix dimensions differ");
        for (std::size_t j = 0; j < state.available.size(); ++j)
            if (state.allocation[i][j] < 0 || state.request[i][j] < 0)
                throw std::invalid_argument("Resource values cannot be negative");
    }
}

// Repeatedly completes any process whose current outstanding request can be met.
inline Detection detect(const State& state, bool verbose = true,
                        std::ostream& out = std::cout) {
    validate(state);
    std::vector<int> work = state.available;
    std::vector<bool> finished(state.allocation.size(), false);
    for (std::size_t i = 0; i < state.allocation.size(); ++i) {
        bool holdsNothing = true;
        for (int allocation : state.allocation[i]) if (allocation != 0) holdsNothing = false;
        if (holdsNothing) finished[i] = true;
    }
    Detection result;
    bool progress = true;
    while (progress) {
        progress = false;
        for (std::size_t i = 0; i < finished.size(); ++i) {
            if (finished[i]) continue;
            bool canFinish = true;
            for (std::size_t j = 0; j < work.size(); ++j)
                if (state.request[i][j] > work[j]) canFinish = false;
            if (!canFinish) continue;
            if (verbose) out << "P" << i << " can finish; releases its allocation; Work:";
            for (std::size_t j = 0; j < work.size(); ++j) {
                work[j] += state.allocation[i][j];
                if (verbose) out << ' ' << work[j];
            }
            if (verbose) out << '\n';
            finished[i] = true;
            result.completionOrder.push_back(static_cast<int>(i));
            progress = true;
        }
    }
    for (std::size_t i = 0; i < finished.size(); ++i)
        if (!finished[i]) result.deadlocked.push_back(static_cast<int>(i));
    if (verbose) {
        if (result.deadlocked.empty()) out << "No deadlock detected.\n";
        else {
            out << "Deadlocked processes: ";
            for (std::size_t i = 0; i < result.deadlocked.size(); ++i)
                out << (i ? ", " : "") << 'P' << result.deadlocked[i];
            out << '\n';
        }
    }
    return result;
}

// Rolls back one victim and returns the recovered state. Detection is repeated
// afterwards so the caller can verify that the deadlock is gone.
inline State recover(State state, VictimPolicy policy = VictimPolicy::FIRST_DEADLOCKED,
                     std::ostream& out = std::cout) {
    const Detection detection = detect(state, false);
    if (detection.deadlocked.empty()) {
        out << "Recovery: no deadlocked process to terminate.\n";
        return state;
    }
    int victim = detection.deadlocked.front();
    if (policy == VictimPolicy::LEAST_ALLOCATED) {
        long best = -1;
        for (int pid : detection.deadlocked) {
            long held = 0;
            for (int units : state.allocation[static_cast<std::size_t>(pid)]) held += units;
            if (best < 0 || held < best) { best = held; victim = pid; }
        }
    }
    out << "Recovery policy: " << victimPolicyName(policy) << ".\n"
        << "Recovery: terminate P" << victim << " and release its resources.\n";
    for (std::size_t j = 0; j < state.available.size(); ++j) {
        state.available[j] += state.allocation[static_cast<std::size_t>(victim)][j];
        state.allocation[static_cast<std::size_t>(victim)][j] = 0;
        state.request[static_cast<std::size_t>(victim)][j] = 0;
    }
    return state;
}

// Detects a cycle in a wait-for graph using three-color depth-first search.
inline bool hasCycle(const std::vector<std::vector<int>>& graph,
                     std::ostream& out = std::cout) {
    const std::size_t n = graph.size();
    std::vector<int> color(n, 0), parent(n, -1), cycle;
    std::function<bool(int)> visit = [&](int node) {
        color[static_cast<std::size_t>(node)] = 1;
        for (int next : graph[static_cast<std::size_t>(node)]) {
            if (next < 0 || static_cast<std::size_t>(next) >= n)
                throw std::invalid_argument("Wait-for graph contains an invalid process ID");
            if (color[static_cast<std::size_t>(next)] == 0) {
                parent[static_cast<std::size_t>(next)] = node;
                if (visit(next)) return true;
            } else if (color[static_cast<std::size_t>(next)] == 1) {
                cycle.push_back(next);
                for (int current = node; current != next; current = parent[static_cast<std::size_t>(current)])
                    cycle.push_back(current);
                cycle.push_back(next);
                std::reverse(cycle.begin(), cycle.end());
                return true;
            }
        }
        color[static_cast<std::size_t>(node)] = 2;
        return false;
    };
    bool found = false;
    for (std::size_t i = 0; i < n && !found; ++i)
        if (color[i] == 0) found = visit(static_cast<int>(i));
    if (found) {
        out << "Wait-for cycle: ";
        for (std::size_t i = 0; i < cycle.size(); ++i)
            out << (i ? " -> " : "") << 'P' << cycle[i];
        out << '\n';
    } else out << "No wait-for cycle.\n";
    return found;
}

inline State sample() {
    return {{0, 0}, {{1, 0}, {0, 1}, {0, 0}}, {{0, 1}, {1, 0}, {0, 0}}};
}

// Demonstrates recovery by terminating one deadlocked process and returning its resources.
inline void recoveryDemo(std::ostream& out = std::cout) {
    State state = sample();
    out << "\nDeadlock detection and recovery\n";
    printState(state, out, "\nBefore recovery");
    Detection before = detect(state, true, out);
    if (before.deadlocked.empty()) return;
    state = recover(state, VictimPolicy::FIRST_DEADLOCKED, out);
    printState(state, out, "\nAfter recovery");
    const Detection after = detect(state, true, out);
    out << (after.deadlocked.empty() ? "Recovery successful: the system can complete every process.\n"
                                     : "Recovery incomplete: deadlock remains.\n");
}

inline void demo(std::ostream& out = std::cout) {
    out << "\nWait-for graph demo (P0 waits for P1; P1 waits for P0)\n";
    hasCycle({{1}, {0}, {}}, out);
    recoveryDemo(out);
}

} // namespace deadlock

#endif
