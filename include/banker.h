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
    // Compute Need = Maximum - Allocation and reject malformed resource tables.
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

    // Find an unfinished process whose remaining Need fits Work; pretend it finishes
    // and releases its Allocation, then repeat until all finish or no one can.
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

    // Try a request on a temporary copy. Commit only if the safety check still succeeds.
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

// The standard five-process example from Operating Systems textbooks.
inline Banker sample() {
    return Banker({3,3,2}, {{7,5,3},{3,2,2},{9,0,2},{2,2,2},{4,3,3}},
                  {{0,1,0},{2,0,0},{3,0,2},{2,1,1},{0,0,2}});
}

// Requests are applied in order to the same state, as in a real running system.
inline void requestDemo(std::ostream& out = std::cout) {
    Banker b = sample();
    b.request(1, {1,0,2}, true, out);
    b.request(4, {3,3,0}, true, out);
    b.request(0, {0,2,0}, true, out);
}
} // namespace banker
#endif
