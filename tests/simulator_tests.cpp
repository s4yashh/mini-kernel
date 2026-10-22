#include "apps.h"
#include "banker.h"
#include "deadlock.h"
#include "disk.h"
#include "fs.h"
#include "ipc.h"
#include "memory.h"
#include "process_manager.h"
#include "scheduler.h"
#include "sync.h"
#include "threading.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
bool closeTo(double actual, double expected) { return std::fabs(actual - expected) < 0.001; }

void testSchedulerAndPaging() {
    const std::vector<PCB> processes = sampleProcesses();
    struct Expected { cpu::Algorithm algorithm; double waiting, turnaround, response; int dispatches; };
    const Expected expected[] = {
        {cpu::Algorithm::FCFS, 11.00, 16.60, 11.00, 5},
        {cpu::Algorithm::SJF, 7.80, 13.40, 7.80, 5},
        {cpu::Algorithm::SRTF, 6.40, 12.00, 3.80, 7},
        {cpu::Algorithm::PRIORITY, 9.20, 14.80, 7.40, 6},
        {cpu::Algorithm::RR, 14.20, 19.80, 4.20, 11}
    };
    for (const Expected& item : expected) {
        const cpu::Result result = cpu::run(processes, item.algorithm, 3, false);
        assert(closeTo(result.averageWaiting, item.waiting));
        assert(closeTo(result.averageTurnaround, item.turnaround));
        assert(closeTo(result.averageResponse, item.response));
        assert(result.contextSwitches == item.dispatches);
    }
    const cpu::Result mlfq = cpu::runMLFQ(processes, false);
    assert(mlfq.processes.size() == processes.size());
    for (const PCB& process : mlfq.processes) assert(process.state == State::TERMINATED);
    const cpu::Result mlq = cpu::runMLQ(processes, false);
    assert(mlq.processes.size() == processes.size());
    for (const PCB& process : mlq.processes) assert(process.state == State::TERMINATED);

    const std::vector<int> refs = memory::sampleReferences();
    assert(memory::run(refs, 3, memory::Algorithm::FIFO, false).faults == 15);
    assert(memory::run(refs, 3, memory::Algorithm::LRU, false).faults == 12);
    assert(memory::run(refs, 3, memory::Algorithm::OPTIMAL, false).faults == 9);
}

void testBankerAndDisk() {
    banker::Banker bank = banker::sample();
    const banker::Safety safe = bank.safety(false);
    assert(safe.safe);
    assert((safe.sequence == std::vector<int>{1, 3, 4, 0, 2}));
    assert(bank.request(1, {1, 0, 2}, false) == banker::Decision::GRANTED);
    assert(bank.request(4, {3, 3, 0}, false) == banker::Decision::WAIT);
    assert(bank.request(0, {0, 2, 0}, false) == banker::Decision::DENIED);
    bank.releaseAllAndFinish(1, false);
    assert(bank.safety(false).safe);

    const std::vector<int> requests = disk::sampleRequests();
    assert(disk::schedule(requests, 53, 200, "FCFS", false).totalMovement == 640);
    assert(disk::schedule(requests, 53, 200, "SSTF", false).totalMovement == 236);
    assert(disk::schedule(requests, 53, 200, "SCAN", false).totalMovement == 236);
    assert(disk::schedule(requests, 53, 200, "C-SCAN", false).totalMovement == 382);
    assert(disk::schedule({10}, 53, 200, "SCAN", false).totalMovement == 53);
}

void testMemoryAndDeadlock() {
    memory::ContiguousAllocator allocator(100);
    assert(allocator.allocate(1, 20, memory::AllocationPolicy::FIRST_FIT));
    assert(allocator.allocate(2, 30, memory::AllocationPolicy::BEST_FIT));
    assert(allocator.release(1));
    assert(allocator.freeSpace() == 70);
    assert(memory::translateSegment({{100, 10}}, 0, 9) == 109);
    bool boundsCaught = false;
    try { memory::translateSegment({{100, 10}}, 0, 10); }
    catch (const std::out_of_range&) { boundsCaught = true; }
    assert(boundsCaught);

    std::ostringstream tlbTrace;
    memory::TLB tlb(1);
    const std::map<std::pair<int, int>, int> pages = {{{1, 2}, 7}};
    assert(tlb.access(1, 2, 4, 100, pages, tlbTrace).physicalAddress == 704);
    assert(tlb.access(1, 2, 8, 100, pages, tlbTrace).tlbHit);
    assert(!tlb.access(1, 3, 0, 100, pages, tlbTrace).present);

    const deadlock::Detection result = deadlock::detect(deadlock::sample(), false);
    assert((result.deadlocked == std::vector<int>{0, 1}));
    std::ostringstream graphTrace;
    assert(deadlock::hasCycle({{1}, {0}}, graphTrace));
    assert(!deadlock::hasCycle({{1}, {}}, graphTrace));
}

void testSyncIpcAndFileSystem() {
    syncsim::Semaphore semaphore(0);
    assert(!semaphore.wait("P1"));
    assert(semaphore.signal() == "P1");
    assert(semaphore.value() == 0);

    ipc::MessageQueue queue(1);
    assert(queue.send(1, "hello"));
    assert(!queue.send(2, "full"));
    ipc::Message message;
    assert(queue.receive(message) && message.sender == 1 && message.text == "hello");

    filesystem::FileSystem fs(16, 8);
    assert(fs.createDirectory("/data"));
    assert(fs.writeFile("/data/one.txt", "hello world", filesystem::Allocation::LINKED));
    assert(fs.readFile("/data/one.txt") == "hello world");
    assert(fs.readFile("/data/one.txt", "user", filesystem::Access::DIRECT, 6, 5) == "world");
    bool accessDenied = false;
    try { fs.readFile("/data/one.txt", "other"); }
    catch (const std::runtime_error&) { accessDenied = true; }
    assert(accessDenied);
    assert(fs.writeFile("/data/index.txt", "index me", filesystem::Allocation::INDEXED));
    assert(fs.entry("/data/index.txt").indexBlock >= 0);
    assert(fs.deleteFile("/data/one.txt"));
    assert(fs.deleteDirectory("/data") == false);
}

void testIntegratedAndProcesses() {
    std::ostringstream output;
    processsim::ProcessTable table;
    const int pid = table.create("demo", 2, 1, 0, output);
    table.blockForIO(pid, 1, "disk", output);
    table.completeIO(pid, 2, output);
    table.terminate(pid, 3, output);
    assert(table.all().front().state == State::TERMINATED);

    threadsim::modelDemo(output);
    apps::integratedDemo(output);
    assert(output.str().find("calculator") != std::string::npos);
    assert(output.str().find("/apps/notes.txt") != std::string::npos);
    assert(output.str().find("WAITING") != std::string::npos);
    assert(output.str().find("DISK serves block") != std::string::npos);
    assert(output.str().find("resources released") != std::string::npos);
    assert(output.str().find("Integrated run complete") != std::string::npos);
}
} // namespace

int main() {
    testSchedulerAndPaging();
    testBankerAndDisk();
    testMemoryAndDeadlock();
    testSyncIpcAndFileSystem();
    testIntegratedAndProcesses();
    std::cout << "All Mini Kernel Simulator tests passed.\n";
}
