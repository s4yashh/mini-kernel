#ifndef MINI_KERNEL_APPS_H
#define MINI_KERNEL_APPS_H

#include "banker.h"
#include "fs.h"
#include "ioqueue.h"
#include "memory.h"
#include "pcb.h"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Integrated application layer.
//
// Applications do not schedule themselves. Each application is a small program
// made of CPU steps and file-operation steps. The Simulator below is the kernel:
// it admits processes (memory + Banker resources), dispatches them with
// preemptive priority scheduling, blocks them on the file-system I/O queue, and
// wakes them when the simulated device finishes a request.
namespace apps {

enum class Kind {
    CALCULATOR, TEXT_EDITOR, COUNTER, FILE_SEARCH,
    FILE_ANALYZER, FILE_COPY, INDEXER, SYSTEM_MONITOR
};

inline const char* kindName(Kind kind) {
    switch (kind) {
        case Kind::CALCULATOR: return "calculator";
        case Kind::TEXT_EDITOR: return "text-editor";
        case Kind::COUNTER: return "counter";
        case Kind::FILE_SEARCH: return "file-search";
        case Kind::FILE_ANALYZER: return "file-analyzer";
        case Kind::FILE_COPY: return "file-copy";
        case Kind::INDEXER: return "background-indexer";
        case Kind::SYSTEM_MONITOR: return "system-monitor";
    }
    return "application";
}

inline const char* kindDescription(Kind kind) {
    switch (kind) {
        case Kind::CALCULATOR: return "evaluates 14 + 28 and saves the result";
        case Kind::TEXT_EDITOR: return "reads a note, edits it and writes the new version";
        case Kind::COUNTER: return "CPU-intensive counting workload, then saves its total";
        case Kind::FILE_SEARCH: return "searches system.log for a word and reports the matches";
        case Kind::FILE_ANALYZER: return "counts lines, words and characters in a CSV file";
        case Kind::FILE_COPY: return "copies report.txt to a new file through the file system";
        case Kind::INDEXER: return "reads three source files and builds a small index";
        case Kind::SYSTEM_MONITOR: return "samples simulated process and file-system state";
    }
    return "";
}

struct Runtime;

struct Step {
    enum class Type { CPU, IO };
    Type type = Type::CPU;
    int ticks = 1;
    io::Operation operation = io::Operation::READ;
    std::string path, destination, argument, data;
    int ioPriority = 3;
    std::function<void(Runtime&, const filesystem::FileSystem&)> compute;
};

struct Spec {
    int pid = 0;
    std::string name;
    Kind kind = Kind::CALCULATOR;
    int priority = 3;                    // 1 highest .. 3 lowest
    int arrival = 0;
    int memory = 4;                      // RAM units
    std::vector<int> resources;          // Banker request (A, B, C)
    std::string argument;                // e.g. the word file-search looks for
    std::string inputPath, outputPath;
    filesystem::Allocation allocation = filesystem::Allocation::CONTIGUOUS;
};

struct Program {
    Spec spec;
    std::vector<Step> steps;

