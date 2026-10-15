#ifndef MINI_KERNEL_SYNC_H
#define MINI_KERNEL_SYNC_H

#include <algorithm>
#include <deque>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace syncsim {

// A counting semaphore transfers a released permit directly to its oldest waiter.
// Therefore signal() leaves value at zero when it wakes a blocked process.
class Semaphore {
    int value_;
    std::deque<std::string> blocked_;
public:
    explicit Semaphore(int initial) : value_(initial) {
        if (initial < 0) throw std::invalid_argument("Negative semaphore value");
    }
    bool wait(const std::string& pid) {
        if (value_ > 0) { --value_; return true; }
        blocked_.push_back(pid);
        return false;
    }
    std::string signal() {
        if (!blocked_.empty()) {
            std::string pid = blocked_.front(); blocked_.pop_front(); return pid;
        }
        ++value_;
        return "";
    }
    int value() const { return value_; }
    const std::deque<std::string>& blocked() const { return blocked_; }
};

// A binary semaphore initialized to one gives mutually exclusive ownership.
class Mutex {
    Semaphore gate_{1};
public:
    bool lock(const std::string& pid) { return gate_.wait(pid); }
    std::string unlock() { return gate_.signal(); }
    int value() const { return gate_.value(); }
};

class ProducerConsumer {
    struct Pending { bool produce; int item; };
    std::size_t capacity_;
    std::deque<int> buffer_;
    Semaphore empty_, full_;
    Mutex mutex_;
    std::map<std::string, Pending> pending_;
    bool verbose_;
    std::ostream& out_;
    int step_ = 0;

    // Print the complete shared state after each meaningful simulated event.
    void log(const std::string& event) {
        if (!verbose_) return;
        out_ << "Step " << ++step_ << ": " << event << " | buffer=[";
        for (std::size_t i = 0; i < buffer_.size(); ++i)
            out_ << (i ? "," : "") << buffer_[i];
        out_ << "] empty=" << empty_.value() << " full=" << full_.value()
             << " mutex=" << mutex_.value() << '\n';
    }

    // Complete an operation after its empty/full permit is obtained. A wakeup
    // already owns the transferred permit, so it must not call wait() again.
    void execute(const std::string& pid, bool produce, int item, bool reserved) {
        Semaphore& first = produce ? empty_ : full_;
        if (!reserved && !first.wait(pid)) {
            pending_[pid] = {produce, item};
            log(pid + (produce ? " BLOCKED: buffer full" : " BLOCKED: buffer empty"));
            return;
        }
        log(pid + (reserved ? " resumes with reserved permit" : " gets semaphore permit"));
        if (!mutex_.lock(pid)) throw std::logic_error("Unexpected mutex contention");
        log(pid + " locks mutex");
        if (produce) {
            buffer_.push_back(item);
            log(pid + " produces " + std::to_string(item));
        } else {
            int taken = buffer_.front(); buffer_.pop_front();
            log(pid + " consumes " + std::to_string(taken));
        }
        mutex_.unlock();
        log(pid + " unlocks mutex");
        std::string woken = (produce ? full_ : empty_).signal();
        log(pid + (produce ? " signals full" : " signals empty") +
            (woken.empty() ? "" : "; wakes " + woken));
        if (!woken.empty()) {
            Pending next = pending_.at(woken);
            pending_.erase(woken);
            execute(woken, next.produce, next.item, true);
        }
    }
public:
    // Empty starts at capacity, full at zero, and mutex at one.
    explicit ProducerConsumer(std::size_t capacity, bool verbose = true,
                              std::ostream& out = std::cout)
        : capacity_(capacity), empty_(static_cast<int>(capacity)), full_(0),
          verbose_(verbose), out_(out) {
        if (capacity == 0) throw std::invalid_argument("Buffer capacity must be positive");
        log("initial state (capacity=" + std::to_string(capacity_) + ")");
    }
    void produce(const std::string& pid, int item) { execute(pid, true, item, false); }
    void consume(const std::string& pid) { execute(pid, false, 0, false); }
};

