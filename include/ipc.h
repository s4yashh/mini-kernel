#ifndef MINI_KERNEL_IPC_H
#define MINI_KERNEL_IPC_H

#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ipc {

struct Message { int sender = 0; std::string text; };

// Bounded FIFO mailbox models message passing and back-pressure between processes.
class MessageQueue {
    std::size_t capacity_;
    std::deque<Message> messages_;
public:
    explicit MessageQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) throw std::invalid_argument("Message queue capacity must be positive");
    }

    bool send(int sender, const std::string& text) {
        if (sender <= 0 || text.empty()) throw std::invalid_argument("Invalid message");
        if (messages_.size() == capacity_) return false;
        messages_.push_back({sender, text});
        return true;
    }

    bool receive(Message& message) {
        if (messages_.empty()) return false;
        message = messages_.front();
        messages_.pop_front();
        return true;
    }

    std::size_t size() const { return messages_.size(); }
    bool empty() const { return messages_.empty(); }
};

// Shared-memory region with explicit reader/writer permissions for the demo.
class SharedMemory {
    std::string data_;
    std::vector<int> readers_;
    int writer_;
public:
    explicit SharedMemory(int writer, std::vector<int> readers = {})
        : readers_(std::move(readers)), writer_(writer) {
        if (writer <= 0) throw std::invalid_argument("Writer PID must be positive");
        readers_.push_back(writer);
    }

    void write(int pid, const std::string& value) {
        if (pid != writer_) throw std::runtime_error("Shared-memory write denied: process is not the owner");
        data_ = value;
    }

    std::string read(int pid) const {
        for (int reader : readers_) if (reader == pid) return data_;
        throw std::runtime_error("Shared-memory read denied: process has no permission");
    }
};

// Shows queue-based message passing and a permission-checked shared-memory exchange.
inline void demo(std::ostream& out = std::cout) {
    out << "\nIPC demo: message passing\n";
    MessageQueue queue(2);
    out << "P1 sends 'compile /src/main.cpp': "
        << (queue.send(1, "compile /src/main.cpp") ? "queued" : "blocked") << '\n';
    out << "P2 sends 'status?': " << (queue.send(2, "status?") ? "queued" : "blocked") << '\n';
    out << "P3 sends 'extra': " << (queue.send(3, "extra") ? "queued" : "blocked: queue full") << '\n';
    Message message;
    while (queue.receive(message))
        out << "Receiver gets from P" << message.sender << ": " << message.text << '\n';

    out << "\nIPC demo: shared memory\n";
    SharedMemory region(1, {2, 3});
    region.write(1, "result=42");
    out << "P2 reads shared value: " << region.read(2) << '\n';
    try { region.write(2, "tampered"); }
    catch (const std::runtime_error& error) { out << "P2 write rejected: " << error.what() << '\n'; }
}

} // namespace ipc

#endif
