#ifndef MINI_KERNEL_PROCESS_MANAGER_H
#define MINI_KERNEL_PROCESS_MANAGER_H

#include "pcb.h"
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace processsim {

// Stores PCBs and exposes deterministic process creation, I/O blocking, wake-up, and exit.
class ProcessTable {
    std::vector<PCB> processes_;
    int nextPid_ = 1;

    PCB& find(int pid) {
        for (PCB& process : processes_) if (process.pid == pid) return process;
        throw std::out_of_range("Unknown process ID");
    }

public:
    int create(const std::string& name, int burst, int priority, int time,
               std::ostream& out = std::cout) {
        if (name.empty() || burst <= 0 || priority < 0 || time < 0)
            throw std::invalid_argument("Process name, burst, priority, or time is invalid");
        PCB process{nextPid_++, name, time, burst, burst, priority};
        processes_.push_back(process);
        out << "[t=" << time << "] kernel: created P" << process.pid << " ("
            << process.name << ") with priority " << process.priority << '\n';
        setState(processes_.back(), State::READY, time, true, out);
        return process.pid;
    }

    void dispatch(int pid, int time, std::ostream& out = std::cout) {
        PCB& process = find(pid);
        if (process.state != State::READY)
            throw std::logic_error("Only a ready process can be dispatched");
        setState(process, State::RUNNING, time, true, out);
    }

    void blockForIO(int pid, int time, const std::string& device,
                    std::ostream& out = std::cout) {
        PCB& process = find(pid);
        if (process.state != State::RUNNING)
            throw std::logic_error("Only a running process can issue an I/O request");
        out << "[t=" << time << "] P" << pid << " requests " << device << " I/O\n";
        setState(process, State::WAITING, time, true, out);
    }

    void completeIO(int pid, int time, std::ostream& out = std::cout) {
        PCB& process = find(pid);
        if (process.state != State::WAITING)
            throw std::logic_error("Only a waiting process can complete I/O");
        out << "[t=" << time << "] " << "I/O complete for P" << pid << '\n';
        setState(process, State::READY, time, true, out);
    }

    void terminate(int pid, int time, std::ostream& out = std::cout) {
        PCB& process = find(pid);
        if (process.state == State::TERMINATED)
            throw std::logic_error("Process is already terminated");
        setState(process, State::TERMINATED, time, true, out);
    }

    const std::vector<PCB>& all() const { return processes_; }

    void print(std::ostream& out = std::cout) const {
        out << "\nProcess table\nPID  Name          State       Priority  Burst\n";
        for (const PCB& process : processes_)
            out << std::left << std::setw(5) << process.pid << std::setw(14) << process.name
                << std::setw(12) << stateName(process.state) << std::setw(10)
                << process.priority << process.burst << '\n';
    }
};

// Prints a simple, deterministic system-call and interrupt example for viva discussion.
inline void systemCallDemo(std::ostream& out = std::cout) {
    out << "\nSystem-call boundary demo\n"
        << "User mode: calculator requests writeFile(\"/apps/result.txt\")\n"
        << "Trap/interrupt: CPU switches to kernel mode\n"
        << "Kernel validates the path and invokes the file-system service\n"
        << "Return-from-trap: result is returned; execution resumes in user mode\n";
}

} // namespace processsim

#endif