// This script forces both empty-buffer and full-buffer blocking, followed by wakeups.
inline void producerConsumerDemo(bool verbose = true, std::ostream& out = std::cout) {
    ProducerConsumer demo(2, verbose, out);
    demo.consume("C1");
    demo.produce("P1", 10);
    demo.produce("P1", 20);
    demo.produce("P1", 30);
    demo.produce("P2", 40);
    demo.consume("C2");
    demo.consume("C2");
    demo.consume("C2");
}

// Five philosophers first take their left fork. Each then needs the next fork,
// which another philosopher holds: circular wait means none can advance.
inline void diningDeadlock(bool verbose = true, std::ostream& out = std::cout) {
    if (!verbose) return;
    out << "\nDining philosophers: deadlocking left-then-right policy\n";
    int owner[5] = {-1,-1,-1,-1,-1};
    for (int p = 0; p < 5; ++p) {
        owner[p] = p;
        out << "Step " << p + 1 << ": P" << p << " takes left fork " << p << '\n';
    }
    for (int p = 0; p < 5; ++p) {
        int right = (p + 1) % 5;
        out << "Step " << p + 6 << ": P" << p << " BLOCKED on right fork "
            << right << " held by P" << owner[right] << '\n';
    }
    out << "DEADLOCK: all five hold one fork and wait in a cycle.\n";
}

// Number forks and acquire the lower-numbered one first. This breaks circular
// wait. The scripted order shows P3 can eat, release, then unblock its neighbors.
inline void diningFixed(bool verbose = true, std::ostream& out = std::cout) {
    if (!verbose) return;
    out << "\nDining philosophers: resource ordering (lower fork first)\n";
    int owner[5] = {-1,-1,-1,-1,-1};
    int first[5], second[5], step = 0;
    for (int p = 0; p < 5; ++p) {
        first[p] = std::min(p, (p + 1) % 5);
        second[p] = std::max(p, (p + 1) % 5);
        if (owner[first[p]] < 0) {
            owner[first[p]] = p;
            out << "Step " << ++step << ": P" << p << " takes fork " << first[p] << '\n';
        } else {
            out << "Step " << ++step << ": P" << p << " BLOCKED on fork "
                << first[p] << " held by P" << owner[first[p]] << '\n';
        }
    }
    for (int p = 0; p < 4; ++p) {
        int fork = second[p];
        if (owner[fork] < 0) {
            owner[fork] = p;
            out << "Step " << ++step << ": P" << p << " takes fork " << fork << '\n';
        } else {
            out << "Step " << ++step << ": P" << p << " BLOCKED on fork " << fork
                << " held by P" << owner[fork] << '\n';
        }
    }
    for (int p = 3; p >= 0; --p) {
        out << "Step " << ++step << ": P" << p << " eats and releases forks "
            << first[p] << " and " << second[p] << '\n';
        owner[first[p]] = owner[second[p]] = -1;
        if (p > 0) {
            owner[second[p - 1]] = p - 1;
            out << "Step " << ++step << ": P" << p - 1 << " wakes and takes fork "
                << second[p - 1] << '\n';
        }
    }
    owner[first[4]] = 4;
    out << "Step " << ++step << ": P4 wakes and takes fork " << first[4] << '\n';
    owner[second[4]] = 4;
    out << "Step " << ++step << ": P4 takes fork " << second[4] << '\n';
    out << "Step " << ++step << ": P4 eats and releases forks 0 and 4\n";
    out << "All five eat; resource ordering prevents circular wait.\n";
}

inline void diningDemo(bool verbose = true, std::ostream& out = std::cout) {
    diningDeadlock(verbose, out);
    diningFixed(verbose, out);
}

} // namespace syncsim
#endif
