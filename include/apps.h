#ifndef MINI_KERNEL_APPS_H
#define MINI_KERNEL_APPS_H

#include "banker.h"
#include "disk.h"
#include "fs.h"
#include "memory.h"
#include "pcb.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace apps {

struct Application {
    PCB process;
    int cpuBeforeIO = 0;
    int memoryUnits = 0;
    banker::Vector resourceRequest;
    std::string outputPath;
    std::function<void(filesystem::FileSystem&, std::ostream&)> run;
    int ioRemaining = 0;
    int ioStart = 0;
    int ioWait = 0;
    int phase = 0;
};

struct DiskRequest { std::size_t appIndex; int block; };

// Runs applications through one discrete-event loop: priority CPU scheduling and
// one-block-per-tick FCFS disk service overlap, with real WAITING/READY transitions.
inline void integratedDemo(std::ostream& out = std::cout) {
    out << "\n=== Integrated mini-kernel run ===\n"
        << "Lower priority number means earlier CPU service. CPU and disk advance together by tick.\n";
    filesystem::FileSystem fs(48, 16);
    if (!fs.createDirectory("/apps")) throw std::runtime_error("Could not initialize /apps");
    memory::ContiguousAllocator ram(64);
    banker::Banker resourceManager = banker::sample();
    std::vector<Application> applications;
    applications.push_back({PCB{1, "calculator", 0, 3, 2, 1}, 2, 6, {1, 0, 2},
        "/apps/calculator.txt",
        [](filesystem::FileSystem& files, std::ostream& log) {
            const int left = 14, right = 28, answer = left + right;
            log << "calculator computes " << left << " + " << right << " = " << answer << '\n';
            if (!files.writeFile("/apps/calculator.txt", "14 + 28 = 42", filesystem::Allocation::CONTIGUOUS))
                throw std::runtime_error("Calculator could not save its result");
        }});
    applications.push_back({PCB{2, "text-editor", 0, 5, 3, 2}, 3, 10, {1, 0, 0},
        "/apps/notes.txt",
        [](filesystem::FileSystem& files, std::ostream& log) {
            const std::string note = "Mini Kernel Simulator: files are stored in simulated blocks.";
            if (!files.writeFile("/apps/notes.txt", note, filesystem::Allocation::LINKED))
                throw std::runtime_error("Editor could not save its document");
            log << "text-editor writes /apps/notes.txt (" << note.size() << " bytes)\n";
        }});
    applications.push_back({PCB{3, "counter", 0, 4, 3, 3}, 2, 8, {0, 1, 0},
        "/apps/counter.txt",
        [](filesystem::FileSystem& files, std::ostream& log) {
            int total = 0;
            for (int value = 1; value <= 5; ++value) total += value;
            if (!files.writeFile("/apps/counter.txt", "1+2+3+4+5 = 15", filesystem::Allocation::INDEXED))
                throw std::runtime_error("Counter could not save its result");
            log << "counter computes 1+2+3+4+5 = " << total << '\n';
        }});

    out << "\nLaunch table (priority 1 is highest)\nPID  Application   Priority CPU-before-I/O RAM  Resources\n";
    for (Application& app : applications) {
        out << std::left << std::setw(5) << app.process.pid << std::setw(14) << app.process.name
            << std::setw(9) << app.process.priority << std::setw(15) << app.cpuBeforeIO
            << std::setw(6) << app.memoryUnits << '[';
        for (std::size_t i = 0; i < app.resourceRequest.size(); ++i)
            out << (i ? "," : "") << app.resourceRequest[i];
        out << "]\n";
        if (!ram.allocate(app.process.pid, app.memoryUnits, memory::AllocationPolicy::FIRST_FIT))
            throw std::runtime_error("Not enough memory to launch " + app.process.name);
        out << "[kernel] P" << app.process.pid << " requests resources ";
        const banker::Decision decision = resourceManager.request(
            static_cast<std::size_t>(app.process.pid), app.resourceRequest, true, out);
        if (decision != banker::Decision::GRANTED)
            throw std::runtime_error("Resource request failed for " + app.process.name);
        app.process.state = State::NEW;
        app.process.remaining = app.cpuBeforeIO;
        app.process.finish = -1;
        app.process.response = -1;
        app.process.waiting = 0;
        app.process.turnaround = 0;
    }
    ram.print(out);

    std::deque<DiskRequest> ioQueue;
    std::vector<int> gantt;
    std::vector<int> diskPath{53};
    int diskMovement = 0, running = -1, completed = 0, time = 0;
    int cpuBusy = 0, dispatches = 0;
    if (out.good()) out << "\nIntegrated event trace\n";

    while (completed < static_cast<int>(applications.size())) {
        if (time > 10000) throw std::runtime_error("Integrated simulation exceeded its safety limit");
        for (Application& app : applications)
            if (app.process.state == State::NEW && app.process.arrival <= time)
                setState(app.process, State::READY, time, true, out);

        // Only requests already queued at the start of this tick can use this disk tick.
        const bool diskWasBusy = !ioQueue.empty();
        int challenger = -1;
        for (std::size_t i = 0; i < applications.size(); ++i) {
            const PCB& candidate = applications[i].process;
            if (candidate.state != State::READY) continue;
            if (challenger < 0 || candidate.priority < applications[static_cast<std::size_t>(challenger)].process.priority ||
                (candidate.priority == applications[static_cast<std::size_t>(challenger)].process.priority &&
                 candidate.pid < applications[static_cast<std::size_t>(challenger)].process.pid))
                challenger = static_cast<int>(i);
        }
        if (running >= 0 && challenger >= 0 &&
            applications[static_cast<std::size_t>(challenger)].process.priority <
            applications[static_cast<std::size_t>(running)].process.priority) {
            setState(applications[static_cast<std::size_t>(running)].process, State::READY, time, true, out);
            running = -1;
        }
        if (running < 0 && challenger >= 0) {
            running = challenger;
            PCB& process = applications[static_cast<std::size_t>(running)].process;
            if (process.response < 0) process.response = time - process.arrival;
            setState(process, State::RUNNING, time, true, out);
            ++dispatches;
        }

        if (running < 0) {
            gantt.push_back(-1);
            out << "[t=" << time << ".." << time + 1 << "] CPU idle\n";
        } else {
            Application& app = applications[static_cast<std::size_t>(running)];
            for (std::size_t i = 0; i < applications.size(); ++i)
                if (static_cast<int>(i) != running && applications[i].process.state == State::READY)
                    ++applications[i].process.waiting;
            gantt.push_back(app.process.pid);
            --app.process.remaining;
            ++cpuBusy;
            out << "[t=" << time << ".." << time + 1 << "] CPU runs P" << app.process.pid
                << " (" << app.process.name << "); phase remaining=" << app.process.remaining << '\n';
            if (app.process.remaining == 0 && app.phase == 0) {
                app.run(fs, out);
                const filesystem::FileEntry& file = fs.entry(app.outputPath);
                app.ioRemaining = static_cast<int>(file.blocks.size());
                app.ioStart = time + 1;
                app.phase = 1;
                setState(app.process, State::WAITING, time + 1, true, out);
                for (int block : file.blocks)
                    ioQueue.push_back({static_cast<std::size_t>(running), block});
                app.process.remaining = 1; // one CPU tick models post-I/O completion work
                running = -1;
            } else if (app.process.remaining == 0 && app.phase == 1) {
                app.process.finish = time + 1;
                app.process.turnaround = app.process.finish - app.process.arrival;
                setState(app.process, State::TERMINATED, time + 1, true, out);
                out << "[kernel] P" << app.process.pid << " releases memory and resources\n";
                ram.release(app.process.pid);
                resourceManager.releaseAllAndFinish(static_cast<std::size_t>(app.process.pid), true, out);
                ++completed;
                running = -1;
            }
        }

        if (diskWasBusy) {
            const DiskRequest request = ioQueue.front();
            ioQueue.pop_front();
            const int previous = diskPath.back();
            diskMovement += std::abs(previous - request.block);
            if (previous != request.block) diskPath.push_back(request.block);
            Application& app = applications[request.appIndex];
            --app.ioRemaining;
            out << "[t=" << time << ".." << time + 1 << "] DISK serves block "
                << request.block << " for P" << app.process.pid << " (head " << previous
                << " -> " << request.block << ")\n";
            if (app.ioRemaining == 0) {
                app.ioWait = time + 1 - app.ioStart;
                setState(app.process, State::READY, time + 1, true, out);
            }
        } else if (out.good()) {
            out << "[t=" << time << ".." << time + 1 << "] DISK idle\n";
        }
        ++time;
    }

    double avgWaiting = 0.0, avgTurnaround = 0.0, avgResponse = 0.0;
    out << "\nCPU Gantt: ";
    for (std::size_t start = 0; start < gantt.size();) {
        std::size_t end = start + 1;
        while (end < gantt.size() && gantt[end] == gantt[start]) ++end;
        out << '[' << start << '-' << end << ':';
        if (gantt[start] < 0) out << "IDLE"; else out << 'P' << gantt[start];
        out << "] ";
        start = end;
    }
    out << "\nDisk FCFS path: ";
    for (std::size_t i = 0; i < diskPath.size(); ++i) out << (i ? " -> " : "") << diskPath[i];
    out << "\nDisk movement: " << diskMovement << " cylinders\n"
        << "\nIntegrated per-process report\nPID  Application   Pri CT TAT WT RT I/O-wait\n";
    for (const Application& app : applications) {
        avgWaiting += app.process.waiting;
        avgTurnaround += app.process.turnaround;
        avgResponse += app.process.response;
        out << std::left << std::setw(5) << app.process.pid << std::setw(14) << app.process.name
            << std::right << std::setw(3) << app.process.priority << std::setw(3) << app.process.finish
            << std::setw(4) << app.process.turnaround << std::setw(3) << app.process.waiting
            << std::setw(3) << app.process.response << std::setw(8) << app.ioWait << '\n';
    }
    const double count = static_cast<double>(applications.size());
    out << std::fixed << std::setprecision(2)
        << "Average WT/TAT/RT: " << avgWaiting / count << " / " << avgTurnaround / count
        << " / " << avgResponse / count << "\nCPU utilization: "
        << (time ? 100.0 * cpuBusy / time : 0.0) << "%; dispatches=" << dispatches << '\n';
    out << "\nFiles written by the prioritized applications\n";
    fs.list("/apps", out);
    fs.printBlockMap(out);
    out << "Integrated run complete: process -> Banker resources -> priority CPU -> memory -> I/O wait/wake -> file system -> disk.\n";
}

} // namespace apps

#endif