    int cpuBurst() const {
        int total = 0;
        for (const Step& step : steps)
            total += step.type == Step::Type::CPU ? step.ticks : 1;  // a system call costs one tick
        return total;
    }
};

struct Runtime {
    Program program;
    PCB process;
    std::size_t step = 0;
    int stepRemaining = 0;
    bool admitted = false;
    std::string buffer;                  // bytes read from the simulated file system
    std::string pending;                 // bytes the next WRITE will store
    std::string result;                  // observable application result
    int ioWait = 0, ioStart = -1, ioRequest = -1;
    int cpuTicks = 0, readyTicks = 0, dispatches = 0;
    std::vector<int> lastBlocks;
    bool finished = false;
};

struct Transition {
    int time = 0;
    int pid = 0;
    State from = State::NEW;
    State to = State::NEW;
    std::string reason;
};

inline Step cpuStep(int ticks,
                    std::function<void(Runtime&, const filesystem::FileSystem&)> compute = {}) {
    Step step;
    step.type = Step::Type::CPU;
    step.ticks = std::max(1, ticks);
    step.compute = compute;
    return step;
}

inline Step ioStep(io::Operation operation, const std::string& path, int priority,
                   const std::string& destination = std::string(),
                   const std::string& data = std::string()) {
    Step step;
    step.type = Step::Type::IO;
    step.operation = operation;
    step.path = path;
    step.destination = destination;
    step.data = data;
    step.ioPriority = priority;
    return step;
}

inline std::string number(long long value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

inline int countOccurrences(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return 0;
    int count = 0;
    for (std::size_t position = haystack.find(needle); position != std::string::npos;
         position = haystack.find(needle, position + needle.size()))
        ++count;
    return count;
}

// Builds one of the eight sample applications. -1 leaves a field at its default.
inline Program makeProgram(Kind kind, int pid, int priority = -1, int arrival = -1,
                           int memory = -1) {
    Program program;
    Spec& spec = program.spec;
    spec.pid = pid;
    spec.name = kindName(kind);
    spec.kind = kind;
    spec.priority = 3;
    spec.arrival = 0;
    spec.memory = 6;
    spec.resources = {0, 0, 0};

    switch (kind) {
        case Kind::CALCULATOR:
            spec.priority = 1; spec.memory = 6; spec.resources = {1, 0, 0};
            spec.inputPath = ""; spec.outputPath = "/apps/calculator.txt";
            spec.allocation = filesystem::Allocation::CONTIGUOUS;
            program.steps = {
                cpuStep(1, [](Runtime& runtime, const filesystem::FileSystem&) {
                    runtime.pending = "14 + 28 = 42";
                    runtime.result = "computed 14 + 28 = 42";
                }),
                ioStep(io::Operation::WRITE, "/apps/calculator.txt", 2),
                cpuStep(1)
            };
            break;
        case Kind::TEXT_EDITOR:
            spec.priority = 2; spec.memory = 10; spec.resources = {1, 0, 0};
            spec.inputPath = "/docs/notes.txt"; spec.outputPath = "/apps/notes.txt";
            spec.allocation = filesystem::Allocation::LINKED;
            program.steps = {
                ioStep(io::Operation::READ, "/docs/notes.txt", 1),
                cpuStep(2, [](Runtime& runtime, const filesystem::FileSystem&) {
                    const std::string addition = "[edited by text-editor]\n";
                    runtime.pending = runtime.buffer + addition;
                    runtime.result = "read " + number(static_cast<long long>(runtime.buffer.size())) +
                                      " bytes and appended one line";
                }),
                ioStep(io::Operation::WRITE, "/apps/notes.txt", 1),
                cpuStep(1)
            };
            break;
        case Kind::COUNTER:
            spec.priority = 3; spec.memory = 8; spec.resources = {0, 1, 0};
            spec.outputPath = "/apps/counter.txt";
            spec.allocation = filesystem::Allocation::INDEXED;
            program.steps = {
                cpuStep(5, [](Runtime& runtime, const filesystem::FileSystem&) {
                    long long total = 0;
                    for (int value = 1; value <= 1000; ++value) total += value;
                    runtime.pending = "1 + 2 + ... + 1000 = " + number(total);
                    runtime.result = "counted 1..1000, total " + number(total);
                }),
                ioStep(io::Operation::WRITE, "/apps/counter.txt", 3),
                cpuStep(1)
            };
            break;
        case Kind::FILE_SEARCH:
            spec.priority = 2; spec.memory = 7; spec.resources = {1, 0, 0};
            spec.argument = "ERROR";
            spec.inputPath = "/docs/system.log"; spec.outputPath = "/apps/search.txt";
            spec.allocation = filesystem::Allocation::INDEXED;
            program.steps = {
                ioStep(io::Operation::READ, "/docs/system.log", 2),
                cpuStep(2, [](Runtime& runtime, const filesystem::FileSystem&) {
                    const int matches = countOccurrences(runtime.buffer, "ERROR");
                    runtime.pending = "searched /docs/system.log for ERROR: " +
                                      number(matches) + " match(es)";
                    runtime.result = "found " + number(matches) + " occurrence(s) of ERROR";
                }),
                ioStep(io::Operation::WRITE, "/apps/search.txt", 2),
                cpuStep(1)
            };
            break;
        case Kind::FILE_ANALYZER:
            spec.priority = 3; spec.memory = 6; spec.resources = {0, 0, 0};
            spec.inputPath = "/docs/numbers.csv"; spec.outputPath = "/apps/analysis.txt";
            spec.allocation = filesystem::Allocation::LINKED;
            program.steps = {
                ioStep(io::Operation::READ, "/docs/numbers.csv", 3),
                cpuStep(3, [](Runtime& runtime, const filesystem::FileSystem&) {
                    int lines = 0, words = 0;
                    bool inWord = false;
                    for (std::size_t i = 0; i < runtime.buffer.size(); ++i) {
                        const char letter = runtime.buffer[i];
                        if (letter == '\n') ++lines;
                        const bool space = letter == ' ' || letter == '\n' || letter == '\t';
                        if (!space && !inWord) { ++words; inWord = true; }
                        if (space) inWord = false;
                    }
                    if (!runtime.buffer.empty() && runtime.buffer[runtime.buffer.size() - 1] != '\n')
                        ++lines;
                    runtime.pending = "lines=" + number(lines) + " words=" + number(words) +
                                      " characters=" + number(static_cast<long long>(runtime.buffer.size()));
                    runtime.result = runtime.pending;
                }),
                ioStep(io::Operation::WRITE, "/apps/analysis.txt", 3),
                cpuStep(1)
            };
            break;
        case Kind::FILE_COPY:
            spec.priority = 1; spec.memory = 9; spec.resources = {0, 1, 0};
            spec.inputPath = "/docs/report.txt"; spec.outputPath = "/apps/report-copy.txt";
            spec.allocation = filesystem::Allocation::CONTIGUOUS;
            program.steps = {
                ioStep(io::Operation::COPY, "/docs/report.txt", 1, "/apps/report-copy.txt"),
                cpuStep(1, [](Runtime& runtime, const filesystem::FileSystem&) {
                    runtime.result = "copied /docs/report.txt to /apps/report-copy.txt";
                })
            };
            break;
        case Kind::INDEXER:
            spec.priority = 3; spec.memory = 8; spec.resources = {0, 0, 1};
            spec.inputPath = "/docs/source.c"; spec.outputPath = "/apps/index.txt";
            spec.allocation = filesystem::Allocation::INDEXED;
            program.steps = {
                ioStep(io::Operation::READ, "/docs/source.c", 3),
                ioStep(io::Operation::READ, "/docs/config.json", 3),
                ioStep(io::Operation::READ, "/docs/numbers.csv", 3),
                ioStep(io::Operation::READ, "/docs/style.txt", 3),
                ioStep(io::Operation::READ, "/docs/report.txt", 3),
                cpuStep(2, [](Runtime& runtime, const filesystem::FileSystem&) {
                    runtime.pending = "index: config.json, numbers.csv, report.txt, source.c, "
                                      "style.txt (" +
                                      number(static_cast<long long>(runtime.buffer.size())) +
                                      " bytes in last file)";
                    runtime.result = "indexed 5 files into /apps/index.txt";
                }),
                ioStep(io::Operation::WRITE, "/apps/index.txt", 3),
                cpuStep(1)
            };
            break;
        case Kind::SYSTEM_MONITOR:
            spec.priority = 2; spec.memory = 6; spec.resources = {0, 0, 0};
            spec.outputPath = "/apps/monitor.txt";
            spec.allocation = filesystem::Allocation::LINKED;
            program.steps = {
                cpuStep(2, [](Runtime& runtime, const filesystem::FileSystem& files) {
                    std::ostringstream report;
                    report << "simulated kernel snapshot: files=" << files.fileCount()
                           << " directories=" << files.directoryCount()
                           << " used-blocks=" << files.usedBlocks() << '/' << files.totalBlocks();
                    runtime.pending = report.str();
                    runtime.result = report.str();
                }),
                ioStep(io::Operation::WRITE, "/apps/monitor.txt", 2),
                cpuStep(1)
            };
            break;
    }

    if (priority >= 0) spec.priority = priority;
    if (arrival >= 0) spec.arrival = arrival;
    if (memory > 0) spec.memory = memory;
    return program;
}

inline std::vector<Program> catalog() {
    std::vector<Program> programs;
    programs.push_back(makeProgram(Kind::CALCULATOR, 1, 1, 0));
    programs.push_back(makeProgram(Kind::TEXT_EDITOR, 2, 2, 0));
    programs.push_back(makeProgram(Kind::COUNTER, 3, 3, 0));
    programs.push_back(makeProgram(Kind::FILE_SEARCH, 4, 2, 2));
    programs.push_back(makeProgram(Kind::FILE_ANALYZER, 5, 3, 1));
    programs.push_back(makeProgram(Kind::FILE_COPY, 6, 1, 4));
    programs.push_back(makeProgram(Kind::INDEXER, 7, 3, 3));
    programs.push_back(makeProgram(Kind::SYSTEM_MONITOR, 8, 2, 5));
    return programs;
}

// Eight applications but only nine Banker rows (index 0 is unused so application
// PID n maps directly to row n). Every request below is small enough that the
// system stays safe no matter which application asks first.
inline banker::Banker appResources() {
    const std::vector<int> available{8, 6, 6};
    banker::Matrix maximum(9, std::vector<int>{2, 2, 2});
    banker::Matrix allocation(9, std::vector<int>{0, 0, 0});
    return banker::Banker(available, maximum, allocation);
}

// Meaningful, deterministic sample files owned by the interactive user.
inline void seedFileSystem(filesystem::FileSystem& files) {
    if (files.directoryCount() == 1) files.createDirectory("/docs");
    if (!files.exists("/apps")) files.createDirectory("/apps");
    const std::string user = "user";
    if (!files.exists("/docs/notes.txt"))
        files.writeFile("/docs/notes.txt",
            "Kernel notes: the ready queue holds processes that can run.\n"
            "Priority 1 is the highest priority; 3 is the lowest.\n",
            filesystem::Allocation::CONTIGUOUS, user);
    if (!files.exists("/docs/report.txt"))
        files.writeFile("/docs/report.txt",
            "Mini Kernel Simulator report\n"
            "Review 2: applications, CPU priority and file I/O priority.\n",
            filesystem::Allocation::LINKED, user);
    if (!files.exists("/docs/numbers.csv"))
        files.writeFile("/docs/numbers.csv",
            "id,value\n1,10\n2,20\n3,30\n4,40\n5,50\n",
            filesystem::Allocation::CONTIGUOUS, user);
    if (!files.exists("/docs/source.c"))
        files.writeFile("/docs/source.c",
            "#include <stdio.h>\nint main(void) {\n    return 0;\n}\n",
            filesystem::Allocation::INDEXED, user);
    if (!files.exists("/docs/config.json"))
        files.writeFile("/docs/config.json",
            "{\n  \"scheduler\": \"priority\",\n  \"ioPolicy\": \"priority\"\n}\n",
            filesystem::Allocation::LINKED, user);
    if (!files.exists("/docs/system.log"))
        files.writeFile("/docs/system.log",
            "boot: mini kernel 1.0\n"
            "INFO: mounted /apps\n"
            "ERROR: simulated disk retry\n"
            "WARN: memory pressure\n"
            "ERROR: request queue full\n"
            "INFO: recovery complete\n",
            filesystem::Allocation::INDEXED, user);
    if (!files.exists("/docs/style.txt"))
        files.writeFile("/docs/style.txt",
            "Style guide: one statement per line.\n",
            filesystem::Allocation::CONTIGUOUS, user);
}

class Simulator {
public:
    struct Options {
        io::Policy ioPolicy = io::Policy::PRIORITY;
        memory::AllocationPolicy memoryPolicy = memory::AllocationPolicy::FIRST_FIT;
        int memoryCapacity = 96;
        bool preemptive = true;
        bool verbose = true;
    };

    Simulator(std::vector<Program> programs, Options options, std::ostream& out = std::cout)
        : options_(options), out_(out), files_(128, 16), ram_(options.memoryCapacity),
          resources_(appResources()) {
        if (programs.empty()) throw std::invalid_argument("At least one application is required");
        if (options.memoryCapacity <= 0) throw std::invalid_argument("RAM capacity must be positive");
        for (std::size_t i = 0; i < programs.size(); ++i) {
            for (std::size_t j = 0; j < i; ++j)
                if (programs[j].spec.pid == programs[i].spec.pid)
                    throw std::invalid_argument("Duplicate application PID");
            Runtime runtime;
            runtime.program = programs[i];
            runtime.process.pid = programs[i].spec.pid;
            runtime.process.name = programs[i].spec.name;
            runtime.process.arrival = programs[i].spec.arrival;
            runtime.process.burst = programs[i].cpuBurst();
            runtime.process.remaining = runtime.process.burst;
            runtime.process.priority = programs[i].spec.priority;
            runtime.process.state = State::NEW;
            runtime.process.finish = -1;
            runtime.process.response = -1;
            runtime.process.waiting = 0;
            runtime.process.turnaround = 0;
            runtimes_.push_back(runtime);
        }
        seedFileSystem(files_);
    }

    void run() {
        if (options_.verbose) printLaunchTable();
        const int count = static_cast<int>(runtimes_.size());
        int time = 0, running = -1, ioOrder = 0;
        int device = -1;

        while (finishedCount() < count) {
            if (time > 5000) {
                if (options_.verbose) out_ << "[kernel] safety limit reached\n";
                break;
            }

            // 1. Admission control: arrival + contiguous memory + Banker resources.
            for (std::size_t i = 0; i < runtimes_.size(); ++i) {
                Runtime& runtime = runtimes_[i];
                if (runtime.admitted || runtime.process.state != State::NEW) continue;
                if (runtime.process.arrival > time) continue;
                if (!ram_.allocate(runtime.process.pid, runtime.program.spec.memory,
                                   options_.memoryPolicy)) {
                    if (options_.verbose)
                        out_ << "[t=" << time << "] admission: P" << runtime.process.pid
                             << " waits, not enough contiguous RAM\n";
                    continue;
                }
                const banker::Decision decision = resources_.request(
                    static_cast<std::size_t>(runtime.process.pid),
                    runtime.program.spec.resources, false);
                if (decision != banker::Decision::GRANTED) {
                    ram_.release(runtime.process.pid);
                    if (options_.verbose)
                        out_ << "[t=" << time << "] admission: P" << runtime.process.pid
                             << " waits, Banker request for resources ["
                             << vectorText(runtime.program.spec.resources)
                             << "] was not granted\n";
                    continue;
                }
                runtime.admitted = true;
                runtime.step = 0;
                runtime.stepRemaining = runtime.program.steps[0].ticks;
                if (options_.verbose)
                    out_ << "[t=" << time << "] admission: P" << runtime.process.pid << " ("
                         << runtime.process.name << ") gets " << runtime.program.spec.memory
                         << " RAM units and resources [" << vectorText(runtime.program.spec.resources)
                         << "]\n";
                transition(runtime, State::READY, time, "admitted by the kernel");
            }

            // 2. Dispatch decision for this tick (preemptive priority).
            const int challenger = bestReady();
            if (running >= 0 && challenger >= 0 && options_.preemptive) {
                Runtime& current = runtimes_[static_cast<std::size_t>(running)];
                Runtime& candidate = runtimes_[static_cast<std::size_t>(challenger)];
                if (candidate.process.priority < current.process.priority) {
                    if (options_.verbose)
                        out_ << "[t=" << time << "] scheduler: P" << candidate.process.pid
                             << " (priority " << candidate.process.priority << ") preempts P"
                             << current.process.pid << " (priority " << current.process.priority
                             << ") because a higher-priority process became READY\n";
                    decisions_.push_back("[t=" + number(time) + "] P" +
                                         number(candidate.process.pid) + " preempts P" +
                                         number(current.process.pid) +
                                         ": higher priority became READY");
                    transition(current, State::READY, time, "preempted by higher-priority process");
                    running = -1;
                }
            }
            if (running < 0) {
                const int chosen = bestReady();
                if (chosen >= 0) {
                    running = chosen;
                    Runtime& runtime = runtimes_[static_cast<std::size_t>(chosen)];
                    if (runtime.process.response < 0)
                        runtime.process.response = time - runtime.process.arrival;
                    ++runtime.dispatches;
                    ++dispatches_;
                    const std::string reason = dispatchReason(chosen);
                    if (options_.verbose) {
                        out_ << "[t=" << time << "] scheduler: " << reason << '\n';
                        decisions_.push_back("[t=" + number(time) + "] CPU: " + reason);
                    }
                    transition(runtime, State::RUNNING, time, "dispatched");
                }
            }

            // 3. Execute one CPU tick.
            if (running < 0) {
                gantt_.push_back(-1);
                ++idleTicks_;
                if (options_.verbose) out_ << "[t=" << time << ".." << time + 1 << "] CPU idle\n";
                readyWaiters(running);
            } else {
                Runtime& runtime = runtimes_[static_cast<std::size_t>(running)];
                gantt_.push_back(runtime.process.pid);
                ++runtime.cpuTicks;
                ++busyTicks_;
                --runtime.stepRemaining;
                readyWaiters(running);
                if (options_.verbose)
                    out_ << "[t=" << time << ".." << time + 1 << "] CPU runs P"
                         << runtime.process.pid << " (" << runtime.process.name << ") step "
                         << runtime.step + 1 << '/' << runtime.program.steps.size()
                         << "; remaining in step=" << runtime.stepRemaining << '\n';
                if (runtime.stepRemaining <= 0) {
                    const Step& step = runtime.program.steps[runtime.step];
                    if (step.type == Step::Type::CPU) {
                        if (step.compute) {
                            step.compute(runtime, files_);
                            if (options_.verbose && !runtime.result.empty())
                                out_ << "        result: " << runtime.result << '\n';
                        }
                        ++runtime.step;
                        if (!advance(runtime, time)) running = -1;
                    } else {
                        // The process executes the file system call, then blocks.
                        submit(runtime, step, time);
                        ++runtime.step;
                        running = -1;
                    }
                }
            }

            // 4. Service the simulated file/disk device for this tick.
            if (device < 0) {
                std::string reason;
                device = io::select(requests_, options_.ioPolicy, &reason, time);
                if (device >= 0) {
                    io::Request& request = requests_[static_cast<std::size_t>(device)];
                    request.status = io::Status::SERVING;
                    request.started = time;
                    request.order = ++ioOrder;
                    if (options_.verbose) {
                        out_ << "[t=" << time << "] I/O scheduler: " << reason << '\n';
                        decisions_.push_back("[t=" + number(time) + "] I/O: " + reason);
                    }
                }
            }
            if (device >= 0) {
                io::Request& request = requests_[static_cast<std::size_t>(device)];
                --request.remaining;
                if (options_.verbose)
                    out_ << "[t=" << time << ".." << time + 1 << "] DISK serves request #"
                         << request.id << " for P" << request.pid << " (" << request.application
                         << "): " << io::operationName(request.operation) << ' ' << request.path
                         << " (" << (request.remaining > 0
                                        ? number(request.remaining) + " tick(s) left"
                                        : std::string("completes this tick"))
                         << ")\n";
                if (request.remaining == 0) {
                    request.completed = time + 1;
                    completeRequest(request, time);
                    Runtime& owner = byPid(request.pid);
                    owner.ioRequest = -1;
                    owner.ioWait += time + 1 - owner.ioStart;
                    device = -1;
                    finishIo(owner, request, time);
                }
            } else if (options_.verbose) {
                out_ << "[t=" << time << ".." << time + 1 << "] DISK idle\n";
            }

            // 5. Queue snapshot at the new time boundary.
            if (options_.verbose) printQueues(time + 1);
            ++time;
        }

        elapsed_ = time;
        for (const Runtime& runtime : runtimes_) {
            if (!runtime.finished && options_.verbose)
                out_ << "[kernel] simulation stopped with P" << runtime.process.pid
                     << " still " << stateName(runtime.process.state) << '\n';
        }
        summarise();
        if (options_.verbose) printReport(out_);
    }

    const std::vector<Runtime>& processes() const { return runtimes_; }
    const std::vector<io::Request>& requests() const { return requests_; }
    const std::vector<int>& gantt() const { return gantt_; }
    const std::vector<Transition>& transitions() const { return transitions_; }
    const std::vector<std::string>& decisions() const { return decisions_; }
    const filesystem::FileSystem& files() const { return files_; }
    filesystem::FileSystem& files() { return files_; }
    int elapsed() const { return elapsed_; }
    double averageWaiting() const { return averageWaiting_; }
    double averageTurnaround() const { return averageTurnaround_; }
    double averageResponse() const { return averageResponse_; }
    double cpuUtilization() const {
        return elapsed_ == 0 ? 0.0 : 100.0 * static_cast<double>(busyTicks_) / elapsed_;
    }
    int dispatches() const { return dispatches_; }
    int ioRequests() const { return static_cast<int>(requests_.size()); }

    const Runtime& byPid(int pid) const {
        for (const Runtime& runtime : runtimes_) if (runtime.process.pid == pid) return runtime;
        throw std::out_of_range("Unknown application PID");
    }
    Runtime& byPid(int pid) {
        for (Runtime& runtime : runtimes_) if (runtime.process.pid == pid) return runtime;
        throw std::out_of_range("Unknown application PID");
    }

    void printReport(std::ostream& out = std::cout) const {
        out << "\nCPU Gantt: ";
        for (std::size_t start = 0; start < gantt_.size();) {
            std::size_t end = start + 1;
            while (end < gantt_.size() && gantt_[end] == gantt_[start]) ++end;
            out << '[' << start << '-' << end << ':';
            if (gantt_[start] < 0) out << "IDLE"; else out << 'P' << gantt_[start];
            out << "] ";
            start = end;
        }
        out << "\n\nProcess table (lower priority number runs first)\n"
            << "PID App                Pri AT BT CT TAT WT RT IOWait Disp State\n";
        for (const Runtime& runtime : runtimes_) {
            const PCB& process = runtime.process;
            out << std::left << std::setw(4) << process.pid << std::setw(19) << process.name
                << std::right << std::setw(3) << process.priority
                << std::setw(3) << process.arrival << std::setw(3) << process.burst
                << std::setw(3) << process.finish << std::setw(4) << process.turnaround
                << std::setw(3) << process.waiting << std::setw(3) << process.response
                << std::setw(7) << runtime.ioWait << std::setw(5) << runtime.dispatches
                << ' ' << stateName(process.state) << '\n';
        }
        out << std::fixed << std::setprecision(2)
            << "Averages WT/TAT/RT: " << averageWaiting_ << " / " << averageTurnaround_
            << " / " << averageResponse_ << "\nCPU utilization: " << cpuUtilization()
            << "%; dispatches=" << dispatches_ << "; idle ticks=" << idleTicks_
            << "; elapsed=" << elapsed_ << " ticks\n";

        if (!requests_.empty()) {
            io::printTable(requests_, out, "\nFile/I/O request log");
            out << "Service order under " << io::policyName(options_.ioPolicy) << ": ";
            std::vector<const io::Request*> ordered;
            for (const io::Request& request : requests_) ordered.push_back(&request);
            std::sort(ordered.begin(), ordered.end(),
                      [](const io::Request* a, const io::Request* b) { return a->order < b->order; });
            for (std::size_t i = 0; i < ordered.size(); ++i) {
                if (ordered[i]->order <= 0) continue;
                out << (i && ordered[i - 1]->order > 0 ? " -> " : "") << '#' << ordered[i]->id
                    << '(' << io::operationName(ordered[i]->operation) << ','
                    << ordered[i]->application << ')';
            }
            out << '\n';
        }

        out << "\nState transition history\n"
            << "Time PID From       To         Reason\n";
        for (const Transition& change : transitions_)
            out << std::right << std::setw(4) << change.time << std::setw(4) << change.pid << "  "
                << std::left << std::setw(11) << stateName(change.from) << std::setw(11)
                << stateName(change.to) << change.reason << '\n';

        out << "\nApplication results\n";
        for (const Runtime& runtime : runtimes_) {
            out << "P" << runtime.process.pid << ' ' << std::left << std::setw(19)
                << runtime.process.name << " -> "
                << (runtime.result.empty() ? "(no result)" : runtime.result);
            if (!runtime.lastBlocks.empty()) {
                out << " [blocks ";
                for (std::size_t i = 0; i < runtime.lastBlocks.size(); ++i)
                    out << (i ? "," : "") << runtime.lastBlocks[i];
                out << ']';
            }
            out << '\n';
        }

        out << "\nFiles produced by the applications\n";
        for (const Runtime& runtime : runtimes_) {
            const std::string& path = runtime.program.spec.outputPath;
            if (path.empty() || !files_.exists(path)) continue;
            const filesystem::FileEntry& file = files_.entry(path);
            out << "  " << path << " (" << file.contents.size() << " bytes, "
                << file.blocks.size() << " blocks, " << filesystem::allocationName(file.allocation)
                << ")\n";
        }
        out << '\n';
        files_.printBlockMap(out);
    }

    std::string serviceOrder() const {
        std::vector<const io::Request*> ordered;
        for (const io::Request& request : requests_) ordered.push_back(&request);
        std::sort(ordered.begin(), ordered.end(),
                  [](const io::Request* a, const io::Request* b) {
                      if (a->order <= 0) return false;
                      if (b->order <= 0) return true;
                      return a->order < b->order;
                  });
        std::ostringstream text;
        for (std::size_t i = 0; i < ordered.size(); ++i) {
            if (ordered[i]->order <= 0) continue;
            text << (text.tellp() > 0 ? " -> " : "") << '#' << ordered[i]->id << '('
                 << ordered[i]->application << ',' << io::operationName(ordered[i]->operation)
                 << ')';
        }
        return text.str();
    }

private:
    Options options_;
    std::ostream& out_;
    filesystem::FileSystem files_;
    memory::ContiguousAllocator ram_;
    banker::Banker resources_;
    std::vector<Runtime> runtimes_;
    std::vector<io::Request> requests_;
    std::vector<int> gantt_;
    std::vector<Transition> transitions_;
    std::vector<std::string> decisions_;
    int elapsed_ = 0, busyTicks_ = 0, idleTicks_ = 0, dispatches_ = 0;
    int nextIoId_ = 1;
    double averageWaiting_ = 0, averageTurnaround_ = 0, averageResponse_ = 0;

    static std::string vectorText(const std::vector<int>& values) {
        std::ostringstream text;
        for (std::size_t i = 0; i < values.size(); ++i) text << (i ? "," : "") << values[i];
        return text.str();
    }

    void transition(Runtime& runtime, State next, int time, const std::string& reason) {
        if (runtime.process.state == next) return;
        if (!canTransition(runtime.process.state, next))
            throw std::logic_error("Illegal process transition for P" +
                                   number(runtime.process.pid) + ": " +
                                   stateName(runtime.process.state) + " -> " + stateName(next));
        Transition change;
        change.time = time;
        change.pid = runtime.process.pid;
        change.from = runtime.process.state;
        change.to = next;
        change.reason = reason;
        transitions_.push_back(change);
        if (options_.verbose)
            out_ << "[t=" << time << "] P" << runtime.process.pid << " ("
                 << runtime.process.name << "): " << stateName(change.from) << " -> "
                 << stateName(next) << " (" << reason << ")\n";
        runtime.process.state = next;
    }

    void printLaunchTable() {
        out_ << "\n=== Integrated mini-kernel run ===\n"
             << "Lower priority number means higher CPU priority. Preemptive priority CPU "
             << "scheduling overlaps with a " << io::policyName(options_.ioPolicy)
             << " file-I/O queue.\n"
             << "PID App                Kind desc / resources      Pri AT Burst RAM Res     Input -> Output\n";
        for (const Runtime& runtime : runtimes_) {
            const Spec& spec = runtime.program.spec;
            out_ << std::left << std::setw(4) << runtime.process.pid
                 << std::setw(19) << runtime.process.name
                 << std::setw(30) << kindDescription(spec.kind)
                 << std::right << std::setw(3) << spec.priority
                 << std::setw(3) << spec.arrival
                 << std::setw(6) << runtime.process.burst
                 << std::setw(4) << spec.memory
                 << "  [" << vectorText(spec.resources) << "] "
                 << (spec.inputPath.empty() ? "-" : spec.inputPath) << " -> "
                 << (spec.outputPath.empty() ? "-" : spec.outputPath) << '\n';
        }
    }

    int bestReady() const {
        int best = -1;
        for (std::size_t i = 0; i < runtimes_.size(); ++i) {
            const Runtime& runtime = runtimes_[i];
            if (runtime.process.state != State::READY) continue;
            if (best < 0) { best = static_cast<int>(i); continue; }
            const Runtime& current = runtimes_[static_cast<std::size_t>(best)];
            if (runtime.process.priority < current.process.priority ||
                (runtime.process.priority == current.process.priority &&
                 runtime.process.arrival < current.process.arrival) ||
                (runtime.process.priority == current.process.priority &&
                 runtime.process.arrival == current.process.arrival &&
                 runtime.process.pid < current.process.pid))
                best = static_cast<int>(i);
        }
        return best;
    }

    std::string dispatchReason(int chosen) const {
        const Runtime& selected = runtimes_[static_cast<std::size_t>(chosen)];
        std::vector<const Runtime*> ready;
        for (const Runtime& runtime : runtimes_)
            if (runtime.process.state == State::READY) ready.push_back(&runtime);
        std::ostringstream text;
        text << 'P' << selected.process.pid << " (" << selected.process.name
             << ", priority " << selected.process.priority << ')';
        int rivals = 0;
        int samePriority = 0;
        std::ostringstream other;
        for (const Runtime* runtime : ready) {
            if (runtime == &selected) continue;
            other << (rivals++ ? ", " : "") << 'P' << runtime->process.pid
                  << " (priority " << runtime->process.priority << ')';
            if (runtime->process.priority == selected.process.priority) ++samePriority;
        }
        if (rivals == 0) {
            text << " selected: it was the only READY process";
        } else {
            text << " selected over " << other.str()
                 << ": lowest priority number in READY";
            if (samePriority > 0)
                text << "; the tie at priority " << selected.process.priority
                     << " is broken by earlier arrival time, then lower PID";
        }
        return text.str();
    }

    void readyWaiters(int running) {
        for (std::size_t i = 0; i < runtimes_.size(); ++i) {
            if (static_cast<int>(i) == running) continue;
            if (runtimes_[i].process.state == State::READY) ++runtimes_[i].readyTicks;
        }
    }

    void printQueues(int time) {
        out_ << "[t=" << time << "] queues: RUNNING=";
        bool wrote = false;
        for (const Runtime& runtime : runtimes_)
            if (runtime.process.state == State::RUNNING) { out_ << 'P' << runtime.process.pid; wrote = true; }
        if (!wrote) out_ << "idle";
        out_ << "  READY=[";
        wrote = false;
        for (const Runtime& runtime : runtimes_)
            if (runtime.process.state == State::READY) {
                out_ << (wrote ? "," : "") << 'P' << runtime.process.pid << "(pri" << runtime.process.priority << ')';
                wrote = true;
            }
        out_ << "]  WAITING=[";
        wrote = false;
        for (const Runtime& runtime : runtimes_)
            if (runtime.process.state == State::WAITING) { out_ << (wrote ? "," : "") << 'P' << runtime.process.pid; wrote = true; }
        out_ << "]  TERMINATED=[";
        wrote = false;
        for (const Runtime& runtime : runtimes_)
            if (runtime.process.state == State::TERMINATED) { out_ << (wrote ? "," : "") << 'P' << runtime.process.pid; wrote = true; }
        out_ << "]\n";
    }

    void submit(Runtime& runtime, const Step& step, int time) {
        io::Request request;
        request.id = nextIoId_++;
        request.pid = runtime.process.pid;
        request.application = runtime.process.name;
        request.operation = step.operation;
        request.path = step.path;
        request.destination = step.destination;
        request.priority = step.ioPriority;
        request.submitted = time + 1;
        std::size_t bytes = step.data.size();
        if (bytes == 0) {
            if (step.operation == io::Operation::WRITE) bytes = runtime.pending.size();
            else if (files_.exists(step.path)) bytes = files_.entry(step.path).contents.size();
        }
        if (step.operation == io::Operation::COPY && files_.exists(step.destination))
            bytes += files_.entry(step.destination).contents.size();
        request.serviceTicks = io::serviceTicksFor(bytes);
        request.remaining = request.serviceTicks;
        request.blocks = static_cast<int>((bytes + 15) / 16);
        if (step.operation == io::Operation::WRITE) request.payload = step.data;
        requests_.push_back(request);
        runtime.ioRequest = request.id;
        runtime.ioStart = time + 1;
        if (options_.verbose)
            out_ << "[t=" << time + 1 << "] P" << runtime.process.pid << " submits I/O request #"
                 << request.id << ": " << io::operationName(step.operation) << ' '
                 << step.path << " (request priority " << step.ioPriority << ")\n";
        transition(runtime, State::WAITING, time + 1,
                   "blocked on file I/O request #" + number(request.id));
    }

    // Moves to the next step. Returns true when the process can keep running.
    // A file system call occupies one CPU tick before the process blocks.
    bool advance(Runtime& runtime, int time) {
        if (runtime.step >= runtime.program.steps.size()) {
            terminate(runtime, time);
            return false;
        }
        const Step& next = runtime.program.steps[runtime.step];
        runtime.stepRemaining = next.type == Step::Type::CPU ? next.ticks : 1;
        return true;
    }

    // Called when a request finishes: the process wakes up and continues with the
    // next step (CPU work or another system call).
    void finishIo(Runtime& runtime, const io::Request& request, int time) {
        if (!advance(runtime, time)) return;
        transition(runtime, State::READY, time + 1,
                   "I/O complete for request #" + number(request.id));
    }

    int finishedCount() const {
        int count = 0;
        for (const Runtime& runtime : runtimes_) if (runtime.finished) ++count;
        return count;
    }

    void completeRequest(io::Request& request, int time) {
        Runtime& runtime = byPid(request.pid);
        request.ok = true;
        try {
            switch (request.operation) {
                case io::Operation::READ: {
                    request.payload = files_.readFile(request.path);
                    runtime.buffer = request.payload;
                    request.blocks = files_.blocksOf(request.path);
                    runtime.lastBlocks = files_.entry(request.path).blocks;
                    request.note = number(static_cast<long long>(request.payload.size())) +
                                   " byte(s) read";
                    break;
                }
                case io::Operation::WRITE: {
                    const std::string data =
                        request.payload.empty() ? runtime.pending : request.payload;
                    if (!files_.writeFile(request.path, data, runtime.program.spec.allocation))
                        throw std::runtime_error("file system refused the write (space or path)");
                    request.blocks = files_.blocksOf(request.path);
                    runtime.lastBlocks = files_.entry(request.path).blocks;
                    request.note = number(static_cast<long long>(data.size())) + " byte(s) written";
                    break;
                }
                case io::Operation::COPY: {
                    if (!files_.copyFile(request.path, request.destination,
                                         runtime.program.spec.allocation))
                        throw std::runtime_error("file system refused the copy (space or path)");
                    request.blocks = files_.blocksOf(request.path) +
                                     files_.blocksOf(request.destination);
                    runtime.lastBlocks = files_.entry(request.destination).blocks;
                    runtime.buffer = files_.readFile(request.destination);
                    request.note = "copied to " + request.destination;
                    break;
                }
                case io::Operation::DELETE: {
                    if (!files_.deleteFile(request.path)) throw std::runtime_error("file not found");
                    request.note = "deleted " + request.path;
                    break;
                }
                case io::Operation::MKDIR: {
                    if (!files_.createDirectory(request.path))
                        throw std::runtime_error("directory could not be created");
                    request.note = "created directory " + request.path;
                    break;
                }
            }
            request.status = io::Status::COMPLETED;
            if (options_.verbose)
                out_ << "[t=" << time + 1 << "] I/O complete #" << request.id << " ("
                     << request.application << "): " << request.note << '\n';
        } catch (const std::exception& error) {
            request.ok = false;
            request.status = io::Status::FAILED;
            request.note = error.what();
            if (options_.verbose)
                out_ << "[t=" << time + 1 << "] I/O FAILED #" << request.id << " ("
                     << request.application << "): " << error.what() << '\n';
            if (runtime.result.empty())
                runtime.result = std::string("I/O failed: ") + error.what();
        }
    }

    void terminate(Runtime& runtime, int time) {
        runtime.process.finish = time + 1;
        runtime.process.turnaround = runtime.process.finish - runtime.process.arrival;
        runtime.process.waiting = runtime.process.turnaround - runtime.process.burst - runtime.ioWait;
        if (runtime.process.waiting < 0) {
            // A process can be admitted after its arrival; that delay is counted as
            // waiting, so a negative value means the bookkeeping must be corrected.
            runtime.process.waiting = std::max(0, runtime.process.turnaround - runtime.process.burst);
        }
        runtime.finished = true;
        transition(runtime, State::TERMINATED, time + 1, "program finished; resources released");
        ram_.release(runtime.process.pid);
        resources_.releaseAllAndFinish(static_cast<std::size_t>(runtime.process.pid), false);
        if (options_.verbose)
            out_ << "[t=" << time + 1 << "] kernel: P" << runtime.process.pid
                 << " releases " << runtime.program.spec.memory
                 << " RAM units and its Banker resources\n";
    }

    void summarise() {
        averageWaiting_ = averageTurnaround_ = averageResponse_ = 0.0;
        int completed = 0;
        for (const Runtime& runtime : runtimes_) {
            if (!runtime.finished) continue;
            ++completed;
            averageWaiting_ += runtime.process.waiting;
            averageTurnaround_ += runtime.process.turnaround;
            averageResponse_ += runtime.process.response;
        }
        if (completed > 0) {
            averageWaiting_ /= completed;
            averageTurnaround_ /= completed;
            averageResponse_ /= completed;
        }
    }
};

// ---------------------------------------------------------------------------
// Guided demonstrations
// ---------------------------------------------------------------------------

inline void integratedDemo(std::ostream& out = std::cout) {
    Simulator::Options options;
    options.ioPolicy = io::Policy::PRIORITY;
    options.memoryCapacity = 96;
    Simulator simulator(catalog(), options, out);
    simulator.run();
    out << "\nIntegrated run complete: process -> Banker resources -> priority CPU -> memory "
        << "-> I/O wait/wake -> simulated file system -> disk.\n";
}

// A low-priority application starts first; higher-priority applications arrive
// later and preempt it. Every preemption is explained by the scheduler.
inline void priorityScenario(std::ostream& out = std::cout) {
    out << "\n=== Scenario 1: CPU priority with late high-priority arrivals ===\n"
        << "The low-priority counter starts at t=0. The editor (priority 2) arrives at t=2 and "
        << "the calculator (priority 1) at t=5. Watch the preemptions.\n";
    std::vector<Program> programs;
    programs.push_back(makeProgram(Kind::COUNTER, 1, 3, 0, 8));
    programs.push_back(makeProgram(Kind::FILE_ANALYZER, 2, 3, 0, 6));
    programs.push_back(makeProgram(Kind::TEXT_EDITOR, 3, 2, 2, 10));
    programs.push_back(makeProgram(Kind::CALCULATOR, 4, 1, 5, 6));
    Simulator::Options options;
    options.ioPolicy = io::Policy::PRIORITY;
    Simulator simulator(programs, options, out);
    simulator.run();
    out << "\nDiscussion: the CPU policy is preemptive priority with a deterministic tie-break "
        << "(priority, then arrival time, then PID). The counter is not starved because every "
        << "application finishes; with an endless stream of priority-1 work it could starve.\n";
}

// Identical request set, two I/O policies; then the same applications end-to-end.
inline void ioPriorityComparison(std::ostream& out = std::cout) {
    io::demo(out);

    out << "\n=== Full application run: FCFS I/O vs priority I/O ===\n"
        << "Both runs use the same applications, the same CPU policy and the same seed files.\n";
    Simulator::Options fcfsOptions;
    fcfsOptions.ioPolicy = io::Policy::FCFS;
    fcfsOptions.verbose = false;
    Simulator fcfs(catalog(), fcfsOptions, out);
    fcfs.run();

    Simulator::Options priorityOptions;
    priorityOptions.ioPolicy = io::Policy::PRIORITY;
    priorityOptions.verbose = false;
    Simulator priority(catalog(), priorityOptions, out);
    priority.run();

    out << "\nMetric                         FCFS I/O        Priority I/O\n"
        << std::left << std::setw(31) << "Average request wait (ticks)"
        << std::setw(16) << std::fixed << std::setprecision(2)
        << io::statistics(fcfs.requests()).averageWait
        << io::statistics(priority.requests()).averageWait << '\n';
    out << std::left << std::setw(31) << "Longest request wait (ticks)"
        << std::setw(16) << io::statistics(fcfs.requests()).maxWait
        << io::statistics(priority.requests()).maxWait << '\n';
    out << std::left << std::setw(31) << "Average turnaround (CPU)"
        << std::setw(16) << fcfs.averageTurnaround() << priority.averageTurnaround() << '\n';
    out << std::left << std::setw(31) << "Elapsed ticks"
        << std::setw(16) << fcfs.elapsed() << priority.elapsed() << '\n';
    out << "\nFCFS service order:\n    " << fcfs.serviceOrder()
        << "\nPriority service order:\n    " << priority.serviceOrder() << '\n';
    out << "\nPer-request wait comparison\n"
        << "ID  Application          Op     Pri  FCFS wait  Priority wait  Status\n";
    for (const io::Request& request : fcfs.requests()) {
        const io::Request* other = nullptr;
        for (const io::Request& candidate : priority.requests())
            if (candidate.id == request.id) other = &candidate;
        out << std::left << std::setw(4) << request.id << std::setw(21) << request.application
            << std::setw(7) << io::operationName(request.operation)
            << std::right << std::setw(4) << request.priority
            << std::setw(11) << request.wait()
            << std::setw(15) << (other ? other->wait() : -1)
            << "  " << (!other ? "?" : other->ok ? "ok" : "failed") << '\n';
    }
    out << "\nThe high-priority editor and file-copy requests jump ahead of the background "
        << "indexer's reads under the priority policy.\n";
}

// One file-dependent application: READ blocks it, the scheduler runs someone
// else, the completion interrupt wakes it, and its real output is shown.
inline void fileDependentScenario(std::ostream& out = std::cout) {
    out << "\n=== Scenario 3: file-dependent applications (READ -> WAITING -> READY) ===\n";
    std::vector<Program> programs;
    programs.push_back(makeProgram(Kind::FILE_SEARCH, 1, 2, 0, 7));
    programs.push_back(makeProgram(Kind::FILE_ANALYZER, 2, 3, 0, 6));
    programs.push_back(makeProgram(Kind::INDEXER, 3, 1, 1, 8));
    Simulator::Options options;
    options.ioPolicy = io::Policy::PRIORITY;
    Simulator simulator(programs, options, out);
    simulator.run();
    out << "\nSearch output: ";
    const std::string search = simulator.files().readFile("/apps/search.txt");
    out << search << '\n';
    if (simulator.files().exists("/apps/analysis.txt"))
        out << "Analysis output: " << simulator.files().readFile("/apps/analysis.txt") << '\n';
    out << "The analyzer's CPU work resumes only after its READ request is serviced; the "
        << "indexer's background reads are served last because they have priority 3.\n";
}

// Scenario 4: directory, create/modify/copy/delete, and visible block accounting.
inline void filesystemScenario(std::ostream& out = std::cout) {
    out << "\n=== Scenario 4: file system operations and block allocation ===\n";
    filesystem::FileSystem files(64, 16);
    seedFileSystem(files);
    out << "\nSample files created at start-up\n";
    files.list("/docs", out);
    files.printBlockMap(out);

    out << "\n1. Create directory /apps/work: "
        << (files.createDirectories("/apps/work") ? "ok" : "failed") << '\n';
    const bool created = files.writeFile("/apps/work/one.txt", "first version of the note\n",
                                         filesystem::Allocation::CONTIGUOUS);
    out << "2. Create /apps/work/one.txt (contiguous): " << (created ? "ok" : "failed")
        << " -> " << files.blocksOf("/apps/work/one.txt") << " blocks\n";
    const bool appended = files.appendFile("/apps/work/one.txt",
                                           "second line added by the editor\n",
                                           filesystem::Allocation::CONTIGUOUS);
    out << "3. Append a line (file grows and is reallocated): " << (appended ? "ok" : "failed")
        << " -> " << files.blocksOf("/apps/work/one.txt") << " blocks\n";
    const bool copied = files.copyFile("/apps/work/one.txt", "/apps/work/one-copy.txt",
                                       filesystem::Allocation::INDEXED);
    out << "4. Copy it to /apps/work/one-copy.txt (indexed): " << (copied ? "ok" : "failed")
        << " -> " << files.blocksOf("/apps/work/one-copy.txt")
        << " blocks (data + index)\n";
    const bool temporary = files.writeFile("/apps/work/temp.txt", "temporary data",
                                           filesystem::Allocation::LINKED);
    out << "5. Create /apps/work/temp.txt (linked): " << (temporary ? "ok" : "failed") << '\n';
    const bool deleted = files.deleteFile("/apps/work/temp.txt");
    out << "6. Delete /apps/work/temp.txt (its blocks return to the free list): "
        << (deleted ? "ok" : "failed") << '\n';
    out << "7. Rename one-copy.txt to final.txt: "
        << (files.renameFile("/apps/work/one-copy.txt", "/apps/work/final.txt") ? "ok" : "failed")
        << '\n';
    out << "8. Read final.txt: " << files.readFile("/apps/work/final.txt");

    files.metadata("/apps/work/final.txt", out);
    files.list("/apps/work", out);
    files.printBlockMap(out);
    out << "Used blocks: " << files.usedBlocks() << '/' << files.totalBlocks()
        << ", free: " << files.freeBlocks() << ", utilisation: "
        << files.utilisation() << "%\n";

    std::string huge(2000, 'x');
    out << "\n9. Deliberately overfill storage with a 2000-byte contiguous file: "
        << (files.writeFile("/apps/work/huge.txt", huge, filesystem::Allocation::CONTIGUOUS)
                ? "written" : "REJECTED (not enough contiguous blocks)")
        << "\n   Used blocks after the rejected write: " << files.usedBlocks()
        << " (unchanged), free: " << files.freeBlocks() << '\n';
    out << "\nEvery byte lives in the simulated block map; nothing was written to the host disk.\n";
}

// Interactive launch: the user configures each application and then the kernel runs it.
inline void interactiveLaunch(std::istream& in, std::ostream& out) {
    out << "\n=== Launch applications yourself ===\n"
        << "Catalog (choose 1-8, or 9 for a custom application):\n"
        << "1 calculator      2 text-editor     3 counter          4 file-search\n"
        << "5 file-analyzer   6 file-copy       7 background-index 8 system-monitor\n"
        << "9 custom application\n";
    int count = 0;
    while (true) {
        out << "How many applications do you want to launch (1..8)? ";
        std::string line;
        if (!std::getline(in, line)) return;
        std::istringstream parse(line);
        if (parse >> count) {
            int extra = 0;
            if (!(parse >> extra) && count >= 1 && count <= 8) break;
        }
        out << "Enter a whole number from 1 to 8.\n";
    }

    std::vector<Program> programs;
    for (int index = 0; index < count; ++index) {
        out << "\nApplication " << index + 1 << " of " << count << " (1-9): ";
        int kind = 0;
        std::string line;
        if (!std::getline(in, line)) return;
        std::istringstream parse(line);
        if (!(parse >> kind) || kind < 1 || kind > 9) { out << "Invalid choice; using calculator.\n"; kind = 1; }

        Program program;
        if (kind == 9) {
            out << "Name: ";
            std::string name;
            if (!std::getline(in, name) || name.empty()) name = "custom";
            int burst = 3, priority = 2, arrival = 0, memoryUnits = 8;
            out << "CPU burst ticks (1..20) [3]: "; if (!std::getline(in, line)) return;
            { std::istringstream p(line); if (p >> burst) { if (burst < 1) burst = 1; if (burst > 20) burst = 20; } else burst = 3; }
            out << "Priority (1 highest .. 3 lowest) [2]: "; if (!std::getline(in, line)) return;
            { std::istringstream p(line); if (!(p >> priority) || priority < 1 || priority > 3) priority = 2; }
            out << "Arrival time [0]: "; if (!std::getline(in, line)) return;
            { std::istringstream p(line); if (!(p >> arrival) || arrival < 0) arrival = 0; }
            out << "Memory units [8]: "; if (!std::getline(in, line)) return;
            { std::istringstream p(line); if (!(p >> memoryUnits) || memoryUnits < 1) memoryUnits = 8; }
            out << "Input file to read [/docs/system.log]: "; if (!std::getline(in, line)) return;
            const std::string input = line.empty() ? "/docs/system.log" : line;
            out << "Output file to write [/apps/custom.txt]: "; if (!std::getline(in, line)) return;
            const std::string output = line.empty() ? "/apps/custom.txt" : line;
            program.spec.pid = index + 1;
            program.spec.name = name;
            program.spec.kind = Kind::FILE_ANALYZER;
            program.spec.priority = priority;
            program.spec.arrival = arrival;
            program.spec.memory = memoryUnits;
            program.spec.resources = {1, 0, 0};
            program.spec.argument = "custom";
            program.spec.inputPath = input;
            program.spec.outputPath = output;
            program.steps = {
                ioStep(io::Operation::READ, input, 2),
                cpuStep(burst, [](Runtime& runtime, const filesystem::FileSystem&) {
                    std::size_t lines = 0;
                    for (std::size_t i = 0; i < runtime.buffer.size(); ++i)
                        if (runtime.buffer[i] == '\n') ++lines;
                    runtime.pending = "custom application processed " +
                                      number(static_cast<long long>(runtime.buffer.size())) +
                                      " byte(s) in " + number(static_cast<long long>(lines)) + " line(s)";
                    runtime.result = runtime.pending;
                }),
                ioStep(io::Operation::WRITE, output, 2),
                cpuStep(1)
            };
        } else {
            const Kind kinds[8] = {Kind::CALCULATOR, Kind::TEXT_EDITOR, Kind::COUNTER,
                                   Kind::FILE_SEARCH, Kind::FILE_ANALYZER, Kind::FILE_COPY,
                                   Kind::INDEXER, Kind::SYSTEM_MONITOR};
            program = makeProgram(kinds[kind - 1], index + 1);
            out << "Configured " << program.spec.name << " (priority " << program.spec.priority
                << ", arrival " << program.spec.arrival << ", burst " << program.cpuBurst()
                << ", RAM " << program.spec.memory << "). Press Enter to accept, or type "
                << "\"priority arrival memory\": ";
            if (!std::getline(in, line)) return;
            std::istringstream parse2(line);
            int priority = 0, arrival = 0, memoryUnits = 0;
            if (parse2 >> priority >> arrival >> memoryUnits) {
                if (priority >= 1 && priority <= 3) program.spec.priority = priority;
                if (arrival >= 0) program.spec.arrival = arrival;
                if (memoryUnits > 0) program.spec.memory = memoryUnits;
            }
        }
        programs.push_back(program);
    }

    out << "\nFile-I/O policy: 1=FCFS 2=Priority [2]: ";
    std::string line;
    if (!std::getline(in, line)) return;
    Simulator::Options options;
    options.ioPolicy = (line == "1") ? io::Policy::FCFS : io::Policy::PRIORITY;
    out << "RAM units [96]: ";
    if (!std::getline(in, line)) return;
    if (!line.empty()) {
        std::istringstream parse(line);
        int capacity = 0;
        if ((parse >> capacity) && capacity > 0) options.memoryCapacity = capacity;
    }

    Simulator simulator(programs, options, out);
    simulator.run();
    out << "\nInteractive run complete.\n";
}

} // namespace apps

#endif
