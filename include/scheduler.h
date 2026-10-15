#ifndef MINI_KERNEL_SCHEDULER_H
#define MINI_KERNEL_SCHEDULER_H

#include "pcb.h"
#include <algorithm>
#include <deque>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>

namespace cpu {

enum class Algorithm { FCFS, SJF, SRTF, PRIORITY, RR };

// Keep names in one place for the menu, trace and comparison table.
inline const char* name(Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::FCFS: return "FCFS";
        case Algorithm::SJF: return "SJF";
        case Algorithm::SRTF: return "SRTF";
        case Algorithm::PRIORITY: return "Priority";
        case Algorithm::RR: return "Round Robin";
    }
    return "Unknown";
}

struct Result {
    Algorithm algorithm;
    std::vector<PCB> processes;
    std::vector<int> ticks; // PID used during each tick; -1 means idle.
    int elapsed = 0, busy = 0, contextSwitches = 0;
    double averageWaiting = 0, averageTurnaround = 0, averageResponse = 0;
};

// Choose one READY process; arrival and PID make ties deterministic.
inline int bestReady(const std::vector<PCB>& processes, Algorithm algorithm) {
    int best = -1;
    for (std::size_t i = 0; i < processes.size(); ++i) {
        const PCB& p = processes[i];
        if (p.state != State::READY) continue;
        if (best < 0) { best = static_cast<int>(i); continue; }
        const PCB& b = processes[best];
        int key = algorithm == Algorithm::PRIORITY ? p.priority :
                  algorithm == Algorithm::FCFS ? p.arrival :
                  algorithm == Algorithm::SJF ? p.burst : p.remaining;
        int old = algorithm == Algorithm::PRIORITY ? b.priority :
                  algorithm == Algorithm::FCFS ? b.arrival :
                  algorithm == Algorithm::SJF ? b.burst : b.remaining;
        if (key < old || (key == old && (p.arrival < b.arrival ||
            (p.arrival == b.arrival && p.pid < b.pid)))) best = static_cast<int>(i);
    }
    return best;
}

