#ifndef MINI_KERNEL_IOQUEUE_H
#define MINI_KERNEL_IOQUEUE_H

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// A simulated file/disk *request* queue.
//
// This module intentionally answers a different question from disk.h:
//   io::select()  -> which pending file request is serviced next (I/O scheduling)
//   disk::schedule() -> how the physical head moves between cylinders (disk scheduling)
// A real system needs both. Keep the two concepts separate when explaining the project.
namespace io {

enum class Operation { READ, WRITE, COPY, DELETE, MKDIR };
enum class Policy { FCFS, PRIORITY };
enum class Status { PENDING, SERVING, COMPLETED, FAILED };

inline const char* operationName(Operation operation) {
    switch (operation) {
        case Operation::READ: return "READ";
        case Operation::WRITE: return "WRITE";
        case Operation::COPY: return "COPY";
        case Operation::DELETE: return "DELETE";
        case Operation::MKDIR: return "MKDIR";
    }
    return "?";
}

inline const char* policyName(Policy policy) {
    return policy == Policy::FCFS ? "FCFS" : "Priority";
}

inline const char* statusName(Status status) {
    switch (status) {
        case Status::PENDING: return "PENDING";
        case Status::SERVING: return "SERVING";
        case Status::COMPLETED: return "COMPLETED";
        case Status::FAILED: return "FAILED";
    }
    return "?";
}

struct Request {
    int id = 0;                       // unique request id
    int pid = 0;                      // originating process
    std::string application;          // application name
    std::string path;                 // file operated on
    std::string destination;          // COPY/MKDIR target
    Operation operation = Operation::READ;
    int priority = 3;                 // 1 highest, 3 lowest (documented convention)
    int submitted = 0;                // simulation tick when queued
    int serviceTicks = 1;             // simulated service duration
    int remaining = 1;                // ticks still required
    int started = -1;                 // tick service began
    int completed = -1;               // tick service finished
    int order = 0;                    // 1-based service order
    int blocks = 0;                   // blocks touched by the operation
    Status status = Status::PENDING;
    bool ok = true;
    std::string note;                 // outcome / error text, or data read
    std::string payload;              // data supplied by WRITE, filled by READ

    int wait() const { return completed >= 0 ? completed - submitted : -1; }
};

// One tick transfers one 16-byte block. Simulated, not a hardware measurement.
inline int serviceTicksFor(std::size_t bytes) {
    const std::size_t blocks = bytes == 0 ? 1 : (bytes + 15) / 16;
    return std::max(1, static_cast<int>(blocks));
}

// Returns the index of the request that must run next, or -1 when the queue is empty.
// FCFS: submission order, then request id.
// Priority: lowest priority number first, then submission time, then request id.
inline int select(const std::vector<Request>& requests, Policy policy,
                  std::string* reason = nullptr, int now = 1 << 30) {
    int best = -1;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        if (requests[i].status != Status::PENDING) continue;
        if (requests[i].submitted > now) continue;
        if (best < 0) { best = static_cast<int>(i); continue; }
        const Request& candidate = requests[i];
        const Request& current = requests[static_cast<std::size_t>(best)];
        bool better = false;
        if (policy == Policy::PRIORITY && candidate.priority != current.priority)
            better = candidate.priority < current.priority;
        else if (candidate.submitted != current.submitted)
            better = candidate.submitted < current.submitted;
        else
            better = candidate.id < current.id;
        if (better) best = static_cast<int>(i);
    }
    if (best >= 0 && reason != nullptr) {
        const Request& chosen = requests[static_cast<std::size_t>(best)];
        std::ostringstream text;
        if (policy == Policy::FCFS) {
            text << "request #" << chosen.id << " (" << operationName(chosen.operation) << ' '
                 << chosen.path << ", P" << chosen.pid << ')'
                 << " selected: FCFS services requests in submission order, and it was submitted at t="
                 << chosen.submitted << " before every other pending request.";
        } else {
            text << "request #" << chosen.id << " (" << operationName(chosen.operation) << ' '
                 << chosen.path << ", P" << chosen.pid << " priority " << chosen.priority << ')'
                 << " selected: lowest priority number among pending requests";
            std::vector<int> rivals;
            for (std::size_t i = 0; i < requests.size(); ++i) {
                if (static_cast<int>(i) == best || requests[i].status != Status::PENDING) continue;
                if (requests[i].priority == chosen.priority) rivals.push_back(requests[i].id);
            }
            if (!rivals.empty()) {
                text << " (equal priority with ";
                for (std::size_t i = 0; i < rivals.size(); ++i)
                    text << (i ? ", #" : "#") << rivals[i];
                text << "; tie broken by submission time, then request id)";
            } else {
                text << "; no pending request has a better priority";
            }
            text << '.';
        }
        *reason = text.str();
    }
    return best;
}

