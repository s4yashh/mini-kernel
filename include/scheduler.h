#ifndef MINI_KERNEL_SCHEDULER_H
#define MINI_KERNEL_SCHEDULER_H

#include "pcb.h"
#include <algorithm>
#include <deque>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace cpu {

enum class Algorithm { FCFS, SJF, SRTF, PRIORITY, RR, MLFQ, MLQ };

inline const char* name(Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::FCFS: return "FCFS";
        case Algorithm::SJF: return "SJF";
        case Algorithm::SRTF: return "SRTF";
        case Algorithm::PRIORITY: return "Priority";
        case Algorithm::RR: return "Round Robin";
        case Algorithm::MLFQ: return "Multilevel Feedback Queue";
        case Algorithm::MLQ: return "Multilevel Queue";
    }
    return "Unknown";
}

struct Result {
    Algorithm algorithm = Algorithm::FCFS;
    std::vector<PCB> processes;
    std::vector<int> ticks;
    std::vector<std::string> decisions;
    int elapsed = 0, busy = 0, contextSwitches = 0;
    double averageWaiting = 0, averageTurnaround = 0, averageResponse = 0;
};

inline int bestReady(const std::vector<PCB>& processes, Algorithm algorithm) {
    int best = -1;
    for (std::size_t i = 0; i < processes.size(); ++i) {
        const PCB& p = processes[i];
        if (p.state != State::READY) continue;
        if (best < 0) { best = static_cast<int>(i); continue; }
        const PCB& b = processes[best];
        int currentValue = p.remaining;
        int bestValue = b.remaining;
        if (algorithm == Algorithm::FCFS) {
            currentValue = p.arrival;
            bestValue = b.arrival;
        } else if (algorithm == Algorithm::SJF) {
            currentValue = p.burst;
            bestValue = b.burst;
        } else if (algorithm == Algorithm::PRIORITY) {
            currentValue = p.priority;
            bestValue = b.priority;
        }
        if (currentValue < bestValue) best = static_cast<int>(i);
        else if (currentValue == bestValue && p.arrival < b.arrival) best = static_cast<int>(i);
        else if (currentValue == bestValue && p.arrival == b.arrival && p.pid < b.pid)
            best = static_cast<int>(i);
    }
    return best;
}

// Human-readable justification for a dispatch. The tie-break is always the same:
// earlier arrival time, then lower PID.
inline std::string explainPick(const std::vector<PCB>& processes, Algorithm algorithm,
                               int chosen) {
    const PCB& selected = processes[static_cast<std::size_t>(chosen)];
    std::ostringstream text;
    text << 'P' << selected.pid << " (" << selected.name << ") selected: ";
    switch (algorithm) {
        case Algorithm::FCFS: text << "earliest arrival time in the READY queue"; break;
        case Algorithm::SJF: text << "shortest total CPU burst in the READY queue"; break;
        case Algorithm::SRTF: text << "shortest remaining time in the READY queue"; break;
        case Algorithm::PRIORITY: text << "lowest priority number in the READY queue"; break;
        case Algorithm::RR: text << "front of the round-robin ready queue"; break;
        case Algorithm::MLFQ: text << "highest non-empty feedback queue"; break;
        case Algorithm::MLQ: text << "highest-priority static queue"; break;
    }
    int ties = 0;
    std::ostringstream rivals;
    for (const PCB& other : processes) {
        if (other.state != State::READY || other.pid == selected.pid) continue;
        bool tie = false;
        if (algorithm == Algorithm::FCFS || algorithm == Algorithm::RR)
            tie = other.arrival == selected.arrival;
        else if (algorithm == Algorithm::SJF) tie = other.burst == selected.burst;
        else if (algorithm == Algorithm::SRTF) tie = other.remaining == selected.remaining;
        else if (algorithm == Algorithm::PRIORITY) tie = other.priority == selected.priority;
        if (tie) {
            ++ties;
            rivals << (ties > 1 ? ", " : "") << 'P' << other.pid;
        }
    }
    if (ties > 0)
        text << "; tie with " << rivals.str()
             << " broken by earlier arrival time, then lower PID";
    return text.str();
}

