# Mini Kernel Simulator — Complete Delivery

Current files from the project workspace.

## CMakeLists.txt

````cmake
cmake_minimum_required(VERSION 3.10)
project(MiniKernelSimulator LANGUAGES CXX)

add_executable(mini_kernel src/main.cpp)
target_include_directories(mini_kernel PRIVATE include)
target_compile_features(mini_kernel PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(mini_kernel PRIVATE /W4)
else()
    target_compile_options(mini_kernel PRIVATE -Wall -Wextra)
endif()
````

## README.md

````markdown
# Mini Kernel Simulator

## Problem statement

Kernel process, CPU, memory, and resource decisions are hard to see in a real OS. This C++17 console program makes each decision visible through deterministic logs, tables, and text charts. It models textbook algorithms and does not control actual hardware.

## Review 1 scope

Implemented: PCB states and transitions; five CPU schedulers; FIFO, LRU, and Optimal page replacement; Banker's safety and resource requests; simulated semaphores, bounded-buffer producer-consumer, and dining philosophers; four disk schedulers. This is approximately 50% of the intended educational simulator. Future work can add I/O queues, process creation and termination controls, memory allocation, deadlock detection/recovery, and persistent experiment reports. Real threads, real devices, and a real kernel are outside this simulator's scope.

## Files

| File | Purpose |
| --- | --- |
| `include/pcb.h` | PCB and logged state transitions |
| `include/scheduler.h` | Tick-based CPU policies, Gantt and metrics |
| `include/memory.h` | Demand paging and replacement traces |
| `include/banker.h` | Safety and provisional resource requests |
| `include/sync.h` | Simulated semaphores and synchronization demos |
| `include/disk.h` | Disk head paths and seek comparison |
| `src/main.cpp` | Menu, sample data and input validation |
| `web/index.html` | Browser demo for CPU scheduling, paging and Banker; synchronization and disk are heading-only tabs |
| `docs/Module_Topics_Used.docx` | Concise faculty table of BCSE303L module numbers and project topics |

## Build and run

Linux/macOS with GCC:

```sh
g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp -o mini_kernel
./mini_kernel
```

Windows PowerShell with MinGW GCC:

```powershell
g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp -o mini_kernel.exe
.\mini_kernel.exe
```

CMake on either platform:

```sh
cmake -S . -B build
cmake --build build
```

Run `build/mini_kernel` on Linux/macOS or `build\Debug\mini_kernel.exe` (multi-config generators) / `build\mini_kernel.exe` (MinGW Makefiles) on Windows.

## Web interface

Open `web/index.html` in any modern browser. It is one HTML file with embedded CSS and JavaScript; no package installation or server is needed. CPU scheduling, paging and Banker's algorithm have working browser demos. Synchronization and disk scheduling appear only as headings for the next web phase. The browser does not execute the C++ binary.

## Sample checkpoints

- Menu 4, scheduler comparison with RR quantum 3: FCFS `11.00/16.60/11.00/5`, SJF `7.80/13.40/7.80/5`, SRTF `6.40/12.00/3.80/7`, Priority `9.20/14.80/7.40/6`, RR `14.20/19.80/4.20/11`. Values are average wait, turnaround, response, and dispatch count. The first dispatch is counted as a context switch for this project.
- Menu 6, page comparison with three frames: FIFO 15, LRU 12, Optimal 9 faults.
- Menu 8: safe sequence `P1 -> P3 -> P4 -> P0 -> P2`.
- Menu 9: P1 granted, P4 waits, P0 denied as unsafe.
- Menu 10: C1 blocks on empty; P1 wakes C1; P2 blocks on full; C2 wakes P2.
- Menu 11: all five deadlock with left-then-right forks; all five eat with resource ordering.
- Menu 13: FCFS 640, SSTF 236, SCAN 236, C-SCAN 382 cylinders.

SCAN starts toward cylinder zero. C-SCAN starts toward cylinder 199 and counts the return from 199 to 0. Averages divide movement by eight requests. The sample data is loaded at startup, so the comparison and demonstration options can run immediately.
# mini-kernel
````

## include/pcb.h

````cpp
#ifndef MINI_KERNEL_PCB_H
#define MINI_KERNEL_PCB_H

#include <iostream>
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

inline void setState(PCB& process, State next, int time, bool verbose = true,
                     std::ostream& out = std::cout) {
    if (process.state == next) return;
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
````

## include/scheduler.h

````cpp
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
    Algorithm algorithm = Algorithm::FCFS;
    std::vector<PCB> processes;
    std::vector<int> ticks;
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

}
#endif
````

## include/memory.h

````cpp
#ifndef MINI_KERNEL_MEMORY_H
#define MINI_KERNEL_MEMORY_H

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace memory {

enum class Algorithm { FIFO, LRU, OPTIMAL };
inline const char* name(Algorithm a) {
    if (a == Algorithm::FIFO) return "FIFO";
    if (a == Algorithm::LRU) return "LRU";
    return "Optimal";
}

struct Result { int faults = 0, hits = 0; double faultRate = 0; };

inline Result run(const std::vector<int>& references, int frameCount, Algorithm algorithm,
                  bool verbose = true, std::ostream& out = std::cout) {
    if (frameCount <= 0) throw std::invalid_argument("Frame count must be positive");
    for (int page : references)
        if (page < 0) throw std::invalid_argument("Page numbers cannot be negative");
    std::vector<int> frames(frameCount, -1), lastUsed(frameCount, -1);
    int fifoNext = 0;
    Result result;
    if (verbose) out << "\n" << name(algorithm) << " page replacement\nStep Page Frames Hit/Fault Evicted\n";
    for (std::size_t i = 0; i < references.size(); ++i) {
        int page = references[i], slot = -1, evicted = -1;
        for (int j = 0; j < frameCount; ++j)
            if (frames[j] == page) { slot = j; break; }
        bool hit = slot >= 0;
        if (hit) ++result.hits;
        else {
            ++result.faults;
            for (int j = 0; j < frameCount; ++j)
                if (frames[j] == -1) { slot = j; break; }
            if (slot < 0 && algorithm == Algorithm::FIFO) slot = fifoNext;
            if (slot < 0 && algorithm == Algorithm::LRU) {
                slot = 0;
                for (int j = 1; j < frameCount; ++j) {
                    if (lastUsed[j] < lastUsed[slot]) slot = j;
                }
            }
            if (slot < 0 && algorithm == Algorithm::OPTIMAL) {
                int farthest = -1;
                for (int j = 0; j < frameCount; ++j) {
                    int next = static_cast<int>(references.size());
                    for (std::size_t k = i + 1; k < references.size(); ++k)
                        if (references[k] == frames[j]) { next = static_cast<int>(k); break; }
                    if (next > farthest) { farthest = next; slot = j; }
                }
            }
            evicted = frames[slot];
            frames[slot] = page;
            if (algorithm == Algorithm::FIFO) fifoNext = (slot + 1) % frameCount;
        }
        lastUsed[slot] = static_cast<int>(i);
        if (verbose) {
            out << std::setw(4) << i + 1 << std::setw(5) << page << "  [";
            for (int j = 0; j < frameCount; ++j)
                out << (j ? " " : "") << (frames[j] < 0 ? "-" : std::to_string(frames[j]));
            out << "] " << (hit ? "HIT       " : "PAGE FAULT") << "   ";
            if (evicted < 0) out << "-"; else out << evicted;
            out << '\n';
        }
    }
    if (!references.empty()) result.faultRate = 100.0 * result.faults / references.size();
    if (verbose) out << "Faults: " << result.faults << ", hits: " << result.hits
                     << ", fault rate: " << std::fixed << std::setprecision(2)
                     << result.faultRate << "%\n";
    return result;
}

inline void compare(const std::vector<int>& references, int frameCount,
                    std::ostream& out = std::cout) {
    out << "\nAlgorithm Faults Hits Fault rate\n";
    for (Algorithm a : {Algorithm::FIFO, Algorithm::LRU, Algorithm::OPTIMAL}) {
        Result r = run(references, frameCount, a, false, out);
        out << std::left << std::setw(10) << name(a) << std::right << std::setw(6)
            << r.faults << std::setw(5) << r.hits << std::setw(10)
            << std::fixed << std::setprecision(2) << r.faultRate << "%\n";
    }
}

inline std::vector<int> sampleReferences() {
    return {7,0,1,2,0,3,0,4,2,3,0,3,2,1,2,0,1,7,0,1};
}

}
#endif
````

## include/banker.h

````cpp
#ifndef MINI_KERNEL_BANKER_H
#define MINI_KERNEL_BANKER_H

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace banker {
using Vector = std::vector<int>;
using Matrix = std::vector<Vector>;

struct Safety { bool safe = false; std::vector<int> sequence; };
enum class Decision { GRANTED, WAIT, DENIED };

class Banker {
    Vector available_;
    Matrix maximum_, allocation_, need_;
public:
    Banker(Vector available, Matrix maximum, Matrix allocation)
        : available_(available), maximum_(maximum), allocation_(allocation) {
        if (available_.empty() || maximum_.empty() || maximum_.size() != allocation_.size())
            throw std::invalid_argument("Invalid Banker matrix sizes");
        for (int x : available_) if (x < 0) throw std::invalid_argument("Negative available resource");
        need_.resize(maximum_.size(), Vector(available_.size()));
        for (std::size_t i = 0; i < maximum_.size(); ++i) {
            if (maximum_[i].size() != available_.size() ||
                allocation_[i].size() != available_.size())
                throw std::invalid_argument("Resource column count differs");
            for (std::size_t j = 0; j < available_.size(); ++j) {
                if (allocation_[i][j] < 0 || maximum_[i][j] < allocation_[i][j])
                    throw std::invalid_argument("Allocation exceeds maximum");
                need_[i][j] = maximum_[i][j] - allocation_[i][j];
            }
        }
    }

    Safety safety(bool verbose = true, std::ostream& out = std::cout) const {
        Vector work = available_;
        std::vector<bool> finished(need_.size(), false);
        Safety answer;
        if (verbose) { out << "Available/Work:"; for (int x : work) out << ' ' << x; out << '\n'; }
        std::size_t nextScan = 0;
        while (answer.sequence.size() < need_.size()) {
            int chosen = -1;
            for (std::size_t offset = 0; offset < need_.size(); ++offset) {
                std::size_t i = (nextScan + offset) % need_.size();
                if (finished[i]) continue;
                bool fits = true;
                for (std::size_t j = 0; j < work.size(); ++j)
                    if (need_[i][j] > work[j]) fits = false;
                if (fits) { chosen = static_cast<int>(i); break; }
            }
            if (chosen < 0) break;
            finished[chosen] = true;
            nextScan = (static_cast<std::size_t>(chosen) + 1) % need_.size();
            answer.sequence.push_back(chosen);
            if (verbose) out << "P" << chosen << " can finish; Work becomes";
            for (std::size_t j = 0; j < work.size(); ++j) {
                work[j] += allocation_[chosen][j];
                if (verbose) out << ' ' << work[j];
            }
            if (verbose) out << '\n';
        }
        answer.safe = answer.sequence.size() == need_.size();
        if (verbose) {
            out << (answer.safe ? "SAFE sequence: " : "UNSAFE; partial sequence: ");
            for (std::size_t i = 0; i < answer.sequence.size(); ++i)
                out << (i ? " -> " : "") << "P" << answer.sequence[i];
            out << '\n';
        }
        return answer;
    }

    Decision request(std::size_t pid, const Vector& amount, bool verbose = true,
                     std::ostream& out = std::cout) {
        if (pid >= need_.size() || amount.size() != available_.size())
            throw std::invalid_argument("Invalid request PID or vector length");
        for (std::size_t j = 0; j < amount.size(); ++j) {
            if (amount[j] < 0 || amount[j] > need_[pid][j]) {
                if (verbose) out << "P" << pid << " request DENIED: exceeds remaining Need\n";
                return Decision::DENIED;
            }
        }
        for (std::size_t j = 0; j < amount.size(); ++j)
            if (amount[j] > available_[j]) {
                if (verbose) out << "P" << pid << " request WAIT: resources unavailable\n";
                return Decision::WAIT;
            }
        Banker trial = *this;
        for (std::size_t j = 0; j < amount.size(); ++j) {
            trial.available_[j] -= amount[j];
            trial.allocation_[pid][j] += amount[j];
            trial.need_[pid][j] -= amount[j];
        }
        if (!trial.safety(false).safe) {
            if (verbose) out << "P" << pid << " request DENIED: provisional state is unsafe\n";
            return Decision::DENIED;
        }
        *this = trial;
        if (verbose) out << "P" << pid << " request GRANTED; state remains safe\n";
        return Decision::GRANTED;
    }
};

inline Banker sample() {
    return Banker({3,3,2}, {{7,5,3},{3,2,2},{9,0,2},{2,2,2},{4,3,3}},
                  {{0,1,0},{2,0,0},{3,0,2},{2,1,1},{0,0,2}});
}

inline void requestDemo(std::ostream& out = std::cout) {
    Banker b = sample();
    b.request(1, {1,0,2}, true, out);
    b.request(4, {3,3,0}, true, out);
    b.request(0, {0,2,0}, true, out);
}
}
#endif
````

## include/sync.h

````cpp
#ifndef MINI_KERNEL_SYNC_H
#define MINI_KERNEL_SYNC_H

#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>

namespace syncsim {

class Semaphore {
    int permits_;
    std::deque<std::string> blocked_;
public:
    explicit Semaphore(int permits) : permits_(permits) {
        if (permits < 0) throw std::invalid_argument("Negative semaphore value");
    }

    bool wait(const std::string& process) {
        if (permits_ > 0) {
            --permits_;
            return true;
        }
        blocked_.push_back(process);
        return false;
    }

    std::string signal() {
        if (!blocked_.empty()) {
            std::string process = blocked_.front();
            blocked_.pop_front();
            return process;
        }
        ++permits_;
        return "";
    }

    int value() const { return permits_; }
    const std::deque<std::string>& blocked() const { return blocked_; }
};

class Mutex {
    Semaphore gate_{1};
public:
    bool lock(const std::string& process) { return gate_.wait(process); }
    std::string unlock() { return gate_.signal(); }
    int value() const { return gate_.value(); }
};

class ProducerConsumer {
    std::deque<int> buffer_;
    Semaphore empty_;
    Semaphore full_{0};
    Mutex mutex_;
    bool verbose_;
    std::ostream& out_;
    int step_ = 0;

    void show(const std::string& message) {
        if (!verbose_) return;
        out_ << "Step " << ++step_ << ": " << message << " | buffer=[";
        for (std::size_t i = 0; i < buffer_.size(); ++i) {
            if (i > 0) out_ << ",";
            out_ << buffer_[i];
        }
        out_ << "] empty=" << empty_.value()
             << " full=" << full_.value()
             << " mutex=" << mutex_.value() << '\n';
    }

public:
    ProducerConsumer(int capacity, bool verbose, std::ostream& out)
        : empty_(capacity), verbose_(verbose), out_(out) {
        if (capacity <= 0) throw std::invalid_argument("Buffer capacity must be positive");
        show("initial state");
    }

    std::string produce(const std::string& process, int item, bool hasPermit = false) {
        if (!hasPermit && !empty_.wait(process)) {
            show(process + " BLOCKED: buffer full");
            return "";
        }
        show(process + " gets an empty slot");
        if (!mutex_.lock(process)) throw std::logic_error("Mutex is busy");
        show(process + " locks mutex");
        buffer_.push_back(item);
        show(process + " produces " + std::to_string(item));
        mutex_.unlock();
        show(process + " unlocks mutex");
        std::string woken = full_.signal();
        std::string message = process + " signals full";
        if (!woken.empty()) message += "; wakes " + woken;
        show(message);
        return woken;
    }

    std::string consume(const std::string& process, bool hasPermit = false) {
        if (!hasPermit && !full_.wait(process)) {
            show(process + " BLOCKED: buffer empty");
            return "";
        }
        show(process + " gets a full slot");
        if (!mutex_.lock(process)) throw std::logic_error("Mutex is busy");
        show(process + " locks mutex");
        int item = buffer_.front();
        buffer_.pop_front();
        show(process + " consumes " + std::to_string(item));
        mutex_.unlock();
        show(process + " unlocks mutex");
        std::string woken = empty_.signal();
        std::string message = process + " signals empty";
        if (!woken.empty()) message += "; wakes " + woken;
        show(message);
        return woken;
    }
};

inline void producerConsumerDemo(bool verbose = true, std::ostream& out = std::cout) {
    ProducerConsumer demo(2, verbose, out);
    demo.consume("C1");
    std::string woken = demo.produce("P1", 10);
    if (woken == "C1") demo.consume("C1", true);
    demo.produce("P1", 20);
    demo.produce("P1", 30);
    demo.produce("P2", 40);
    woken = demo.consume("C2");
    if (woken == "P2") demo.produce("P2", 40, true);
    demo.consume("C2");
    demo.consume("C2");
}

inline void diningDeadlock(bool verbose = true, std::ostream& out = std::cout) {
    if (!verbose) return;
    out << "\nDining philosophers: left fork first\n";
    int step = 0;
    for (int p = 0; p < 5; ++p)
        out << "Step " << ++step << ": P" << p << " takes left fork " << p << '\n';
    for (int p = 0; p < 5; ++p)
        out << "Step " << ++step << ": P" << p << " BLOCKED on right fork "
            << (p + 1) % 5 << " held by P" << (p + 1) % 5 << '\n';
    out << "DEADLOCK: all five hold one fork and wait in a cycle.\n";
}

inline void diningFixed(bool verbose = true, std::ostream& out = std::cout) {
    if (!verbose) return;
    out << "\nDining philosophers: lower-numbered fork first\n";
    int step = 0;
    for (int p = 0; p < 4; ++p)
        out << "Step " << ++step << ": P" << p << " takes fork " << p << '\n';
    out << "Step " << ++step << ": P4 BLOCKED on fork 0 held by P0\n";
    for (int p = 0; p < 3; ++p)
        out << "Step " << ++step << ": P" << p << " BLOCKED on fork "
            << p + 1 << " held by P" << p + 1 << '\n';
    out << "Step " << ++step << ": P3 takes fork 4\n";
    for (int p = 3; p >= 0; --p) {
        out << "Step " << ++step << ": P" << p << " eats and releases forks "
            << p << " and " << p + 1 << '\n';
        if (p > 0)
            out << "Step " << ++step << ": P" << p - 1
                << " wakes and takes fork " << p << '\n';
    }
    out << "Step " << ++step << ": P4 wakes and takes fork 0\n";
    out << "Step " << ++step << ": P4 takes fork 4\n";
    out << "Step " << ++step << ": P4 eats and releases forks 0 and 4\n";
    out << "All five eat; resource ordering prevents circular wait.\n";
}

inline void diningDemo(bool verbose = true, std::ostream& out = std::cout) {
    diningDeadlock(verbose, out);
    diningFixed(verbose, out);
}

}
#endif
````

## include/disk.h

````cpp
#ifndef MINI_KERNEL_DISK_H
#define MINI_KERNEL_DISK_H

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace disk {
struct Result {
    std::string algorithm;
    std::vector<int> serviceOrder;
    std::vector<int> headPath;
    int totalMovement = 0;
    double averageSeek = 0.0;
};
inline void validate(const std::vector<int>& requests, int head, int diskSize) {
    if (diskSize <= 0 || head < 0 || head >= diskSize)
        throw std::invalid_argument("Disk size/head position is invalid");
    for (int request : requests)
        if (request < 0 || request >= diskSize)
            throw std::invalid_argument("Request lies outside the disk");
}
inline void moveTo(Result& result, int cylinder) {
    int previous = result.headPath.back();
    result.totalMovement += (previous > cylinder ? previous - cylinder : cylinder - previous);
    if (previous != cylinder) result.headPath.push_back(cylinder);
}
inline void service(Result& result, int cylinder) {
    moveTo(result, cylinder);
    result.serviceOrder.push_back(cylinder);
}
inline Result schedule(const std::vector<int>& requests, int head, int diskSize,
                       const std::string& algorithm, bool verbose = true,
                       std::ostream& out = std::cout) {
    validate(requests, head, diskSize);
    Result result{algorithm, {}, {head}, 0, 0.0};

    if (algorithm == "FCFS") {
        for (int request : requests) service(result, request);
    } else if (algorithm == "SSTF") {
        std::vector<bool> done(requests.size(), false);
        for (std::size_t count = 0; count < requests.size(); ++count) {
            std::size_t best = requests.size();
            int bestDistance = diskSize;
            for (std::size_t i = 0; i < requests.size(); ++i) {
                if (done[i]) continue;
                int distance = std::abs(requests[i] - result.headPath.back());
                if (best == requests.size() || distance < bestDistance ||
                    (distance == bestDistance && requests[i] < requests[best])) {
                    best = i;
                    bestDistance = distance;
                }
            }
            done[best] = true;
            service(result, requests[best]);
        }
    } else if (algorithm == "SCAN" || algorithm == "C-SCAN") {
        std::vector<int> high, low;
        for (int request : requests) {
            if (request >= head) high.push_back(request);
            else low.push_back(request);
        }
        std::sort(high.begin(), high.end());
        std::sort(low.begin(), low.end());
        if (algorithm == "SCAN") {
            for (int i = static_cast<int>(low.size()) - 1; i >= 0; --i)
                service(result, low[i]);
            if (!high.empty()) moveTo(result, 0);
            for (int request : high) service(result, request);
        } else {
            for (int request : high) service(result, request);
            if (!low.empty()) {
                moveTo(result, diskSize - 1);
                moveTo(result, 0);
                for (int request : low) service(result, request);
            }
        }
    } else {
        throw std::invalid_argument("Unknown disk scheduling algorithm");
    }

    if (!requests.empty())
        result.averageSeek = static_cast<double>(result.totalMovement) / requests.size();
    if (verbose) {
        out << "\n" << result.algorithm << " disk scheduling\nService order: ";
        if (result.serviceOrder.empty()) out << "(none)";
        for (std::size_t i = 0; i < result.serviceOrder.size(); ++i)
            out << (i ? " -> " : "") << result.serviceOrder[i];
        out << "\nHead path:     ";
        for (std::size_t i = 0; i < result.headPath.size(); ++i)
            out << (i ? " -> " : "") << result.headPath[i];
        out << "\nTotal head movement: " << result.totalMovement
            << "\nAverage seek/request: " << std::fixed << std::setprecision(2)
            << result.averageSeek << "\n";
    }
    return result;
}
inline void compare(const std::vector<int>& requests, int head, int diskSize,
                    bool verbose = true, std::ostream& out = std::cout) {
    if (!verbose) return;
    out << "\nAlgorithm  Total movement  Avg seek/request\n";
    for (const std::string& name : {"FCFS", "SSTF", "SCAN", "C-SCAN"}) {
        Result result = schedule(requests, head, diskSize, name, false, out);
        out << std::left << std::setw(11) << name << std::right << std::setw(14)
            << result.totalMovement << std::setw(19) << std::fixed
            << std::setprecision(2) << result.averageSeek << "\n";
    }
}
inline std::vector<int> sampleRequests() { return {98, 183, 37, 122, 14, 124, 65, 67}; }

}

#endif
````

## src/main.cpp

````cpp
#include "banker.h"
#include "disk.h"
#include "memory.h"
#include "pcb.h"
#include "scheduler.h"
#include "sync.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

int parseInteger(const std::string& line, int minimum, int maximum) {
    std::istringstream in(line);
    int value;
    std::string extra;
    if (!(in >> value) || (in >> extra) || value < minimum || value > maximum)
        throw std::invalid_argument("Enter a whole number in the stated range");
    return value;
}

int askInteger(const std::string& prompt, int minimum, int maximum,
               int defaultValue = -1) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) throw std::runtime_error("Input ended");
        if (line.empty() && defaultValue >= minimum) return defaultValue;
        try { return parseInteger(line, minimum, maximum); }
        catch (const std::invalid_argument& error) { std::cout << error.what() << '\n'; }
    }
}