struct Stats {
    int completed = 0, failed = 0, totalWait = 0, maxWait = 0;
    double averageWait = 0.0;
};

inline Stats statistics(const std::vector<Request>& requests) {
    Stats stats;
    for (const Request& request : requests) {
        if (request.status == Status::COMPLETED) ++stats.completed;
        if (request.status == Status::FAILED) ++stats.failed;
        if (request.completed >= 0) {
            stats.totalWait += request.wait();
            stats.maxWait = std::max(stats.maxWait, request.wait());
        }
    }
    const int serviced = stats.completed + stats.failed;
    if (serviced > 0) stats.averageWait = static_cast<double>(stats.totalWait) / serviced;
    return stats;
}

inline void printTable(const std::vector<Request>& requests, std::ostream& out,
                       const std::string& title) {
    out << title << "\n"
        << "ID  Pid App               Operation Path                     Dest                     Pri Submit Start Done  Blocks Status  Order\n";
    for (const Request& request : requests) {
        out << std::left << std::setw(4) << request.id
            << std::setw(4) << ('P' + std::to_string(request.pid))
            << std::setw(18) << request.application
            << std::setw(10) << operationName(request.operation)
            << std::setw(25) << request.path
            << std::setw(25) << (request.destination.empty() ? "-" : request.destination)
            << std::right << std::setw(3) << request.priority
            << std::setw(7) << request.submitted
            << std::setw(6) << (request.started < 0 ? -1 : request.started)
            << std::setw(6) << (request.completed < 0 ? -1 : request.completed)
            << std::setw(7) << request.blocks
            << ' ' << std::left << std::setw(10) << statusName(request.status)
            << std::right << std::setw(5) << request.order << '\n';
    }
}

// Runs a single simulated I/O device over a fixed request set. Used for the
// controlled FCFS-vs-priority comparison: the request set and submission times
// are identical in both runs, so only the ordering policy changes.
inline std::vector<Request> simulate(std::vector<Request> requests, Policy policy,
                                     std::vector<std::string>* reasons = nullptr,
                                     std::ostream* out = nullptr) {
    for (std::size_t i = 0; i < requests.size(); ++i) requests[i].id = static_cast<int>(i) + 1;
    int time = 0, order = 0, serviced = 0;
    while (serviced < static_cast<int>(requests.size())) {
        std::string reason;
        const int index = select(requests, policy, &reason, time);
        if (index < 0) { ++time; continue; }
        Request& request = requests[static_cast<std::size_t>(index)];
        request.status = Status::SERVING;
        request.started = time;
        request.order = ++order;
        if (reason != std::string() && reasons != nullptr) reasons->push_back(reason);
        if (out != nullptr && !reason.empty())
            *out << "[t=" << time << "] I/O scheduler: " << reason << '\n';
        ++time;
        for (int remaining = request.remaining - 1; remaining > 0; --remaining) {
            if (out != nullptr)
                *out << "[t=" << time << "] device continues request #" << request.id << " ("
                     << operationName(request.operation) << ' ' << request.path << ")\n";
            ++time;
        }
        request.completed = time;
        request.status = Status::COMPLETED;
        ++serviced;
    }
    return requests;
}