inline Result run(std::vector<PCB> processes, Algorithm algorithm, int quantum = 3,
                  bool verbose = true, std::ostream& out = std::cout) {
    if (quantum <= 0) throw std::invalid_argument("Quantum must be positive");
    for (std::size_t i = 0; i < processes.size(); ++i) {
        PCB& p = processes[i];
        if (p.pid <= 0 || p.arrival < 0 || p.burst <= 0 || p.priority < 0)
            throw std::invalid_argument("PID/burst must be positive; arrival/priority cannot be negative");
        for (std::size_t j = 0; j < i; ++j)
            if (processes[j].pid == p.pid) throw std::invalid_argument("Duplicate PID");
        p.remaining = p.burst; p.state = State::NEW;
        p.finish = -1; p.response = -1; p.waiting = p.turnaround = 0;
    }
    Result result;
    result.algorithm = algorithm;
    result.processes = processes;
    std::deque<int> queue;
    int running = -1, pendingRR = -1, slice = 0, completed = 0, time = 0;
    if (verbose) out << "\n=== " << name(algorithm) << " kernel trace ===\n";

    while (completed < static_cast<int>(processes.size())) {
        for (std::size_t i = 0; i < processes.size(); ++i) {
            if (processes[i].state == State::NEW && processes[i].arrival == time) {
                setState(processes[i], State::READY, time, verbose, out);
                if (algorithm == Algorithm::RR) queue.push_back(static_cast<int>(i));
            }
        }
        if (pendingRR >= 0) { queue.push_back(pendingRR); pendingRR = -1; }
        if (running >= 0 && (algorithm == Algorithm::SRTF ||
                             algorithm == Algorithm::PRIORITY)) {
            int challenger = bestReady(processes, algorithm);
            bool preempt = false;
            if (challenger >= 0 && algorithm == Algorithm::SRTF)
                preempt = processes[challenger].remaining < processes[running].remaining;
            if (challenger >= 0 && algorithm == Algorithm::PRIORITY)
                preempt = processes[challenger].priority < processes[running].priority;
            if (preempt) {
                if (verbose)
                    out << "  scheduler: P" << processes[challenger].pid
                        << " preempts P" << processes[running].pid << " because its "
                        << (algorithm == Algorithm::SRTF ? "remaining time is shorter" : "priority is better")
                        << '\n';
                result.decisions.push_back("[t=" + std::to_string(time) + "] P" +
                    std::to_string(processes[challenger].pid) + " preempts P" +
                    std::to_string(processes[running].pid));
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
                const std::string reason = explainPick(processes, algorithm, running);
                result.decisions.push_back("[t=" + std::to_string(time) + "] " + reason);
                if (verbose) out << "  scheduler: " << reason << '\n';
                ++result.contextSwitches;
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

// Runs a three-level feedback scheduler: new jobs start at level 0, use a short
// quantum, and unfinished jobs move down a level; higher queues preempt lower ones.
inline Result runMLFQ(std::vector<PCB> processes, bool verbose = true,
                     std::ostream& out = std::cout) {
    const int quantum[3] = {2, 4, 8};
    for (std::size_t i = 0; i < processes.size(); ++i) {
        PCB& p = processes[i];
        if (p.pid <= 0 || p.arrival < 0 || p.burst <= 0)
            throw std::invalid_argument("PID/burst must be positive; arrival cannot be negative");
        for (std::size_t j = 0; j < i; ++j)
            if (processes[j].pid == p.pid) throw std::invalid_argument("Duplicate PID");
        p.remaining = p.burst; p.state = State::NEW;
        p.finish = -1; p.response = -1; p.waiting = p.turnaround = 0;
    }

    Result result;
    result.algorithm = Algorithm::MLFQ;
    std::deque<int> queues[3];
    std::vector<int> level(processes.size(), 0);
    int running = -1, slice = 0, completed = 0, time = 0;
    if (verbose) out << "\n=== Multilevel Feedback Queue trace (quanta 2/4/8) ===\n";

    while (completed < static_cast<int>(processes.size())) {
        for (std::size_t i = 0; i < processes.size(); ++i) {
            if (processes[i].state == State::NEW && processes[i].arrival == time) {
                setState(processes[i], State::READY, time, verbose, out);
                level[i] = 0;
                queues[0].push_back(static_cast<int>(i));
            }
        }
        if (running >= 0) {
            bool higherReady = false;
            for (int q = 0; q < level[static_cast<std::size_t>(running)]; ++q)
                if (!queues[q].empty()) higherReady = true;
            if (higherReady) {
                const int currentLevel = level[static_cast<std::size_t>(running)];
                setState(processes[static_cast<std::size_t>(running)], State::READY, time, verbose, out);
                queues[currentLevel].push_front(running);
                running = -1;
            }
        }
        if (running < 0) {
            for (int q = 0; q < 3; ++q) if (!queues[q].empty()) {
                running = queues[q].front();
                queues[q].pop_front();
                break;
            }
            if (running >= 0) {
                PCB& p = processes[static_cast<std::size_t>(running)];
                if (p.response < 0) p.response = time - p.arrival;
                setState(p, State::RUNNING, time, verbose, out);
                std::ostringstream reason;
                reason << "P" << p.pid << " (" << p.name << ") selected from feedback queue "
                       << level[static_cast<std::size_t>(running)]
                       << ", the highest non-empty queue (quanta 2/4/8)";
                result.decisions.push_back("[t=" + std::to_string(time) + "] " + reason.str());
                if (verbose) out << "  scheduler: " << reason.str() << '\n';
                ++result.contextSwitches;
                slice = 0;
            }
        }
        if (running < 0) {
            result.ticks.push_back(-1);
            if (verbose) out << "[t=" << time << "] CPU idle\n";
        } else {
            PCB& p = processes[static_cast<std::size_t>(running)];
            result.ticks.push_back(p.pid);
            --p.remaining;
            ++slice;
            ++result.busy;
            if (verbose) out << "[t=" << time << ".." << time + 1 << "] P" << p.pid
                             << " runs at queue " << level[static_cast<std::size_t>(running)]
                             << "; remaining=" << p.remaining << '\n';
            if (p.remaining == 0) {
                p.finish = time + 1;
                p.turnaround = p.finish - p.arrival;
                p.waiting = p.turnaround - p.burst;
                setState(p, State::TERMINATED, time + 1, verbose, out);
                running = -1;
                ++completed;
            } else if (slice == quantum[level[static_cast<std::size_t>(running)]]) {
                int& currentLevel = level[static_cast<std::size_t>(running)];
                if (currentLevel < 2) ++currentLevel;
                setState(p, State::READY, time + 1, verbose, out);
                queues[currentLevel].push_back(running);
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
        result.averageWaiting /= static_cast<double>(processes.size());
        result.averageTurnaround /= static_cast<double>(processes.size());
        result.averageResponse /= static_cast<double>(processes.size());
    }
    if (verbose) {
        out << "Gantt: ";
        for (std::size_t start = 0; start < result.ticks.size();) {
            std::size_t end = start + 1;
            while (end < result.ticks.size() && result.ticks[end] == result.ticks[start]) ++end;
            out << '[' << start << '-' << end << ':';
            if (result.ticks[start] < 0) out << "IDLE";
            else out << 'P' << result.ticks[start];
            out << "] ";
            start = end;
        }
        out << "\nAverage WT/TAT/RT: " << std::fixed << std::setprecision(2)
            << result.averageWaiting << " / " << result.averageTurnaround << " / "
            << result.averageResponse << "\nCPU utilization: "
            << (time ? 100.0 * result.busy / time : 0.0) << "%\nDispatches: "
            << result.contextSwitches << '\n';
        out << "PID Name       CT TAT WT RT\n";
        for (const PCB& p : processes)
            out << std::setw(3) << p.pid << ' ' << std::left << std::setw(10) << p.name
                << std::right << std::setw(3) << p.finish << std::setw(4) << p.turnaround
                << std::setw(3) << p.waiting << std::setw(3) << p.response << '\n';
    }
    return result;
}

// Runs fixed multilevel queues. Priority selects the queue once; RR rotates jobs
// within each queue and a ready higher queue can preempt a lower queue.
inline Result runMLQ(std::vector<PCB> processes, bool verbose = true,
                     std::ostream& out = std::cout) {
    const int quantum[3] = {2, 3, 4};
    std::vector<int> level(processes.size(), 0);
    for (std::size_t i = 0; i < processes.size(); ++i) {
        PCB& p = processes[i];
        if (p.pid <= 0 || p.arrival < 0 || p.burst <= 0 || p.priority < 0)
            throw std::invalid_argument("PID/burst must be positive; arrival/priority cannot be negative");
        for (std::size_t j = 0; j < i; ++j)
            if (processes[j].pid == p.pid) throw std::invalid_argument("Duplicate PID");
        level[i] = p.priority <= 1 ? 0 : (p.priority <= 3 ? 1 : 2);
        p.remaining = p.burst; p.state = State::NEW;
        p.finish = -1; p.response = -1; p.waiting = p.turnaround = 0;
    }
    Result result;
    result.algorithm = Algorithm::MLQ;
    std::deque<int> queues[3];
    int running = -1, runningLevel = -1, slice = 0, completed = 0, time = 0;
    if (verbose) out << "\n=== Multilevel Queue trace (static priority classes, RR quanta 2/3/4) ===\n";
    while (completed < static_cast<int>(processes.size())) {
        for (std::size_t i = 0; i < processes.size(); ++i) {
            if (processes[i].state == State::NEW && processes[i].arrival == time) {
                setState(processes[i], State::READY, time, verbose, out);
                queues[level[i]].push_back(static_cast<int>(i));
            }
        }
        if (running >= 0) {
            bool higherReady = false;
            for (int q = 0; q < runningLevel; ++q) if (!queues[q].empty()) higherReady = true;
            if (higherReady) {
                setState(processes[static_cast<std::size_t>(running)], State::READY, time, verbose, out);
                queues[runningLevel].push_front(running);
                running = -1;
            }
        }
        if (running < 0) {
            for (int q = 0; q < 3; ++q) if (!queues[q].empty()) {
                runningLevel = q;
                running = queues[q].front();
                queues[q].pop_front();
                break;
            }
            if (running >= 0) {
                PCB& p = processes[static_cast<std::size_t>(running)];
                if (p.response < 0) p.response = time - p.arrival;
                setState(p, State::RUNNING, time, verbose, out);
                std::ostringstream reason;
                reason << "P" << p.pid << " (" << p.name << ") selected from static queue "
                       << runningLevel << " (priority class "
                       << (runningLevel == 0 ? "1-1" : runningLevel == 1 ? "2-3" : "4+")
                       << "); higher queues always run first";
                result.decisions.push_back("[t=" + std::to_string(time) + "] " + reason.str());
                if (verbose) out << "  scheduler: " << reason.str() << '\n';
                ++result.contextSwitches;
                slice = 0;
            }
        }
        if (running < 0) {
            result.ticks.push_back(-1);
            if (verbose) out << "[t=" << time << "] CPU idle\n";
        } else {
            PCB& p = processes[static_cast<std::size_t>(running)];
            result.ticks.push_back(p.pid);
            --p.remaining; ++slice; ++result.busy;
            if (verbose) out << "[t=" << time << ".." << time + 1 << "] P" << p.pid
                             << " runs in static queue " << runningLevel << "; remaining="
                             << p.remaining << '\n';
            if (p.remaining == 0) {
                p.finish = time + 1;
                p.turnaround = p.finish - p.arrival;
                p.waiting = p.turnaround - p.burst;
                setState(p, State::TERMINATED, time + 1, verbose, out);
                running = -1; ++completed;
            } else if (slice == quantum[runningLevel]) {
                setState(p, State::READY, time + 1, verbose, out);
                queues[runningLevel].push_back(running);
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
        result.averageWaiting /= static_cast<double>(processes.size());
        result.averageTurnaround /= static_cast<double>(processes.size());
        result.averageResponse /= static_cast<double>(processes.size());
    }
    if (verbose) {
        out << "Gantt: ";
        for (std::size_t start = 0; start < result.ticks.size();) {
            std::size_t end = start + 1;
            while (end < result.ticks.size() && result.ticks[end] == result.ticks[start]) ++end;
            out << '[' << start << '-' << end << ':';
            if (result.ticks[start] < 0) out << "IDLE"; else out << 'P' << result.ticks[start];
            out << "] "; start = end;
        }
        out << "\nAverage WT/TAT/RT: " << std::fixed << std::setprecision(2)
            << result.averageWaiting << " / " << result.averageTurnaround << " / "
            << result.averageResponse << "\nCPU utilization: "
            << (time ? 100.0 * result.busy / time : 0.0) << "%\nDispatches: "
            << result.contextSwitches << '\n';
        out << "PID Name       Queue CT TAT WT RT\n";
        for (std::size_t i = 0; i < processes.size(); ++i) {
            const PCB& p = processes[i];
            out << std::setw(3) << p.pid << ' ' << std::left << std::setw(10) << p.name
                << std::right << std::setw(5) << level[i] << std::setw(3) << p.finish
                << std::setw(4) << p.turnaround << std::setw(3) << p.waiting
                << std::setw(3) << p.response << '\n';
        }
    }
    return result;
}

}
#endif
