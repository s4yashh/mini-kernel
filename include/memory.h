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