std::vector<int> askReferences() {
    int count = askInteger("Number of references (1..100): ", 1, 100);
    while (true) {
        std::cout << "Enter " << count << " nonnegative page numbers, space separated: ";
        std::string line; if (!std::getline(std::cin, line)) throw std::runtime_error("Input ended");
        std::istringstream input(line);
        std::vector<int> values;
        int page;
        while (input >> page) values.push_back(page);
        bool valid = input.eof() && values.size() == static_cast<std::size_t>(count);
        for (int value : values) if (value < 0) valid = false;
        if (valid) return values;
        std::cout << "Invalid reference list. Try again.\n";
    }
}

std::vector<PCB> askProcesses() {
    int count = askInteger("Number of processes (1..20): ", 1, 20);
    std::vector<PCB> processes;
    for (int i = 0; i < count;) {
        std::cout << "Process " << i + 1 << " (PID name arrival burst priority): ";
        std::string line; if (!std::getline(std::cin, line)) throw std::runtime_error("Input ended");
        std::istringstream in(line);
        int pid, arrival, burst, priority; std::string name, extra;
        if ((in >> pid >> name >> arrival >> burst >> priority) && !(in >> extra) &&
            pid > 0 && arrival >= 0 && burst > 0 && priority >= 0) {
            bool duplicate = false;
            for (const PCB& p : processes) if (p.pid == pid) duplicate = true;
            if (!duplicate) {
                processes.push_back({pid, name, arrival, burst, burst, priority});
                ++i; continue;
            }
        }
        std::cout << "Invalid process or duplicate PID. Try again.\n";
    }
    return processes;
}

