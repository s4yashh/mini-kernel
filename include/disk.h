#ifndef MINI_KERNEL_DISK_H
#define MINI_KERNEL_DISK_H

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace disk {

// The path includes physical travel to disk edges; serviceOrder contains requests only.
struct Result {
    std::string algorithm;
    std::vector<int> serviceOrder;
    std::vector<int> headPath;
    int totalMovement = 0;
    double averageSeek = 0.0;
};

// Reject positions outside the disk before computing a route.
inline void validate(const std::vector<int>& requests, int head, int diskSize) {
    if (diskSize <= 0 || head < 0 || head >= diskSize)
        throw std::invalid_argument("Disk size/head position is invalid");
    for (int request : requests)
        if (request < 0 || request >= diskSize)
            throw std::invalid_argument("Request lies outside the disk");
}

// Record physical head travel, including travel that services no request.
inline void moveTo(Result& result, int cylinder) {
    int previous = result.headPath.back();
    result.totalMovement += (previous > cylinder ? previous - cylinder : cylinder - previous);
    if (previous != cylinder) result.headPath.push_back(cylinder);
}

// Add a request to the service order as the head reaches its cylinder.
inline void service(Result& result, int cylinder) {
    moveTo(result, cylinder);
    result.serviceOrder.push_back(cylinder);
}

// Compute one policy's service order, physical path, and seek statistics.
inline Result schedule(const std::vector<int>& requests, int head, int diskSize,
                       const std::string& algorithm, bool verbose = true,
                       std::ostream& out = std::cout) {
    validate(requests, head, diskSize);
    Result result{algorithm, {}, {head}, 0, 0.0};

    if (algorithm == "FCFS") {
        for (int request : requests) service(result, request);
    } else if (algorithm == "SSTF") {
        // Reconsider every outstanding request after each service. Ties use the lower cylinder.
        std::vector<bool> done(requests.size(), false);
        for (std::size_t count = 0; count < requests.size(); ++count) {
            std::size_t best = requests.size();
            int bestDistance = std::numeric_limits<int>::max();
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
        // This demo's SCAN starts toward 0; C-SCAN starts toward higher cylinders.
        std::vector<int> high, low;
        for (int request : requests) {
            if (request >= head) high.push_back(request);
            else low.push_back(request);
        }
        std::sort(high.begin(), high.end());
        std::sort(low.begin(), low.end());
        if (algorithm == "SCAN") {
            for (auto it = low.rbegin(); it != low.rend(); ++it) service(result, *it);
            if (!high.empty()) moveTo(result, 0);
            for (int request : high) service(result, request);
        } else {
            for (int request : high) service(result, request);
            if (!low.empty()) {
                moveTo(result, diskSize - 1);
                // The wrap is counted as physical movement; it services no request.
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

// Run all four policies on identical input and print a compact comparison.
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

// Return the standard eight-request exercise used in the menu demos.
inline std::vector<int> sampleRequests() { return {98, 183, 37, 122, 14, 124, 65, 67}; }

} // namespace disk

#endif
