#ifndef MINI_KERNEL_SYNC_H
#define MINI_KERNEL_SYNC_H

#include <algorithm>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <vector>

namespace syncsim {

// A counting semaphore with deterministic FIFO hand-off for the simulator.
class Semaphore {
    int permits_;
    std::deque<std::string> blocked_;
public:
    explicit Semaphore(int permits) : permits_(permits) {
        if (permits < 0) throw std::invalid_argument("Semaphore value cannot be negative");
    }

    bool wait(const std::string& process) {
        if (permits_ > 0) { --permits_; return true; }
        blocked_.push_back(process);
        return false;
    }

    std::string signal() {
        if (!blocked_.empty()) {
            const std::string process = blocked_.front();
            blocked_.pop_front();
            return process;
        }
        ++permits_;
        return {};
    }

    int value() const { return permits_; }
    const std::deque<std::string>& blocked() const { return blocked_; }
};

// Implements a mutex using a binary semaphore.
class Mutex {
    Semaphore gate_{1};
public:
    bool lock(const std::string& process) { return gate_.wait(process); }
    std::string unlock() { return gate_.signal(); }
    int value() const { return gate_.value(); }
};

// Simulates bounded-buffer semaphore operations without creating real threads.
class ProducerConsumer {
    std::deque<int> buffer_;
    Semaphore empty_;
    Semaphore full_{0};
    Mutex mutex_;
    bool verbose_;
    std::ostream& out_;
    int step_ = 0;

    void show(const std::string& message) {
        if (!verbose_) return;
        out_ << "Step " << ++step_ << ": " << message << " | buffer=[";
        for (std::size_t i = 0; i < buffer_.size(); ++i)
            out_ << (i ? "," : "") << buffer_[i];
        out_ << "] empty=" << empty_.value() << " full=" << full_.value()
             << " mutex=" << mutex_.value() << '\n';
    }

public:
    ProducerConsumer(int capacity, bool verbose, std::ostream& out)
        : empty_(capacity), verbose_(verbose), out_(out) {
        if (capacity <= 0) throw std::invalid_argument("Buffer capacity must be positive");
        show("initial state");
    }

    std::string produce(const std::string& process, int item, bool handedPermit = false) {
        if (!handedPermit && !empty_.wait(process)) {
            show(process + " BLOCKED: buffer full");
            return {};
        }
        if (!mutex_.lock(process)) throw std::logic_error("Simulated mutex unexpectedly busy");
        buffer_.push_back(item);
        show(process + " produces " + std::to_string(item));
        mutex_.unlock();
        const std::string woken = full_.signal();
        show(process + " signals full" + (woken.empty() ? "" : "; wakes " + woken));
        return woken;
    }

    std::string consume(const std::string& process, bool handedPermit = false) {
        if (!handedPermit && !full_.wait(process)) {
            show(process + " BLOCKED: buffer empty");
            return {};
        }
        if (buffer_.empty()) throw std::logic_error("A full-slot permit has no item");
        if (!mutex_.lock(process)) throw std::logic_error("Simulated mutex unexpectedly busy");
        const int item = buffer_.front();
        buffer_.pop_front();
        show(process + " consumes " + std::to_string(item));
        mutex_.unlock();
        const std::string woken = empty_.signal();
        show(process + " signals empty" + (woken.empty() ? "" : "; wakes " + woken));
        return woken;
    }
};

// Shows an empty-buffer block and full-buffer block, then completes each hand-off.
inline void producerConsumerDemo(bool verbose = true, std::ostream& out = std::cout) {
    ProducerConsumer demo(2, verbose, out);
    demo.consume("C1");
    std::string woken = demo.produce("P1", 10);
    if (woken == "C1") demo.consume("C1", true);
    demo.produce("P1", 20);
    demo.produce("P1", 30);
    woken = demo.produce("P2", 40);
    if (woken == "P2") demo.produce("P2", 40, true);
    woken = demo.consume("C2");
    if (woken == "P2") demo.produce("P2", 40, true);
    demo.consume("C2");
    demo.consume("C2");
}

// Shows the circular wait caused by every philosopher taking the left fork first.
inline void diningDeadlock(bool verbose = true, std::ostream& out = std::cout) {
    if (!verbose) return;
    out << "\nDining philosophers: left fork first\n";
    for (int p = 0; p < 5; ++p) out << "P" << p << " takes fork " << p << '\n';
    for (int p = 0; p < 5; ++p)
        out << "P" << p << " BLOCKED waiting for fork " << (p + 1) % 5 << '\n';
    out << "DEADLOCK: circular wait among all five philosophers.\n";
}

// Uses a global fork ordering so circular wait cannot form.
inline void diningFixed(bool verbose = true, std::ostream& out = std::cout) {
    if (!verbose) return;
    out << "\nDining philosophers: resource ordering\n";
    for (int p = 0; p < 5; ++p) {
        const int left = p, right = (p + 1) % 5;
        out << "P" << p << " takes fork " << (left < right ? left : right)
            << " then fork " << (left < right ? right : left)
            << "; eats; releases both\n";
    }
    out << "All five eat; ordering prevents circular wait.\n";
}

inline void diningDemo(bool verbose = true, std::ostream& out = std::cout) {
    diningDeadlock(verbose, out);
    diningFixed(verbose, out);
}

// Shows the Peterson protocol's turn/flag hand-off with a fixed two-thread interleaving.
inline void petersonDemo(std::ostream& out = std::cout) {
    bool flag[2] = {false, false};
    int turn = 0;
    out << "\nPeterson's two-process mutual exclusion\n";
    flag[0] = true; turn = 1;
    out << "T0 raises flag[0], sets turn=1; enters critical section (T1 is not interested)\n";
    flag[1] = true; turn = 0;
    out << "T1 raises flag[1], sets turn=0; waits because T0 has priority\n";
    flag[0] = false;
    out << "T0 leaves critical section and lowers flag[0]\n";
    if (!(flag[0] && turn == 0)) out << "T1 now enters critical section; mutual exclusion preserved\n";
    flag[1] = false;
}

// Illustrates Bakery ticket order and deterministic tie-breaking by process ID.
inline void bakeryDemo(std::ostream& out = std::cout) {
    const std::vector<int> ticket = {2, 1, 1, 3};
    std::vector<int> order = {0, 1, 2, 3};
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return ticket[static_cast<std::size_t>(a)] < ticket[static_cast<std::size_t>(b)];
    });
    out << "\nLamport Bakery ticket order\nTickets: T0=2 T1=1 T2=1 T3=3\nCritical-section order: ";
    for (std::size_t i = 0; i < order.size(); ++i)
        out << (i ? " -> " : "") << 'T' << order[i];
    out << "\nEqual tickets are ordered by process ID.\n";
}

} // namespace syncsim

#endif