// Simulate one clock tick at a time: admit, preempt, dispatch, execute, then complete.
inline Result run(std::vector<PCB> processes, Algorithm algorithm, int quantum = 3,
                  bool verbose = true, std::ostream& out = std::cout) {
    if (quantum <= 0) throw std::invalid_argument("Quantum must be positive");
    for (std::size_t i = 0; i < processes.size(); ++i) {
        PCB& p = processes[i];
        if (p.pid <= 0 || p.arrival < 0 || p.burst <= 0)
            throw std::invalid_argument("PID/burst must be positive; arrival cannot be negative");
        for (std::size_t j = 0; j < i; ++j)
            if (processes[j].pid == p.pid) throw std::invalid_argument("Duplicate PID");
        p.remaining = p.burst; p.state = State::NEW;
        p.finish = -1; p.response = -1; p.waiting = p.turnaround = 0;
    }
    Result result{algorithm, processes, {}, 0, 0, 0, 0, 0, 0};
    std::deque<int> queue;
    int running = -1, pendingRR = -1, slice = 0, completed = 0, time = 0;
    if (verbose) out << "\n=== " << name(algorithm) << " kernel trace ===\n";

    while (completed < static_cast<int>(processes.size())) {
        // Arrivals become READY before deciding who owns this tick.
        for (std::size_t i = 0; i < processes.size(); ++i) {
            if (processes[i].state == State::NEW && processes[i].arrival == time) {
                setState(processes[i], State::READY, time, verbose, out);
                if (algorithm == Algorithm::RR) queue.push_back(static_cast<int>(i));
            }
        }
        // At a quantum boundary, new arrivals join before the expired process.
        if (pendingRR >= 0) { queue.push_back(pendingRR); pendingRR = -1; }
        if (running >= 0 && (algorithm == Algorithm::SRTF ||
                             algorithm == Algorithm::PRIORITY)) {
            int challenger = bestReady(processes, algorithm);
            if (challenger >= 0 &&
                (algorithm == Algorithm::SRTF
                    ? processes[challenger].remaining < processes[running].remaining
                    : processes[challenger].priority < processes[running].priority)) {
                setState(processes[running], State::READY, time, verbose, out);
                running = -1;
            }
        }
        if (running < 0) {
            if (algorithm == Algorithm::RR) {
                if (!queue.empty()) { running = queue.front(); queue.pop_front(); }
            } else {
                running = bestReady(processes, algorithm);
            }
            if (running >= 0) {
                PCB& p = processes[running];
                if (p.response < 0) p.response = time - p.arrival;
                setState(p, State::RUNNING, time, verbose, out);
                ++result.contextSwitches; // Count dispatches, including the first.
                slice = 0;
            }
        }
        if (running < 0) {
            result.ticks.push_back(-1);
            if (verbose) out << "[t=" << time << "] CPU idle\n";
        } else {
            PCB& p = processes[running];
            result.ticks.push_back(p.pid);
            ++result.busy; --p.remaining; ++slice;
            if (verbose) out << "[t=" << time << ".." << time + 1 << "] P"
                             << p.pid << " runs; remaining=" << p.remaining << '\n';
            if (p.remaining == 0) {
                p.finish = time + 1;
                p.turnaround = p.finish - p.arrival;
                p.waiting = p.turnaround - p.burst;
                setState(p, State::TERMINATED, time + 1, verbose, out);
                running = -1; ++completed;
            } else if (algorithm == Algorithm::RR && slice == quantum) {
                setState(p, State::READY, time + 1, verbose, out);
                pendingRR = running;
                running = -1;
            }
        }
        ++time;
    }
    result.elapsed = time;
    result.processes = processes;
    for (const PCB& p : processes) {
        result.averageWaiting += p.waiting;
        result.averageTurnaround += p.turnaround;
        result.averageResponse += p.response;
    }
    if (!processes.empty()) {
        result.averageWaiting /= processes.size();
        result.averageTurnaround /= processes.size();
        result.averageResponse /= processes.size();
    }
    if (verbose) {
        out << "Gantt: ";
        for (std::size_t start = 0; start < result.ticks.size();) {
            std::size_t end = start + 1;
            while (end < result.ticks.size() && result.ticks[end] == result.ticks[start]) ++end;
            out << "[" << start << "-" << end << ":";
            if (result.ticks[start] < 0) out << "IDLE";
            else out << "P" << result.ticks[start];
            out << "] "; start = end;
        }
        out << "\nPID Name       AT BT Pri CT TAT WT RT\n";
        for (const PCB& p : processes)
            out << std::setw(3) << p.pid << ' ' << std::left << std::setw(10) << p.name
                << std::right << std::setw(3) << p.arrival << std::setw(3) << p.burst
                << std::setw(4) << p.priority << std::setw(3) << p.finish
                << std::setw(4) << p.turnaround << std::setw(3) << p.waiting
                << std::setw(3) << p.response << '\n';
        out << std::fixed << std::setprecision(2)
            << "Averages WT/TAT/RT: " << result.averageWaiting << " / "
            << result.averageTurnaround << " / " << result.averageResponse
            << "\nCPU utilization: " << (time ? 100.0 * result.busy / time : 0.0)
            << "%\nContext switches (dispatches): " << result.contextSwitches << "\n";
    }
    return result;
}

// Run each policy on a fresh copy and print the metrics side by side.
inline void compare(const std::vector<PCB>& processes, int quantum = 3,
                    std::ostream& out = std::cout) {
    out << "\nAlgorithm      Avg WT  Avg TAT  Avg RT  Dispatches\n";
    for (Algorithm algorithm : {Algorithm::FCFS, Algorithm::SJF, Algorithm::SRTF,
                                Algorithm::PRIORITY, Algorithm::RR}) {
        Result r = run(processes, algorithm, quantum, false, out);
        out << std::left << std::setw(15) << name(algorithm) << std::right
            << std::fixed << std::setprecision(2) << std::setw(7) << r.averageWaiting
            << std::setw(9) << r.averageTurnaround << std::setw(8)
            << r.averageResponse << std::setw(12) << r.contextSwitches << '\n';
    }
}

} // namespace cpu
#endif