void menu() {
    std::cout << "\n=== MINI KERNEL SIMULATOR ===\n"
              << " 1. Enter processes manually\n 2. Load sample processes\n"
              << " 3. Run scheduler (trace + Gantt)\n 4. Compare schedulers\n"
              << " 5. Run page replacement\n 6. Compare page replacement\n"
              << " 7. Set reference string and frames\n 8. Banker's safety check\n"
              << " 9. Banker's request demo\n10. Producer-consumer demo\n"
              << "11. Dining philosophers demo\n12. Disk scheduling sample paths\n"
              << "13. Compare disk schedulers\n14. Exit\nChoice: ";
}

int main() {
    std::vector<PCB> processes = sampleProcesses();
    std::vector<int> references = memory::sampleReferences();
    int frames = 3;
    while (true) {
        menu();
        std::string line;
        if (!std::getline(std::cin, line)) break;
        int choice;
        try { choice = parseInteger(line, 1, 14); }
        catch (const std::invalid_argument&) { std::cout << "Invalid menu choice.\n"; continue; }
        try {
            switch (choice) {
                case 1:
                    processes = askProcesses();
                    std::cout << "Loaded " << processes.size() << " processes.\n";
                    break;
                case 2:
                    processes = sampleProcesses();
                    std::cout << "Loaded five sample processes.\n";
                    break;
                case 3: {
                    int a = askInteger("Algorithm 1=FCFS 2=SJF 3=SRTF 4=Priority 5=RR [1]: ", 1, 5, 1);
                    int quantum = 3;
                    if (a == 5) quantum = askInteger("RR quantum [3]: ", 1, 1000, 3);
                    cpu::run(processes, static_cast<cpu::Algorithm>(a - 1), quantum);
                    break;
                }
                case 4:
                    cpu::compare(processes, 3);
                    break;
                case 5: {
                    int a = askInteger("Algorithm 1=FIFO 2=LRU 3=Optimal [1]: ", 1, 3, 1);
                    memory::run(references, frames, static_cast<memory::Algorithm>(a - 1));
                    break;
                }
                case 6:
                    memory::compare(references, frames);
                    break;
                case 7:
                    references = askReferences();
                    frames = askInteger("Frame count (1..20): ", 1, 20);
                    std::cout << "Reference string and frames updated.\n";
                    break;
                case 8:
                    banker::sample().safety();
                    break;
                case 9:
                    banker::requestDemo();
                    break;
                case 10:
                    syncsim::producerConsumerDemo();
                    break;
                case 11:
                    syncsim::diningDemo();
                    break;
                case 12: {
                    std::vector<int> requests = disk::sampleRequests();
                    std::cout << "Requests: 98 183 37 122 14 124 65 67; head=53; disk size=200\n";
                    for (const std::string& a : {"FCFS", "SSTF", "SCAN", "C-SCAN"})
                        disk::schedule(requests, 53, 200, a);
                    break;
                }
                case 13:
                    disk::compare(disk::sampleRequests(), 53, 200);
                    break;
                case 14:
                    return 0;
            }
        } catch (const std::exception& error) {
            std::cout << "Error: " << error.what() << '\n';
            if (!std::cin) break;
        }
    }
    return 0;
}
````