// A reproducible I/O-priority demonstration using the same request set under
// both policies. CPU scheduling is not involved here; these are device requests.
inline void demo(std::ostream& out = std::cout) {
    std::vector<Request> base;
    const auto add = [&](int pid, const char* app, Operation op, const char* path,
                         const char* dest, int priority, int submitted, int ticks) {
        Request request;
        request.pid = pid;
        request.application = app;
        request.operation = op;
        request.path = path;
        request.destination = dest;
        request.priority = priority;
        request.submitted = submitted;
        request.blocks = ticks;
        request.serviceTicks = ticks;
        request.remaining = ticks;
        base.push_back(request);
    };
    // Five background reads already queued when the editor write arrives at t=2.
    add(7, "indexer", Operation::READ, "/docs/source.c", "", 3, 0, 2);
    add(7, "indexer", Operation::READ, "/docs/config.json", "", 3, 0, 4);
    add(7, "indexer", Operation::READ, "/docs/numbers.csv", "", 3, 0, 3);
    add(7, "indexer", Operation::READ, "/docs/style.txt", "", 3, 1, 2);
    add(7, "indexer", Operation::READ, "/docs/report.txt", "", 3, 1, 5);
    add(2, "text-editor", Operation::WRITE, "/apps/notes.txt", "", 1, 2, 2);
    add(4, "file-search", Operation::READ, "/docs/system.log", "", 2, 3, 6);

    out << "\n=== I/O request prioritisation: identical request set, two policies ===\n"
        << "Priority 1 is the most important file request; 3 is a background request.\n"
        << "The service order decides *which pending request runs next*.\n"
        << "It is separate from the disk-head movement shown by disk.h.\n";

    std::vector<Request> fcfs = simulate(base, Policy::FCFS);
    std::vector<Request> priority = simulate(base, Policy::PRIORITY);

    out << "\nService order comparison\nOrder  FCFS                      Priority\n";
    for (std::size_t i = 0; i < fcfs.size(); ++i) {
        const Request* a = nullptr;
        const Request* b = nullptr;
        for (const Request& request : fcfs) if (request.order == static_cast<int>(i) + 1) a = &request;
        for (const Request& request : priority) if (request.order == static_cast<int>(i) + 1) b = &request;
        out << std::right << std::setw(4) << i + 1 << "   " << std::left << std::setw(26)
            << ('#' + std::to_string(a->id) + ' ' + a->application + ' ' +
                operationName(a->operation))
            << ('#' + std::to_string(b->id) + ' ' + b->application + ' ' +
                operationName(b->operation));
        if (a->id != b->id) out << "  <- reordered";
        out << '\n';
    }

    out << "\nPer-request wait\n"
        << "ID  Application        Operation Priority submitted FCFS order/wait Priority order/wait\n";
    for (const Request& request : fcfs) {
        const Request* other = nullptr;
        for (const Request& candidate : priority)
            if (candidate.id == request.id) other = &candidate;
        out << std::left << std::setw(4) << request.id << std::setw(19) << request.application
            << std::setw(10) << operationName(request.operation)
            << std::right << std::setw(8) << request.priority
            << std::setw(10) << request.submitted
            << std::setw(13) << (std::to_string(request.order) + "/" + std::to_string(request.wait()))
            << std::setw(16) << (other ? (std::to_string(other->order) + "/" +
                                          std::to_string(other->wait())) : std::string("-"))
            << '\n';
    }
    int fcfsEditor = -1, priorityEditor = -1;
    for (const Request& request : fcfs)
        if (request.application == "text-editor") fcfsEditor = request.wait();
    for (const Request& request : priority)
        if (request.application == "text-editor") priorityEditor = request.wait();
    const Stats f = statistics(fcfs), p = statistics(priority);
    out << std::fixed << std::setprecision(2)
        << "\nEditor write wait: FCFS " << fcfsEditor << " ticks, Priority " << priorityEditor
        << " ticks. Average wait over all requests: FCFS " << f.averageWait << ", Priority "
        << p.averageWait << " ticks (priority trading background throughput for latency).\n"
        << "The high-priority editor write is no longer stuck behind five background reads.\n"
        << "This is a configurable educational policy of our simulator, not a universal OS rule.\n";
}

} // namespace io

#endif
