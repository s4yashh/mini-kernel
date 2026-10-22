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
            throw std::invalid_argument("Disk request is outside the disk");
}

inline void moveTo(Result& result, int cylinder) {
    const int previous = result.headPath.back();
    result.totalMovement += std::abs(previous - cylinder);
    if (previous != cylinder) result.headPath.push_back(cylinder);
}

inline void service(Result& result, int cylinder) {
    moveTo(result, cylinder);
    result.serviceOrder.push_back(cylinder);
}

// Computes a disk-head path for FCFS, SSTF, SCAN (toward zero first), or C-SCAN.
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
                const int distance = std::abs(requests[i] - result.headPath.back());
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
        std::vector<int> lower, upper;
        for (int request : requests)
            (request < head ? lower : upper).push_back(request);
        std::sort(lower.begin(), lower.end());
        std::sort(upper.begin(), upper.end());
        if (algorithm == "SCAN") {
            for (auto it = lower.rbegin(); it != lower.rend(); ++it) service(result, *it);
            if (!requests.empty()) {
                moveTo(result, 0);
                for (int request : upper) service(result, request);
            }
        } else {
            for (int request : upper) service(result, request);
            if (!lower.empty()) {
                moveTo(result, diskSize - 1);
                moveTo(result, 0);
                for (int request : lower) service(result, request);
            }
        }
    } else {
        throw std::invalid_argument("Unknown disk scheduling algorithm");
    }

    if (!requests.empty()) result.averageSeek =
        static_cast<double>(result.totalMovement) / static_cast<double>(requests.size());
    if (verbose) {
        out << "\n" << algorithm << " disk scheduling\nService order: ";
        if (result.serviceOrder.empty()) out << "(none)";
        for (std::size_t i = 0; i < result.serviceOrder.size(); ++i)
            out << (i ? " -> " : "") << result.serviceOrder[i];
        out << "\nHead path: ";
        for (std::size_t i = 0; i < result.headPath.size(); ++i)
            out << (i ? " -> " : "") << result.headPath[i];
        out << "\nTotal movement: " << result.totalMovement
            << "\nAverage seek/request: " << std::fixed << std::setprecision(2)
            << result.averageSeek << '\n';
    }
    return result;
}

inline void compare(const std::vector<int>& requests, int head, int diskSize,
                    std::ostream& out = std::cout) {
    out << "\nAlgorithm  Total movement  Avg seek/request\n";
    for (const std::string& algorithm : {"FCFS", "SSTF", "SCAN", "C-SCAN"}) {
        const Result result = schedule(requests, head, diskSize, algorithm, false, out);
        out << std::left << std::setw(11) << algorithm << std::right << std::setw(14)
            << result.totalMovement << std::setw(19) << std::fixed << std::setprecision(2)
            << result.averageSeek << '\n';
    }
}

inline std::vector<int> sampleRequests() { return {98, 183, 37, 122, 14, 124, 65, 67}; }

} // namespace disk

#endif