## Build and run

**Linux:**

```sh
g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp -o mini_kernel
./mini_kernel
```

**Windows PowerShell with MinGW:**

```powershell
g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp -o mini_kernel.exe
.\mini_kernel.exe
```

**CMake on either platform:**

```sh
cmake -S . -B build
cmake --build build
```

## Test checklist

| Menu option | Select / enter | Expected result |
| --- | --- | --- |
| 1 | Enter `1`, then `1`, then `6 test 0 2 1` | One custom process loaded; option 4 gives wait 0. |
| 2 | Enter `2` | Five sample processes loaded. |
| 3 | Enter `3`, then `1`–`5`; for RR enter quantum `3` | Tick trace, Gantt, process table, averages; option 3 with FCFS shows 11.00/16.60/11.00 and 5 dispatches. |
| 4 | Enter `4` | FCFS 11.00/16.60/11.00/5; SJF 7.80/13.40/7.80/5; SRTF 6.40/12.00/3.80/7; Priority 9.20/14.80/7.40/6; RR 14.20/19.80/4.20/11. |
| 5 | Enter `5`, then `1`–`3` | Step table; FIFO 15, LRU 12, Optimal 9 faults with default data. |
| 6 | Enter `6` | FIFO 15, LRU 12, Optimal 9 faults. |
| 7 | Enter `7`, then `3`, then `1 2 3`, then `2` | Custom references and frame count saved; option 6 uses them. Restore defaults with a fresh run. |
| 8 | Enter `8` | SAFE: P1 → P3 → P4 → P0 → P2. |
| 9 | Enter `9` | P1 granted; P4 waits; P0 denied because provisional state is unsafe. |
| 10 | Enter `10` | C1 blocks on empty, P1 wakes C1; P2 blocks on full, C2 wakes P2. |
| 11 | Enter `11` | Left-first policy deadlocks; ordered forks let all five eat. |
| 12 | Enter `12` | Four routes; total movement 640, 236, 236, 382. |
| 13 | Enter `13` | FCFS 640/80.00; SSTF 236/29.50; SCAN 236/29.50; C-SCAN 382/47.75. |
| 14 | Enter `14` | Program exits. |

The four scheduler values are average waiting, turnaround, response, and dispatch count. Non-numeric menu input prints an error and returns to the menu. Defaults are loaded at startup; Enter accepts FCFS/FIFO and RR quantum 3 when those prompts appear.

## 10–12 minute lab demo script

1. **0:00–1:00 — Introduce the problem.** Explain that internal kernel decisions are invisible in normal systems, so the simulator exposes them with a deterministic clock and logs.
2. **1:00–2:00 — Show the menu and PCB model.** Choose 2. Point out PID, arrival, burst, priority, and transition log format.
3. **2:00–4:00 — CPU scheduling.** Choose 3, then 3 for SRTF. Explain tick admission, preemption, response time, Gantt segments, and dispatch count. Choose 4 and compare all five policies.
4. **4:00–5:30 — Demand paging.** Choose 5, then 1 for FIFO. Explain frames, faults, and eviction. Choose 6 and compare 15/12/9 faults.
5. **5:30–7:00 — Banker.** Choose 8 for the safe sequence, then 9 to show grant, wait, and unsafe denial. Explain the provisional allocation test.
6. **7:00–9:00 — Synchronization.** Choose 10. Point out empty/full/mutex values, blocked queue, and permit handoff. Choose 11 to contrast circular wait with ordered fork acquisition.
7. **9:00–10:30 — Disk scheduling.** Choose 12 to show paths, then 13 for totals. Explain why SCAN toward 0 totals 236 and why C-SCAN counts its return jump.
8. **10:30–12:00 — Close.** Mention limits: one-thread deterministic simulation, no real device I/O; highlight planned I/O and memory-allocation modules. Exit with 14.

## 15 likely viva questions

1. **What is a PCB?** A record of a process’s identity, scheduling data, state, and measured times.
2. **Why simulate ticks?** Each tick makes arrivals, preemption, execution, and completion observable.
3. **What is response time?** First CPU start minus arrival time.
4. **What is turnaround time?** Completion time minus arrival time.
5. **What is waiting time?** Turnaround time minus CPU burst in this CPU-only model.
6. **What is preemption?** Taking the CPU from an unfinished process when policy or quantum requires it.
7. **Why can SJF starve jobs?** New short jobs can repeatedly overtake a long job.
8. **What does a semaphore value mean?** The number of permits immediately available; a blocked queue holds waiting processes.
9. **How does the mutex work?** It is a binary semaphore initialized to one, permitting one critical-section owner.
10. **Why do producer and consumer use empty and full?** They prevent overflow and underflow of the bounded buffer.
11. **Why does left-then-right dining deadlock?** Each philosopher holds one fork and waits for the next, forming circular wait.
12. **How does resource ordering fix dining?** Every philosopher takes the lower-numbered fork first, preventing a wait cycle.
13. **What is a safe state in Banker’s algorithm?** Some order exists in which every process can obtain its remaining need and finish.
14. **How does SSTF choose a request?** It services the outstanding cylinder closest to the current head; it may starve distant requests.
15. **How do SCAN and C-SCAN differ?** SCAN reverses direction at an edge; C-SCAN returns to the starting edge and continues in one service direction.

## Review 1 write-up

**Problem statement.** Understanding process, CPU, memory, synchronization, deadlock, and disk decisions is difficult because a real operating system performs them internally. This simulator exposes those decisions as readable traces and tables.

**Scope.** In scope: deterministic console demonstrations of PCB transitions, five CPU policies, three page policies, Banker safety and requests, two synchronization problems, and four disk policies. Out of scope: real kernel control, real threads, hardware I/O, and persistence.

**Modules.** `pcb.h` owns process state; `scheduler.h` simulates CPU ticks; `memory.h` replaces pages; `banker.h` checks safe allocation; `sync.h` demonstrates semaphore handoff and deadlock prevention; `disk.h` calculates service paths; `main.cpp` drives the menu.

**Completion and plan.** This is a working, approximately 50% Review 1 build. Future additions can include I/O queues, memory allocation, deadlock detection/recovery, process-control actions, and exportable experiment reports.

**Verification.** The direct GCC-compatible C++17 command compiled with `-Wall -Wextra` and no warnings. Every stated CPU, paging, Banker, and disk checkpoint matched. CMake was not available in the verification environment, so its build command could not be run here.

