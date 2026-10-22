#ifndef MINI_KERNEL_MEMORY_H
#define MINI_KERNEL_MEMORY_H

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <map>
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

// Main-memory allocation policies for variable-sized contiguous partitions.
enum class AllocationPolicy { FIRST_FIT, BEST_FIT, WORST_FIT };

inline const char* policyName(AllocationPolicy policy) {
    switch (policy) {
        case AllocationPolicy::FIRST_FIT: return "First Fit";
        case AllocationPolicy::BEST_FIT: return "Best Fit";
        case AllocationPolicy::WORST_FIT: return "Worst Fit";
    }
    return "Unknown";
}

struct Region { int start = 0, size = 0, pid = -1; };

class ContiguousAllocator {
    std::vector<Region> regions_;
    int capacity_;

    void coalesce() {
        for (std::size_t i = 1; i < regions_.size();) {
            if (regions_[i - 1].pid == -1 && regions_[i].pid == -1) {
                regions_[i - 1].size += regions_[i].size;
                regions_.erase(regions_.begin() + static_cast<std::ptrdiff_t>(i));
            } else ++i;
        }
    }

public:
    explicit ContiguousAllocator(int capacity) : capacity_(capacity) {
        if (capacity <= 0) throw std::invalid_argument("Memory capacity must be positive");
        regions_.push_back({0, capacity, -1});
    }

    bool allocate(int pid, int size, AllocationPolicy policy) {
        if (pid <= 0 || size <= 0) throw std::invalid_argument("PID and allocation size must be positive");
        for (const Region& region : regions_)
            if (region.pid == pid) throw std::invalid_argument("Process already has a memory allocation");
        int chosen = -1;
        for (std::size_t i = 0; i < regions_.size(); ++i) {
            const Region& region = regions_[i];
            if (region.pid != -1 || region.size < size) continue;
            if (chosen < 0 ||
                (policy == AllocationPolicy::BEST_FIT && region.size < regions_[chosen].size) ||
                (policy == AllocationPolicy::WORST_FIT && region.size > regions_[chosen].size))
                chosen = static_cast<int>(i);
            if (policy == AllocationPolicy::FIRST_FIT) break;
        }
        if (chosen < 0) return false;
        Region& free = regions_[static_cast<std::size_t>(chosen)];
        const int start = free.start;
        if (free.size == size) free.pid = pid;
        else {
            free.start += size;
            free.size -= size;
            regions_.insert(regions_.begin() + chosen, {start, size, pid});
        }
        return true;
    }

    bool release(int pid) {
        bool released = false;
        for (Region& region : regions_) if (region.pid == pid) {
            region.pid = -1;
            released = true;
            break;
        }
        if (released) coalesce();
        return released;
    }

    int capacity() const { return capacity_; }
    int freeSpace() const {
        int total = 0;
        for (const Region& region : regions_) if (region.pid == -1) total += region.size;
        return total;
    }
    int largestHole() const {
        int largest = 0;
        for (const Region& region : regions_) if (region.pid == -1 && region.size > largest)
            largest = region.size;
        return largest;
    }
    int externalFragmentation() const { return freeSpace() - largestHole(); }

    void print(std::ostream& out = std::cout) const {
        out << "Memory map (" << capacity_ << " units)\n";
        for (const Region& region : regions_)
            out << '[' << region.start << ".." << region.start + region.size - 1 << "] "
                << (region.pid < 0 ? "FREE" : "P" + std::to_string(region.pid))
                << " (" << region.size << ")\n";
        out << "Free=" << freeSpace() << ", external fragmentation="
            << externalFragmentation() << '\n';
    }
};

// Compares the three contiguous allocation policies on the same deterministic requests.
inline void compareAllocationPolicies(std::ostream& out = std::cout) {
    for (AllocationPolicy policy : {AllocationPolicy::FIRST_FIT, AllocationPolicy::BEST_FIT,
                                    AllocationPolicy::WORST_FIT}) {
        out << "\n" << policyName(policy) << "\n";
        ContiguousAllocator allocator(100);
        allocator.allocate(1, 20, policy);
        allocator.allocate(2, 15, policy);
        allocator.allocate(3, 25, policy);
        allocator.allocate(4, 10, policy);
        allocator.release(2);
        allocator.release(4);
        const bool success = allocator.allocate(5, 22, policy);
        out << "P5 request 22: " << (success ? "allocated" : "failed") << '\n';
        allocator.print(out);
    }
}

struct Segment { int base = 0, limit = 0; };