## web/index.html

````html
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Mini Kernel Simulator</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Instrument+Sans:wght@400;500;600;700&family=JetBrains+Mono:wght@400;500;700&display=swap">
<style>
:root {
  --bg: #f1f4f7;
  --surface: #ffffff;
  --fg: #17202a;
  --muted: #566372;
  --line: #d6dce3;
  --accent: #1f5fbf;
  --accent-fg: #ffffff;
  --good: #16794a;
  --bad: #b3261e;
  --warn: #8a5a00;
  --idle: #e3e8ee;
  --p0: #2f6fba; --p1: #b4532a; --p2: #2e7d57; --p3: #8a4fbf;
  --p4: #a06a00; --p5: #c2415d; --p6: #0f7b8a; --p7: #5b6573;
  --font-body: "Instrument Sans", system-ui, -apple-system, "Segoe UI", sans-serif;
  --font-data: "JetBrains Mono", ui-monospace, "Cascadia Mono", Consolas, monospace;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #0e1318; --surface: #161c23; --fg: #e5eaf0; --muted: #98a4b1; --line: #2a333d;
    --accent: #6aa3ff; --accent-fg: #0b1320; --good: #4cc790; --bad: #ff8a80; --warn: #e0b04a; --idle: #232b34;
    color-scheme: dark;
  }
}
:root[data-theme="dark"] {
  --bg: #0e1318; --surface: #161c23; --fg: #e5eaf0; --muted: #98a4b1; --line: #2a333d;
  --accent: #6aa3ff; --accent-fg: #0b1320; --good: #4cc790; --bad: #ff8a80; --warn: #e0b04a; --idle: #232b34;
  color-scheme: dark;
}
* { box-sizing: border-box; }
[hidden] { display: none !important; }
body {
  background: var(--bg); color: var(--fg); font-family: var(--font-body); font-size: 15px; line-height: 1.5;
  padding-block: 20px 40px;
  padding-left: max(16px, env(safe-area-inset-left, 0px));
  padding-right: max(16px, env(safe-area-inset-right, 0px));
}
.wrap { max-width: 1080px; margin: 0 auto; display: grid; gap: 18px; }
h1 { font-size: 1.5rem; margin: 0; letter-spacing: -0.01em; text-wrap: balance; }
h2 { font-size: 0.78rem; margin: 0 0 10px; text-transform: uppercase; letter-spacing: 0.09em; color: var(--muted); font-weight: 600; }
p { margin: 0; }
.sub { color: var(--muted); margin-top: 4px; max-width: 65ch; }
.tabs { display: flex; gap: 6px; flex-wrap: wrap; border-bottom: 1px solid var(--line); }
.tab {
  font: inherit; font-weight: 600; background: none; color: var(--muted); border: 0; border-bottom: 3px solid transparent;
  padding: 10px 14px; cursor: pointer; margin-bottom: -1px;
}
.tab[aria-selected="true"] { color: var(--fg); border-bottom-color: var(--accent); }
.tab:hover { color: var(--fg); }
:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
.panel { display: grid; gap: 16px; }
.card { background: var(--surface); border: 1px solid var(--line); border-radius: 8px; padding: 16px; min-width: 0; }
.row { display: flex; gap: 14px; flex-wrap: wrap; align-items: end; }
label { display: grid; gap: 4px; font-size: 0.8rem; color: var(--muted); font-weight: 500; }
input, select {
  font: inherit; font-family: var(--font-data); font-size: 0.9rem; color: var(--fg); background: var(--bg);
  border: 1px solid var(--line); border-radius: 6px; padding: 7px 9px; min-width: 0;
}
input[type="number"] { width: 5.2em; }
input.name { width: 8.5em; font-family: var(--font-body); }
input.wide { width: min(100%, 30em); }
button.btn {
  font: inherit; font-weight: 600; cursor: pointer; border-radius: 6px; padding: 8px 14px;
  background: var(--surface); color: var(--fg); border: 1px solid var(--line);
}
button.btn:hover { border-color: var(--accent); }
button.primary { background: var(--accent); color: var(--accent-fg); border-color: var(--accent); }
.scroll { overflow-x: auto; }
table { border-collapse: collapse; width: 100%; font-family: var(--font-data); font-size: 0.85rem; font-variant-numeric: tabular-nums; }
th, td { text-align: left; padding: 6px 10px; border-bottom: 1px solid var(--line); white-space: nowrap; }
th { color: var(--muted); font-weight: 500; font-size: 0.75rem; text-transform: uppercase; letter-spacing: 0.06em; }
td.num, th.num { text-align: right; }
.swatch { display: inline-block; width: 10px; height: 10px; border-radius: 2px; margin-right: 6px; }
.grid2 { display: grid; gap: 16px; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); }
@media (max-width: 760px) { .grid2 { grid-template-columns: minmax(0, 1fr); } }
.stats { display: flex; gap: 22px; flex-wrap: wrap; margin-top: 12px; }
.stat { display: grid; gap: 2px; }
.stat b { font-family: var(--font-data); font-size: 1.25rem; font-weight: 700; font-variant-numeric: tabular-nums; }
.stat span { color: var(--muted); font-size: 0.75rem; text-transform: uppercase; letter-spacing: 0.06em; }

.gantt-scroll { overflow-x: auto; padding-bottom: 6px; }
.gantt { position: relative; height: 44px; background: var(--idle); border-radius: 4px; }
.seg {
  position: absolute; top: 0; height: 44px; display: flex; align-items: center; justify-content: center;
  color: #fff; font-family: var(--font-data); font-weight: 700; font-size: 0.8rem;
  border-right: 2px solid var(--surface); overflow: hidden;
}
.seg.idle { background: var(--idle); color: var(--muted); font-weight: 500; }
.ruler { position: relative; height: 20px; margin-top: 4px; font-family: var(--font-data); font-size: 0.68rem; color: var(--muted); }
.ruler span { position: absolute; top: 0; transform: translateX(-50%); font-variant-numeric: tabular-nums; }
.ruler span:first-child { transform: none; }
.player { display: flex; gap: 12px; align-items: center; margin-top: 12px; flex-wrap: wrap; }
.player input[type="range"] { flex: 1 1 200px; padding: 0; accent-color: var(--accent); }
.clock { font-family: var(--font-data); font-weight: 700; min-width: 5.5em; }

.log { font-family: var(--font-data); font-size: 0.82rem; max-height: 300px; overflow: auto; display: grid; gap: 2px; }
.log div { padding: 2px 6px; border-radius: 4px; white-space: nowrap; }
.log .t { color: var(--muted); display: inline-block; min-width: 4.4em; }
.log .last { background: var(--idle); }
.st-RUNNING { color: var(--good); font-weight: 700; }
.st-TERMINATED { color: var(--bad); font-weight: 700; }
.st-READY { color: var(--warn); font-weight: 700; }
.st-NEW { color: var(--muted); font-weight: 700; }
tr.best td { color: var(--good); font-weight: 700; }

td.fault, td.hit { text-align: center; font-weight: 700; }
td.fault { color: var(--bad); }
td.hit { color: var(--good); }
.pg td, .pg th { text-align: center; padding: 6px 8px; }
.pg td.rowhead, .pg th.rowhead { text-align: left; color: var(--muted); }
.pg td.cell.changed { background: var(--idle); color: var(--accent); font-weight: 700; }
.bars { display: grid; gap: 10px; }
.barrow { display: grid; grid-template-columns: 5.5em minmax(0, 1fr) 7em; gap: 10px; align-items: center; font-family: var(--font-data); font-size: 0.85rem; }
.bartrack { background: var(--idle); border-radius: 4px; height: 18px; }
.barfill { height: 18px; border-radius: 4px; background: var(--accent); }
.note { color: var(--muted); font-size: 0.85rem; max-width: 70ch; }
.msg { margin-top: 12px; padding: 10px 12px; border-radius: 6px; border: 1px solid var(--line); background: var(--bg); font-size: 0.9rem; }
.msg.ok { border-color: var(--good); }
.msg.no { border-color: var(--bad); }
.msg.wait { border-color: var(--warn); }
.seq { display: flex; gap: 6px; align-items: center; flex-wrap: wrap; font-family: var(--font-data); font-weight: 700; margin-top: 8px; }
.seq i { font-style: normal; color: var(--muted); }
.pill { padding: 3px 9px; border-radius: 999px; background: var(--idle); }
.pill.bad { color: var(--bad); }
.pill.good { color: var(--good); }
@media (prefers-reduced-motion: reduce) { * { scroll-behavior: auto !important; transition: none !important; } }
</style>
</head>
<body>

<div class="wrap">
  <header>
    <h1>Mini Kernel Simulator</h1>
    <p class="sub">Watch how an operating system kernel schedules processes, replaces memory pages and avoids deadlock. Change any value and the result updates.</p>
  </header>

  <div class="tabs" role="tablist" aria-label="Kernel modules">
    <button class="tab" role="tab" id="tabbtn-sched" aria-selected="true" aria-controls="tab-sched">CPU scheduling</button>
    <button class="tab" role="tab" id="tabbtn-page" aria-selected="false" aria-controls="tab-page">Memory paging</button>
    <button class="tab" role="tab" id="tabbtn-bank" aria-selected="false" aria-controls="tab-bank">Deadlock avoidance</button>
    <button class="tab" role="tab" id="tabbtn-sync" aria-selected="false" aria-controls="tab-sync">Synchronization</button>
    <button class="tab" role="tab" id="tabbtn-disk" aria-selected="false" aria-controls="tab-disk">Disk scheduling</button>
  </div>

  <section class="panel" id="tab-sched" role="tabpanel" aria-labelledby="tabbtn-sched">
    <div class="card">
      <h2>Processes (PCB table)</h2>
      <div class="scroll"><table id="proc-table"></table></div>
      <div class="row" style="margin-top:12px">
        <button class="btn" id="add-proc" type="button">Add process</button>
        <button class="btn" id="reset-proc" type="button">Reset to sample</button>
        <label>Algorithm
          <select id="algo">
            <option value="FCFS">FCFS</option>
            <option value="SJF">SJF (non-preemptive)</option>
            <option value="SRTF" selected>SRTF (preemptive SJF)</option>
            <option value="PRIORITY">Priority (preemptive)</option>
            <option value="RR">Round Robin</option>
          </select>
        </label>
        <label id="q-wrap" hidden>Time quantum
          <input type="number" id="quantum" value="3" min="1" max="20">
        </label>
      </div>
      <p class="note" style="margin-top:10px">Lower priority number means higher priority. Times are in clock ticks.</p>
    </div>

    <div class="card">
      <h2>Gantt chart</h2>
      <div class="gantt-scroll"><div id="gantt-box"></div></div>
      <div class="player">
        <button class="btn primary" id="play" type="button">Play</button>
        <input type="range" id="scrub" min="0" max="1" value="1" aria-label="Simulation time">
        <span class="clock" id="clock">t = 0</span>
      </div>
      <div class="stats" id="sched-stats"></div>
    </div>

    <div class="grid2">
      <div class="card">
        <h2>Per-process results</h2>
        <div class="scroll"><table id="result-table"></table></div>
      </div>
      <div class="card">
        <h2>Kernel log (state transitions)</h2>
        <div class="log" id="klog"></div>
      </div>
    </div>

    <div class="card">
      <h2>All algorithms on the same processes</h2>
      <div class="scroll"><table id="compare-table"></table></div>
      <p class="note" style="margin-top:10px">Round Robin uses the quantum above (3 if unset). Best average waiting time is highlighted.</p>
    </div>
  </section>

  <section class="panel" id="tab-page" role="tabpanel" aria-labelledby="tabbtn-page" hidden>
    <div class="card">
      <h2>Demand paging</h2>
      <div class="row">
        <label>Page reference string (space separated)
          <input class="wide" type="text" id="refs" value="7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1">
        </label>
        <label>Frames
          <input type="number" id="frames" value="3" min="1" max="8">
        </label>
        <label>Algorithm
          <select id="palgo">
            <option value="FIFO">FIFO</option>
            <option value="LRU">LRU</option>
            <option value="OPT">Optimal</option>
          </select>
        </label>
      </div>
    </div>
    <div class="card">
      <h2>Step by step</h2>
      <div class="scroll"><table class="pg" id="page-table"></table></div>
      <div class="stats" id="page-stats"></div>
    </div>
    <div class="card">
      <h2>Page faults by algorithm</h2>
      <div class="bars" id="page-bars"></div>
      <p class="note" style="margin-top:12px">Optimal evicts the page needed farthest in the future. A real kernel cannot know the future, so it is the lower bound that FIFO and LRU are measured against.</p>
    </div>
  </section>

  <section class="panel" id="tab-bank" role="tabpanel" aria-labelledby="tabbtn-bank" hidden>
    <div class="card">
      <h2>Banker's algorithm: system state</h2>
      <div class="scroll"><table id="bank-table"></table></div>
      <div id="bank-avail" class="note" style="margin-top:10px"></div>
      <div id="bank-seq"></div>
    </div>
    <div class="card">
      <h2>Make a resource request</h2>
      <div class="row">
        <label>Process
          <select id="req-pid"></select>
        </label>
        <label>A <input type="number" id="req-a" value="1" min="0"></label>
        <label>B <input type="number" id="req-b" value="0" min="0"></label>
        <label>C <input type="number" id="req-c" value="2" min="0"></label>
        <button class="btn primary" id="req-go" type="button">Request</button>
        <button class="btn" id="req-reset" type="button">Reset state</button>
      </div>
      <div id="bank-msg" role="status"></div>
      <p class="note" style="margin-top:12px">A request is granted only if the system stays in a safe state. Try P1 (1,0,2), then P4 (3,3,0), then P0 (0,2,0).</p>
    </div>
    <div class="card">
      <h2>Safety algorithm trace</h2>
      <div class="log" id="bank-trace"></div>
    </div>
  </section>
  <section class="panel" id="tab-sync" role="tabpanel" aria-labelledby="tabbtn-sync" hidden>
    <div class="card"><h2>Synchronization</h2></div>
  </section>
  <section class="panel" id="tab-disk" role="tabpanel" aria-labelledby="tabbtn-disk" hidden>
    <div class="card"><h2>Disk scheduling</h2></div>
  </section>
</div>

<script>
function runScheduler(input, algo, q) {
  const procs = input.map(p => Object.assign({}, p, { remaining: p.burst, state: 'NEW', finish: -1, response: -1 }));
  const n = procs.length, ready = [], gantt = [], log = [];
  let running = -1, done = 0, slice = 0, t = 0, last = -1, cs = 0;
  const setState = (i, s, time) => {
    log.push({ t: time, idx: i, pid: procs[i].pid, name: procs[i].name, from: procs[i].state, to: s });
    procs[i].state = s;
  };
  const pick = () => {
    let best = 0;
    for (let i = 1; i < ready.length; i++) {
      const x = procs[ready[i]], y = procs[ready[best]];
      let better = false;
      if (algo === 'SJF') better = x.burst < y.burst;
      else if (algo === 'SRTF') better = x.remaining < y.remaining;
      else if (algo === 'PRIORITY') better = x.priority < y.priority;
      if (better) best = i;
    }
    return best;
  };
  while (done < n && t < 5000) {
    for (let i = 0; i < n; i++) if (procs[i].arrival === t) { setState(i, 'READY', t); ready.push(i); }
    if (running !== -1) {
      let preempt = false;
      if (algo === 'RR' && slice >= q && ready.length) preempt = true;
      if ((algo === 'SRTF' || algo === 'PRIORITY') && ready.length) {
        const r = procs[running], c = procs[ready[pick()]];
        preempt = algo === 'SRTF' ? c.remaining < r.remaining : c.priority < r.priority;
      }
      if (algo === 'RR' && slice >= q && !ready.length) slice = 0;
      if (preempt) { setState(running, 'READY', t); ready.push(running); running = -1; }
    }
    if (running === -1 && ready.length) {
      const pos = pick();
      running = ready[pos];
      ready.splice(pos, 1);
      setState(running, 'RUNNING', t);
      if (procs[running].response < 0) procs[running].response = t - procs[running].arrival;
      if (last !== running) cs++;
      slice = 0;
    }
    if (running === -1) gantt.push(-1);
    else {
      gantt.push(running);
      last = running;
      procs[running].remaining--;
      slice++;
      if (procs[running].remaining === 0) {
        procs[running].finish = t + 1;
        setState(running, 'TERMINATED', t + 1);
        running = -1; done++;
      }
    }
    t++;
  }
  let busy = 0, w = 0, tt = 0, rt = 0;
  procs.forEach(p => {
    p.turnaround = p.finish - p.arrival;
    p.waiting = p.turnaround - p.burst;
    w += p.waiting; tt += p.turnaround; rt += p.response; busy += p.burst;
  });
  return { procs, gantt, log, total: t, cs, avgWait: w / n, avgTurn: tt / n, avgResp: rt / n, util: 100 * busy / t };
}