// Translates a logical segment/offset pair after checking the segment limit.
inline int translateSegment(const std::vector<Segment>& table, std::size_t segment,
                            int offset) {
    if (segment >= table.size() || offset < 0 || offset >= table[segment].limit)
        throw std::out_of_range("Segmentation fault: invalid segment or offset");
    return table[segment].base + offset;
}

struct PageTranslation { bool present = false, tlbHit = false; int frame = -1, physicalAddress = -1; };

// Uses a small LRU TLB in front of a supplied page table and reports each translation.
class TLB {
    struct Entry { int pid, page, frame; unsigned long age; };
    std::vector<Entry> entries_;
    std::size_t capacity_;
    unsigned long clock_ = 0;
public:
    explicit TLB(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) throw std::invalid_argument("TLB capacity must be positive");
    }

    PageTranslation access(int pid, int page, int offset, int pageSize,
                           const std::map<std::pair<int, int>, int>& pageTable,
                           std::ostream& out = std::cout) {
        if (pid < 0 || page < 0 || offset < 0 || pageSize <= 0 || offset >= pageSize)
            throw std::invalid_argument("Invalid virtual address");
        ++clock_;
        for (Entry& entry : entries_) if (entry.pid == pid && entry.page == page) {
            entry.age = clock_;
            const int physical = entry.frame * pageSize + offset;
            out << "TLB HIT: P" << pid << " page " << page << " -> frame " << entry.frame
                << ", physical address " << physical << '\n';
            return {true, true, entry.frame, physical};
        }
        const auto found = pageTable.find({pid, page});
        if (found == pageTable.end()) {
            out << "TLB MISS, PAGE FAULT: P" << pid << " page " << page << " is not resident\n";
            return {};
        }
        if (entries_.size() == capacity_) {
            const auto oldest = std::min_element(entries_.begin(), entries_.end(),
                [](const Entry& a, const Entry& b) { return a.age < b.age; });
            entries_.erase(oldest);
        }
        entries_.push_back({pid, page, found->second, clock_});
        const int physical = found->second * pageSize + offset;
        out << "TLB MISS: page-table lookup P" << pid << " page " << page << " -> frame "
            << found->second << ", physical address " << physical << '\n';
        return {true, false, found->second, physical};
    }
};

// Counts distinct pages in each sliding window; a working set larger than RAM signals pressure.
inline bool showWorkingSet(const std::vector<int>& references, std::size_t window,
                           int frameCount, std::ostream& out = std::cout) {
    if (window == 0 || frameCount <= 0) throw std::invalid_argument("Window and frames must be positive");
    bool thrashing = false;
    out << "\nWorking-set windows (window=" << window << ", frames=" << frameCount << ")\n";
    for (std::size_t end = 0; end < references.size(); ++end) {
        const std::size_t begin = end + 1 > window ? end + 1 - window : 0;
        std::vector<int> pages;
        for (std::size_t i = begin; i <= end; ++i)
            if (std::find(pages.begin(), pages.end(), references[i]) == pages.end())
                pages.push_back(references[i]);
        const bool pressure = pages.size() > static_cast<std::size_t>(frameCount);
        thrashing = thrashing || pressure;
        out << "t=" << end + 1 << " WSS=" << pages.size() << " pages [";
        for (std::size_t i = 0; i < pages.size(); ++i) out << (i ? " " : "") << pages[i];
        out << "] " << (pressure ? "MEMORY PRESSURE" : "OK") << '\n';
    }
    out << (thrashing ? "Thrashing risk: working set exceeds available frames.\n"
                      : "No working-set window exceeds available frames.\n");
    return thrashing;
}

// Combines allocation policies, segmentation bounds checks, TLB lookup, and working-set pressure.
inline void advancedDemo(std::ostream& out = std::cout) {
    compareAllocationPolicies(out);
    out << "\nSegmentation address translation\n";
    const std::vector<Segment> segments = {{1000, 100}, {2400, 80}, {4000, 200}};
    out << "Logical (segment 1, offset 25) -> physical "
        << translateSegment(segments, 1, 25) << '\n';
    try { translateSegment(segments, 1, 80); }
    catch (const std::out_of_range& error) { out << "Bounds check: " << error.what() << '\n'; }

    out << "\nTLB translation (page size 100)\n";
    TLB tlb(2);
    const std::map<std::pair<int, int>, int> pageTable = {{{1, 0}, 4}, {{1, 1}, 9}};
    tlb.access(1, 0, 12, 100, pageTable, out);
    tlb.access(1, 0, 35, 100, pageTable, out);
    tlb.access(1, 1, 4, 100, pageTable, out);
    tlb.access(1, 2, 0, 100, pageTable, out);

    showWorkingSet({1,2,1,3,4,3,4,5,6,5,6}, 4, 3, out);
}

}
#endif