function runPaging(refs, frames, algo) {
  const mem = [], loadedAt = [], lastUsed = [], steps = [];
  let faults = 0;
  const n = refs.length;
  for (let t = 0; t < n; t++) {
    const page = refs[t];
    const at = mem.indexOf(page);
    let fault = at === -1, victim = null, changed = -1;
    if (!fault) lastUsed[at] = t;
    else {
      faults++;
      if (mem.length < frames) { mem.push(page); loadedAt.push(t); lastUsed.push(t); changed = mem.length - 1; }
      else {
        let v = 0;
        if (algo === 'FIFO') { for (let i = 1; i < frames; i++) if (loadedAt[i] < loadedAt[v]) v = i; }
        else if (algo === 'LRU') { for (let i = 1; i < frames; i++) if (lastUsed[i] < lastUsed[v]) v = i; }
        else {
          let far = -1;
          for (let i = 0; i < frames; i++) {
            let next = n + 1;
            for (let k = t + 1; k < n; k++) if (refs[k] === mem[i]) { next = k; break; }
            if (next > far) { far = next; v = i; }
          }
        }
        victim = mem[v]; mem[v] = page; loadedAt[v] = t; lastUsed[v] = t; changed = v;
      }
    }
    steps.push({ page, frames: mem.slice(), fault, victim, changed });
  }
  return { steps, faults, hits: n - faults, rate: n ? 100 * faults / n : 0 };
}

function bankNeed(s) { return s.max.map((row, i) => row.map((x, j) => x - s.alloc[i][j])); }
function bankSafe(s) {
  const need = bankNeed(s), work = s.avail.slice(), fin = s.max.map(() => false), seq = [], trace = [];
  let progress = true;
  while (seq.length < s.max.length && progress) {
    progress = false;
    for (let i = 0; i < s.max.length; i++) {
      if (fin[i]) continue;
      if (need[i].every((x, j) => x <= work[j])) {
        const before = work.slice();
        for (let j = 0; j < work.length; j++) work[j] += s.alloc[i][j];
        trace.push({ pid: i, before, after: work.slice() });
        fin[i] = true; seq.push(i); progress = true;
      }
    }
  }
  return { safe: seq.length === s.max.length, seq, trace };
}
function bankSample() {
  return {
    avail: [3, 3, 2],
    max: [[7,5,3],[3,2,2],[9,0,2],[2,2,2],[4,3,3]],
    alloc: [[0,1,0],[2,0,0],[3,0,2],[2,1,1],[0,0,2]]
  };
}
function bankRequest(s, pid, req) {
  const need = bankNeed(s);
  if (req.some((x, j) => x > need[pid][j])) return { status: 'no', text: 'Denied. The request is more than P' + pid + ' declared as its maximum need.' };
  if (req.some((x, j) => x > s.avail[j])) return { status: 'wait', text: 'Wait. The resources are not available right now, so P' + pid + ' must block.' };
  const trial = { avail: s.avail.map((x, j) => x - req[j]), max: s.max, alloc: s.alloc.map(r => r.slice()) };
  for (let j = 0; j < req.length; j++) trial.alloc[pid][j] += req[j];
  const r = bankSafe(trial);
  if (r.safe) return { status: 'ok', text: 'Granted. The system stays safe. Safe sequence: ' + r.seq.map(i => 'P' + i).join(' → '), next: trial };
  return { status: 'no', text: 'Denied. Granting this leaves the system in an unsafe state, so deadlock would be possible.' };
}

(function () {
  const COLORS = 8;
  const SAMPLE = [
    { pid: 1, name: 'init', arrival: 0, burst: 8, priority: 3 },
    { pid: 2, name: 'editor', arrival: 1, burst: 4, priority: 1 },
    { pid: 3, name: 'compiler', arrival: 2, burst: 9, priority: 4 },
    { pid: 4, name: 'browser', arrival: 3, burst: 5, priority: 2 },
    { pid: 5, name: 'daemon', arrival: 6, burst: 2, priority: 5 }
  ];
  const ALGOS = [['FCFS', 'FCFS'], ['SJF', 'SJF'], ['SRTF', 'SRTF'], ['PRIORITY', 'Priority'], ['RR', 'Round Robin']];
  const $ = id => document.getElementById(id);
  const el = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text !== undefined) e.textContent = text; return e; };
  const fmt = x => x.toFixed(2);
  const col = i => 'var(--p' + (i % COLORS) + ')';

  const tabs = ['sched', 'page', 'bank', 'sync', 'disk'];
  tabs.forEach(k => {
    $('tabbtn-' + k).addEventListener('click', () => {
      tabs.forEach(j => {
        $('tab-' + j).hidden = j !== k;
        $('tabbtn-' + j).setAttribute('aria-selected', j === k ? 'true' : 'false');
      });
    });
  });

  let procs = SAMPLE.map(p => Object.assign({}, p));
  let result = null, tNow = 0, timer = null;

  function buildProcTable() {
    const tb = $('proc-table');
    tb.innerHTML = '';
    const head = el('tr');
    ['PID', 'Name', 'Arrival', 'Burst', 'Priority', ''].forEach(h => head.appendChild(el('th', '', h)));
    tb.appendChild(head);
    procs.forEach((p, i) => {
      const tr = el('tr');
      const c0 = el('td'); const sw = el('span', 'swatch'); sw.style.background = col(i); c0.appendChild(sw); c0.appendChild(document.createTextNode('P' + p.pid)); tr.appendChild(c0);
      const mk = (key, type, cls, min) => {
        const td = el('td'); const inp = document.createElement('input');
        inp.type = type; inp.value = p[key]; if (cls) inp.className = cls; if (min !== undefined) inp.min = min;
        inp.setAttribute('aria-label', key + ' of P' + p.pid);
        inp.addEventListener('input', () => {
          if (type === 'number') { let v = parseInt(inp.value, 10); if (isNaN(v)) return; p[key] = Math.max(min, v); }
          else p[key] = inp.value || ('proc' + p.pid);
          recompute(true);
        });
        td.appendChild(inp); tr.appendChild(td);
      };
      mk('name', 'text', 'name'); mk('arrival', 'number', '', 0); mk('burst', 'number', '', 1); mk('priority', 'number', '', 0);
      const tdx = el('td'); const rm = el('button', 'btn', 'Remove'); rm.type = 'button';
      rm.disabled = procs.length <= 1;
      rm.addEventListener('click', () => { procs.splice(i, 1); procs.forEach((q, k) => q.pid = k + 1); buildProcTable(); recompute(true); });
      tdx.appendChild(rm); tr.appendChild(tdx);
      tb.appendChild(tr);
    });
  }

  function currentQuantum() { const q = parseInt($('quantum').value, 10); return isNaN(q) || q < 1 ? 1 : q; }

  function recompute(resetTime) {
    stopPlay();
    result = runScheduler(procs, $('algo').value, currentQuantum());
    $('q-wrap').hidden = $('algo').value !== 'RR';
    const sc = $('scrub'); sc.max = result.total;
    if (resetTime) tNow = result.total;
    tNow = Math.min(tNow, result.total);
    sc.value = tNow;
    renderSched();
    renderCompare();
  }

  function renderSched() {
    const r = result, UNIT = 30;
    const box = $('gantt-box'); box.innerHTML = '';
    const width = r.total * UNIT;
    const g = el('div', 'gantt'); g.style.width = width + 'px';
    let s = 0;
    for (let i = 1; i <= r.gantt.length; i++) {
      if (i === r.gantt.length || r.gantt[i] !== r.gantt[s]) {
        const from = s, to = Math.min(i, tNow);
        if (to > from) {
          const idx = r.gantt[s];
          const seg = el('div', idx < 0 ? 'seg idle' : 'seg', idx < 0 ? (to - from > 1 ? 'idle' : '') : 'P' + r.procs[idx].pid);
          seg.style.left = from * UNIT + 'px'; seg.style.width = (to - from) * UNIT + 'px';
          if (idx >= 0) seg.style.background = col(idx);
          g.appendChild(seg);
        }
        s = i;
      }
    }
    box.appendChild(g);
    const ruler = el('div', 'ruler'); ruler.style.width = width + 'px';
    for (let k = 0; k <= r.total; k++) { const sp = el('span', '', String(k)); sp.style.left = k * UNIT + 'px'; if (k === tNow) sp.style.color = 'var(--accent)'; ruler.appendChild(sp); }
    box.appendChild(ruler);
    box.style.minWidth = width + 'px';
    $('clock').textContent = 't = ' + tNow + ' / ' + r.total;

    const done = tNow >= r.total;
    const stats = $('sched-stats'); stats.innerHTML = '';
    [['Avg waiting', fmt(r.avgWait)], ['Avg turnaround', fmt(r.avgTurn)], ['Avg response', fmt(r.avgResp)],
     ['CPU utilization', fmt(r.util) + '%'], ['Context switches', String(r.cs)]].forEach(([k, v]) => {
      const d = el('div', 'stat'); d.appendChild(el('b', '', v)); d.appendChild(el('span', '', k)); stats.appendChild(d);
    });

    const rt = $('result-table'); rt.innerHTML = '';
    const h = el('tr'); ['Process', 'AT', 'BT', 'Pri', 'CT', 'TAT', 'WT', 'RT'].forEach((x, i) => h.appendChild(el('th', i ? 'num' : '', x))); rt.appendChild(h);
    r.procs.forEach((p, i) => {
      const tr = el('tr');
      const c0 = el('td'); const sw = el('span', 'swatch'); sw.style.background = col(i); c0.appendChild(sw); c0.appendChild(document.createTextNode('P' + p.pid + ' ' + p.name)); tr.appendChild(c0);
      [p.arrival, p.burst, p.priority, p.finish, p.turnaround, p.waiting, p.response].forEach(v => tr.appendChild(el('td', 'num', String(v))));
      rt.appendChild(tr);
    });

    const lg = $('klog'); lg.innerHTML = '';
    const shown = r.log.filter(e => e.t <= tNow);
    shown.forEach((e, i) => {
      const d = el('div', i === shown.length - 1 && !done ? 'last' : '');
      d.appendChild(el('span', 't', '[t=' + e.t + ']'));
      d.appendChild(document.createTextNode('P' + e.pid + ' (' + e.name + '): '));
      d.appendChild(el('span', 'st-' + e.from, e.from));
      d.appendChild(document.createTextNode(' → '));
      d.appendChild(el('span', 'st-' + e.to, e.to));
      lg.appendChild(d);
    });
    lg.scrollTop = lg.scrollHeight;
  }

  function renderCompare() {
    const tb = $('compare-table'); tb.innerHTML = '';
    const h = el('tr'); ['Algorithm', 'Avg WT', 'Avg TAT', 'Avg RT', 'Ctx switches'].forEach((x, i) => h.appendChild(el('th', i ? 'num' : '', x))); tb.appendChild(h);
    const rows = ALGOS.map(([k, name]) => ({ name, r: runScheduler(procs, k, currentQuantum()) }));
    const best = Math.min.apply(null, rows.map(x => x.r.avgWait));
    rows.forEach(x => {
      const tr = el('tr', x.r.avgWait === best ? 'best' : '');
      tr.appendChild(el('td', '', x.name));
      [fmt(x.r.avgWait), fmt(x.r.avgTurn), fmt(x.r.avgResp), String(x.r.cs)].forEach(v => tr.appendChild(el('td', 'num', v)));
      tb.appendChild(tr);
    });
  }

  function stopPlay() { if (timer) { clearInterval(timer); timer = null; } $('play').textContent = 'Play'; }
  $('play').addEventListener('click', () => {
    if (timer) { stopPlay(); return; }
    if (tNow >= result.total) tNow = 0;
    $('play').textContent = 'Pause';
    timer = setInterval(() => {
      tNow++;
      $('scrub').value = tNow;
      renderSched();
      if (tNow >= result.total) stopPlay();
    }, 450);
    $('scrub').value = tNow; renderSched();
  });
  $('scrub').addEventListener('input', () => { stopPlay(); tNow = parseInt($('scrub').value, 10); renderSched(); });
  $('algo').addEventListener('change', () => recompute(true));
  $('quantum').addEventListener('input', () => recompute(true));
  $('add-proc').addEventListener('click', () => {
    if (procs.length >= COLORS) return;
    const n = procs.length + 1;
    procs.push({ pid: n, name: 'proc' + n, arrival: 0, burst: 3, priority: n });
    buildProcTable(); recompute(true);
  });
  $('reset-proc').addEventListener('click', () => { procs = SAMPLE.map(p => Object.assign({}, p)); buildProcTable(); recompute(true); });

  function parseRefs() {
    return $('refs').value.split(/[\s,]+/).filter(Boolean).map(Number).filter(x => Number.isInteger(x) && x >= 0).slice(0, 40);
  }
  function renderPaging() {
    const refs = parseRefs();
    const frames = Math.max(1, Math.min(8, parseInt($('frames').value, 10) || 1));
    const algo = $('palgo').value;
    const tb = $('page-table'); tb.innerHTML = '';
    if (!refs.length) { $('page-stats').innerHTML = ''; $('page-bars').innerHTML = ''; return; }
    const r = runPaging(refs, frames, algo);
    const head = el('tr'); head.appendChild(el('th', 'rowhead', 'Reference'));
    r.steps.forEach(s => head.appendChild(el('th', '', String(s.page)))); tb.appendChild(head);
    for (let f = 0; f < frames; f++) {
      const tr = el('tr'); tr.appendChild(el('td', 'rowhead', 'Frame ' + (f + 1)));
      r.steps.forEach(s => tr.appendChild(el('td', 'cell' + (s.changed === f ? ' changed' : ''), f < s.frames.length ? String(s.frames[f]) : '·')));
      tb.appendChild(tr);
    }
    const res = el('tr'); res.appendChild(el('td', 'rowhead', 'Result'));
    r.steps.forEach(s => res.appendChild(el('td', s.fault ? 'fault' : 'hit', s.fault ? 'F' : 'H'))); tb.appendChild(res);
    const ev = el('tr'); ev.appendChild(el('td', 'rowhead', 'Evicted'));
    r.steps.forEach(s => ev.appendChild(el('td', '', s.victim === null ? '' : String(s.victim)))); tb.appendChild(ev);

    const st = $('page-stats'); st.innerHTML = '';
    [['Page faults', String(r.faults)], ['Hits', String(r.hits)], ['Fault rate', r.rate.toFixed(1) + '%']].forEach(([k, v]) => {
      const d = el('div', 'stat'); d.appendChild(el('b', '', v)); d.appendChild(el('span', '', k)); st.appendChild(d);
    });

    const bars = $('page-bars'); bars.innerHTML = '';
    const all = [['FIFO', 'FIFO'], ['LRU', 'LRU'], ['OPT', 'Optimal']].map(([k, n]) => ({ n, r: runPaging(refs, frames, k) }));
    all.forEach(x => {
      const row = el('div', 'barrow'); row.appendChild(el('span', '', x.n));
      const track = el('div', 'bartrack'); const fill = el('div', 'barfill'); fill.style.width = (100 * x.r.faults / refs.length) + '%'; track.appendChild(fill); row.appendChild(track);
      row.appendChild(el('span', '', x.r.faults + ' of ' + refs.length)); bars.appendChild(row);
    });
  }
  ['refs', 'frames', 'palgo'].forEach(id => $(id).addEventListener('input', renderPaging));

  let bank = bankSample();
  const RES = ['A', 'B', 'C'];
  function renderBank() {
    const need = bankNeed(bank);
    const tb = $('bank-table'); tb.innerHTML = '';
    const h1 = el('tr'); h1.appendChild(el('th', '', 'Process'));
    ['Allocation', 'Max', 'Need'].forEach(g => { const th = el('th', '', g + ' (A B C)'); h1.appendChild(th); }); tb.appendChild(h1);
    bank.max.forEach((_, i) => {
      const tr = el('tr'); tr.appendChild(el('td', '', 'P' + i));
      [bank.alloc[i], bank.max[i], need[i]].forEach(v => tr.appendChild(el('td', '', v.join('  '))));
      tb.appendChild(tr);
    });
    $('bank-avail').textContent = 'Available: ' + bank.avail.join('  ') + '   (A B C)';
    const r = bankSafe(bank);
    const seq = $('bank-seq'); seq.innerHTML = '';
    const line = el('div', 'seq');
    line.appendChild(el('span', 'pill ' + (r.safe ? 'good' : 'bad'), r.safe ? 'SAFE' : 'UNSAFE'));
    if (r.safe) r.seq.forEach((p, k) => { if (k) line.appendChild(el('i', '', '→')); line.appendChild(el('span', '', 'P' + p)); });
    seq.appendChild(line);
    const tr = $('bank-trace'); tr.innerHTML = '';
    if (!r.trace.length) tr.appendChild(el('div', '', 'No process can finish with the available resources.'));
    r.trace.forEach(s => {
      const d = el('div'); d.textContent = 'P' + s.pid + ' can finish (need ≤ work).  Work: ' + s.before.join(' ') + '  →  ' + s.after.join(' ');
      tr.appendChild(d);
    });
  }
  const sel = $('req-pid');
  bank.max.forEach((_, i) => { const o = document.createElement('option'); o.value = i; o.textContent = 'P' + i; sel.appendChild(o); });
  sel.value = '1';
  $('req-go').addEventListener('click', () => {
    const pid = parseInt(sel.value, 10);
    const req = ['req-a', 'req-b', 'req-c'].map(id => Math.max(0, parseInt($(id).value, 10) || 0));
    const out = bankRequest(bank, pid, req);
    if (out.next) bank = out.next;
    const m = $('bank-msg'); m.className = 'msg ' + out.status;
    m.textContent = 'P' + pid + ' requests ' + req.join(' ') + ' → ' + out.text;
    renderBank();
  });
  $('req-reset').addEventListener('click', () => { bank = bankSample(); $('bank-msg').className = ''; $('bank-msg').textContent = ''; renderBank(); });

  buildProcTable();
  recompute(true);
  renderPaging();
  renderBank();
})();
</script>

</body>
</html>
````
