// The regression suite relies on assert(); keep it active even in Release builds.
#ifdef NDEBUG
#undef NDEBUG
#endif

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
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
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
    table.dispatch(pid, 1, output);
    table.blockForIO(pid, 1, "disk", output);
    table.completeIO(pid, 2, output);
    table.dispatch(pid, 2, output);
    table.terminate(pid, 3, output);
    assert(table.all().front().state == State::TERMINATED);

    threadsim::modelDemo(output);
    apps::integratedDemo(output);
    assert(output.str().find("calculator") != std::string::npos);
    assert(output.str().find("/apps/notes.txt") != std::string::npos);
    assert(output.str().find("WAITING") != std::string::npos);
    assert(output.str().find("DISK serves request #") != std::string::npos);
    assert(output.str().find("Banker resources") != std::string::npos);
    assert(output.str().find("Integrated run complete") != std::string::npos);
}

void testFileSystemOperations() {
    filesystem::FileSystem fs(8, 8);
    assert(fs.createDirectories("/a/b"));
    assert(fs.isDirectory("/a") && fs.isDirectory("/a/b"));
    assert(!fs.exists("/a/b/f.txt"));
    assert(fs.writeFile("/a/b/f.txt", "0123456789", filesystem::Allocation::CONTIGUOUS));
    assert(fs.exists("/a/b/f.txt") && fs.isFile("/a/b/f.txt"));
    assert(fs.entry("/a/b/f.txt").blocks.size() == 2);
    assert(fs.blocksOf("/a/b/f.txt") == 2);
    assert(fs.usedBlocks() == 2);
    assert(fs.readFile("/a/b/f.txt") == "0123456789");
    assert(fs.resolve("/a/b", "../x.txt") == "/a/x.txt");
    assert(fs.resolve("/a/b", "/c/d") == "/c/d");
    assert(fs.resolve("/", "..") == std::string());

    assert(fs.appendFile("/a/b/f.txt", "abc", filesystem::Allocation::LINKED));
    assert(fs.readFile("/a/b/f.txt") == "0123456789abc");
    assert(fs.entry("/a/b/f.txt").writes == 2);
    assert(fs.entry("/a/b/f.txt").allocation == filesystem::Allocation::LINKED);

    std::ostringstream metadata;
    fs.metadata("/a/b/f.txt", metadata);
    assert(metadata.str().find("linked") != std::string::npos);
    assert(metadata.str().find("13 bytes") != std::string::npos);

    assert(fs.renameFile("/a/b/f.txt", "/a/b/g.txt"));
    assert(!fs.exists("/a/b/f.txt") && fs.exists("/a/b/g.txt"));
    assert(!fs.renameFile("/a/b/g.txt", "/a/b/g.txt"));
    assert(fs.writeFile("/a/b/exists.txt", "x", filesystem::Allocation::CONTIGUOUS));
    assert(!fs.renameFile("/a/b/g.txt", "/a/b/exists.txt"));    // target exists
    assert(!fs.renameFile("/a/b/g.txt", "/a/b"));               // target is a directory
    assert(!fs.renameFile("/a/b/missing.txt", "/a/b/z.txt"));
    assert(fs.deleteFile("/a/b/exists.txt"));

    assert(fs.copyFile("/a/b/g.txt", "/a/b/h.txt", filesystem::Allocation::INDEXED));
    assert(fs.readFile("/a/b/h.txt") == "0123456789abc");
    assert(fs.blocksOf("/a/b/h.txt") == 3);                    // 2 data + 1 index
    assert(fs.entry("/a/b/h.txt").indexBlock >= 0);
    assert(fs.usedBlocks() == 5);

    assert(fs.moveFile("/a/b/h.txt", "/a/h.txt"));
    assert(fs.exists("/a/h.txt") && !fs.exists("/a/b/h.txt"));
    assert(!fs.copyFile("/a/h.txt", "/nope/h.txt", filesystem::Allocation::CONTIGUOUS));

    bool denied = false;
    try { fs.readFile("/a/h.txt", "guest"); }
    catch (const std::runtime_error&) { denied = true; }
    assert(denied);
    assert(!fs.writeFile("/a/h.txt", "hack", filesystem::Allocation::CONTIGUOUS, "guest"));
    assert(fs.writeFile("/a/h.txt", "owned", filesystem::Allocation::CONTIGUOUS, "admin"));
    assert(fs.readFile("/a/h.txt") == "owned");
    assert(fs.currentOwner("/a/h.txt") == "user");

    // Invalid paths and directory collisions are rejected.
    assert(!fs.writeFile("relative.txt", "x", filesystem::Allocation::CONTIGUOUS));
    assert(!fs.writeFile("/a/../x.txt", "x", filesystem::Allocation::CONTIGUOUS));
    assert(!fs.writeFile("/a", "x", filesystem::Allocation::CONTIGUOUS));
    assert(!fs.writeFile("/a/b/x//y.txt", "x", filesystem::Allocation::CONTIGUOUS));
    assert(!fs.writeFile("/missing/x.txt", "x", filesystem::Allocation::CONTIGUOUS));
    assert(!fs.createDirectory("/missing/sub"));

    assert(fs.deleteFile("/a/h.txt", "admin"));
    assert(fs.deleteFile("/a/b/g.txt"));
    assert(fs.usedBlocks() == 0);
    assert(fs.deleteDirectory("/a/b"));                        // empty
    assert(fs.deleteDirectory("/a"));                          // empty
    assert(!fs.deleteDirectory("/"));

    // Exact-fit and exhaustion behaviour.
    filesystem::FileSystem small(3, 4);
    assert(small.createDirectory("/d"));
    assert(small.writeFile("/d/x", "123456789012", filesystem::Allocation::CONTIGUOUS)); // 3 blocks
    assert(small.freeBlocks() == 0);
    assert(!small.writeFile("/d/y", "z", filesystem::Allocation::LINKED));
    assert(!small.writeFile("/d/x", "1234567890123", filesystem::Allocation::CONTIGUOUS));
    assert(small.readFile("/d/x") == "123456789012");          // unchanged by failed write
    assert(small.deleteFile("/d/x"));
    assert(small.freeBlocks() == 3);
    // Indexed allocation needs one extra block, so the same file no longer fits.
    assert(!small.writeFile("/d/i", "123456789012", filesystem::Allocation::INDEXED));
    assert(small.writeFile("/d/i", "12345678", filesystem::Allocation::INDEXED)); // 2 data + index
    assert(small.blocksOf("/d/i") == 3);
    assert(small.freeBlocks() == 0);
}

void testApplicationsAndKernel() {
    const std::vector<apps::Program> programs = apps::catalog();
    assert(programs.size() >= 5);
    std::set<int> pids;
    for (const apps::Program& program : programs) {
        assert(program.spec.pid > 0);
        assert(program.cpuBurst() > 0);
        assert(program.spec.memory > 0);
        assert(program.spec.priority >= 1 && program.spec.priority <= 3);
        assert(!program.steps.empty());
        pids.insert(program.spec.pid);
    }
    assert(pids.size() == programs.size());

    apps::Simulator::Options options;
    options.verbose = false;
    std::ostringstream sink;
    apps::Simulator simulator(programs, options, sink);
    simulator.run();

    assert(simulator.processes().size() == programs.size());
    int completed = 0;
    for (const apps::Runtime& runtime : simulator.processes()) {
        assert(runtime.finished);
        assert(runtime.process.state == State::TERMINATED);
        assert(runtime.process.finish > 0);
        assert(runtime.process.response >= 0);
        assert(runtime.dispatches >= 1);
        assert(runtime.cpuTicks > 0);
        assert(runtime.process.turnaround ==
               runtime.process.finish - runtime.process.arrival);
        assert(runtime.process.waiting >= 0);
        assert(runtime.process.waiting + runtime.process.burst + runtime.ioWait ==
               runtime.process.turnaround);
        assert(!runtime.result.empty());
        ++completed;
    }
    assert(completed == static_cast<int>(programs.size()));
    assert(simulator.elapsed() > 0);
    assert(simulator.gantt().size() == static_cast<std::size_t>(simulator.elapsed()));
    assert(simulator.cpuUtilization() > 0.0 && simulator.cpuUtilization() <= 100.0);
    assert(simulator.dispatches() >= static_cast<int>(programs.size()));

    // Every request was served exactly once.
    int expectedOrder = 1;
    for (const io::Request& request : simulator.requests()) {
        assert(request.status == io::Status::COMPLETED);
        assert(request.ok);
        assert(request.order > 0);
        assert(request.completed > request.started);
        ++expectedOrder;
    }
    std::vector<int> orders;
    for (const io::Request& request : simulator.requests()) orders.push_back(request.order);
    std::sort(orders.begin(), orders.end());
    for (std::size_t i = 0; i < orders.size(); ++i)
        assert(orders[i] == static_cast<int>(i) + 1);
    assert(simulator.ioRequests() > 0);

    // The applications really used the simulated file system.
    const filesystem::FileSystem& files = simulator.files();
    assert(files.exists("/apps/calculator.txt"));
    assert(files.readFile("/apps/calculator.txt") == "14 + 28 = 42");
    assert(files.readFile("/apps/search.txt").find("2 match") != std::string::npos);
    assert(files.readFile("/apps/counter.txt").find("500500") != std::string::npos);
    assert(files.readFile("/apps/analysis.txt").find("lines=6") != std::string::npos);
    assert(files.readFile("/apps/report-copy.txt") == files.readFile("/docs/report.txt"));
    assert(files.readFile("/apps/notes.txt").find("[edited by text-editor]") != std::string::npos);
    assert(files.exists("/apps/index.txt") && files.exists("/apps/monitor.txt"));

    // Tie-break: equal priority and arrival -> lower PID first.
    std::vector<apps::Program> tie;
    tie.push_back(apps::makeProgram(apps::Kind::COUNTER, 2, 2, 0));
    tie.push_back(apps::makeProgram(apps::Kind::CALCULATOR, 1, 2, 0));
    apps::Simulator tieSimulator(tie, options, sink);
    tieSimulator.run();
    assert(tieSimulator.gantt().front() == 1);
    assert(tieSimulator.byPid(1).process.turnaround > 0);
}

void testPriorityDecisions() {
    apps::Simulator::Options options;
    options.verbose = false;
    std::ostringstream sink;

    // A priority-1 application arrives late and preempts the priority-3 process.
    std::vector<apps::Program> programs;
    programs.push_back(apps::makeProgram(apps::Kind::COUNTER, 1, 3, 0));
    programs.push_back(apps::makeProgram(apps::Kind::CALCULATOR, 2, 1, 2));
    apps::Simulator simulator(programs, options, sink);
    simulator.run();
    assert(simulator.gantt().size() >= 4);
    assert(simulator.gantt()[0] == 1);
    assert(simulator.gantt()[1] == 1);
    assert(simulator.gantt()[2] == 2);                        // preemption at t=2
    bool preempted = false, explained = false;
    for (const std::string& decision : simulator.decisions()) {
        if (decision.find("preempts") != std::string::npos) preempted = true;
        if (decision.find("priority") != std::string::npos) explained = true;
    }
    assert(preempted && explained);
    assert(simulator.byPid(1).process.finish > 2);
    assert(simulator.byPid(1).dispatches >= 2);

    // Equal priority, different arrival: the earlier arrival is dispatched first.
    std::vector<apps::Program> arrivals;
    arrivals.push_back(apps::makeProgram(apps::Kind::FILE_ANALYZER, 1, 2, 0));
    arrivals.push_back(apps::makeProgram(apps::Kind::TEXT_EDITOR, 2, 2, 1));
    arrivals.push_back(apps::makeProgram(apps::Kind::CALCULATOR, 3, 1, 0));
    apps::Simulator arrivalSimulator(arrivals, options, sink);
    arrivalSimulator.run();
    assert(arrivalSimulator.gantt()[0] == 3);                 // priority 1 first
    assert(arrivalSimulator.gantt()[2] == 1);                 // tie in priority 2 -> earlier arrival
    assert(arrivalSimulator.byPid(2).process.response > arrivalSimulator.byPid(1).process.response);

    // Without preemption the late high-priority job must wait for the current one.
    options.preemptive = false;
    apps::Simulator nonPreemptive(programs, options, sink);
    nonPreemptive.run();
    assert(nonPreemptive.gantt()[2] == 1);
    options.preemptive = true;
}

void testIoPolicies() {
    std::vector<io::Request> base(4);
    const int priorities[4] = {3, 1, 2, 3};
    const int submitted[4] = {0, 2, 1, 0};
    for (int i = 0; i < 4; ++i) {
        base[static_cast<std::size_t>(i)].pid = i + 1;
        base[static_cast<std::size_t>(i)].application = "app";
        base[static_cast<std::size_t>(i)].path = "/docs/file";
        base[static_cast<std::size_t>(i)].priority = priorities[i];
        base[static_cast<std::size_t>(i)].submitted = submitted[i];
        base[static_cast<std::size_t>(i)].serviceTicks = 1;
        base[static_cast<std::size_t>(i)].remaining = 1;
    }

    std::vector<std::string> reasons;
    const std::vector<io::Request> fcfs = io::simulate(base, io::Policy::FCFS, &reasons);
    const std::vector<io::Request> priority = io::simulate(base, io::Policy::PRIORITY);
    assert(reasons.size() == 4);
    for (const std::string& reason : reasons)
        assert(reason.find("FCFS") != std::string::npos);

    std::vector<int> fcfsOrder, priorityOrder;
    for (int order = 1; order <= 4; ++order) {
        for (const io::Request& request : fcfs) if (request.order == order) fcfsOrder.push_back(request.id);
        for (const io::Request& request : priority) if (request.order == order) priorityOrder.push_back(request.id);
    }
    assert((fcfsOrder == std::vector<int>{1, 4, 3, 2}));
    assert((priorityOrder == std::vector<int>{1, 3, 2, 4}));

    std::string reason;
    io::Request first = base.front();
    first.id = 1;
    first.status = io::Status::PENDING;
    const std::vector<io::Request> single(1, first);
    assert(io::select(single, io::Policy::PRIORITY, &reason) == 0);
    assert(reason.find("priority") != std::string::npos);
    assert(io::select(std::vector<io::Request>(), io::Policy::FCFS) == -1);

    // Same applications, same CPU policy, two I/O policies.
    apps::Simulator::Options fcfsOptions;
    fcfsOptions.ioPolicy = io::Policy::FCFS;
    fcfsOptions.verbose = false;
    apps::Simulator::Options priorityOptions;
    priorityOptions.ioPolicy = io::Policy::PRIORITY;
    priorityOptions.verbose = false;
    std::ostringstream sink;
    apps::Simulator fcfsRun(apps::catalog(), fcfsOptions, sink);
    apps::Simulator priorityRun(apps::catalog(), priorityOptions, sink);
    fcfsRun.run();
    priorityRun.run();
    assert(fcfsRun.requests().size() == priorityRun.requests().size());
    assert(!fcfsRun.serviceOrder().empty() && !priorityRun.serviceOrder().empty());
    assert(fcfsRun.serviceOrder() != priorityRun.serviceOrder());
    for (const io::Request& request : fcfsRun.requests())
        assert(request.status == io::Status::COMPLETED);
    for (const io::Request& request : priorityRun.requests())
        assert(request.status == io::Status::COMPLETED);

    // The priority-1 editor write must not sit behind background priorities.
    int fcfsEditor = -1, priorityEditor = -1;
    for (const io::Request& request : fcfsRun.requests())
        if (request.application == "text-editor" && request.operation == io::Operation::WRITE)
            fcfsEditor = request.wait();
    for (const io::Request& request : priorityRun.requests())
        if (request.application == "text-editor" && request.operation == io::Operation::WRITE)
            priorityEditor = request.wait();
    assert(fcfsEditor >= 0 && priorityEditor >= 0);
    assert(priorityEditor < fcfsEditor);
}

void testDeadlockAndAllocation() {
    const deadlock::State state = deadlock::sample();
    const deadlock::Detection detected = deadlock::detect(state, false);
    assert((detected.deadlocked == std::vector<int>{0, 1}));
    std::ostringstream trace;
    const deadlock::State recovered =
        deadlock::recover(state, deadlock::VictimPolicy::LEAST_ALLOCATED, trace);
    assert(deadlock::detect(recovered, false).deadlocked.empty());
    assert(recovered.available[0] == 1 && recovered.available[1] == 0); // P0's resource returned
    assert(trace.str().find("terminate") != std::string::npos);

    // The wait-for graph cycle and acyclic cases.
    assert(deadlock::hasCycle({{1}, {2}, {0}}, trace));
    assert(!deadlock::hasCycle({{1}, {2}, {}}, trace));

    memory::ContiguousAllocator allocator(100);
    assert(allocator.allocate(1, 10, memory::AllocationPolicy::FIRST_FIT));
    assert(allocator.allocate(2, 20, memory::AllocationPolicy::BEST_FIT));
    assert(allocator.usedSpace() == 30);
    assert(allocator.allocationCount() == 2);
    assert(allocator.utilization() > 29.9 && allocator.utilization() < 30.1);
    assert(allocator.release(1));
    assert(allocator.freeSpace() == 80);
    assert(allocator.largestHole() == 70);
    assert(allocator.externalFragmentation() == 10);
    assert(!allocator.release(99));
    assert(!allocator.allocate(3, 71, memory::AllocationPolicy::FIRST_FIT));
    assert(allocator.allocate(3, 70, memory::AllocationPolicy::WORST_FIT));
    assert(allocator.freeSpace() == 10);
    bool duplicateRejected = false;
    try { allocator.allocate(3, 1, memory::AllocationPolicy::FIRST_FIT); }
    catch (const std::invalid_argument&) { duplicateRejected = true; }
    assert(duplicateRejected);

    // Same request sequence under all three policies: placement must differ.
    const auto runPolicy = [](memory::AllocationPolicy policy) {
        memory::ContiguousAllocator map(100);
        map.allocate(1, 10, policy);
        map.allocate(2, 25, policy);
        map.allocate(3, 5, policy);
        map.allocate(4, 30, policy);
        map.allocate(5, 5, policy);
        map.allocate(6, 20, policy);
        map.allocate(7, 5, policy);
        map.release(2);
        map.release(4);
        map.release(6);
        const bool first = map.allocate(8, 18, policy);
        const bool second = map.allocate(9, 12, policy);
        return std::make_pair(map, first && second);
    };
    const auto firstFit = runPolicy(memory::AllocationPolicy::FIRST_FIT);
    const auto bestFit = runPolicy(memory::AllocationPolicy::BEST_FIT);
    const auto worstFit = runPolicy(memory::AllocationPolicy::WORST_FIT);
    assert(firstFit.second && bestFit.second && worstFit.second);
    const auto startOf = [](const memory::ContiguousAllocator& map, int pid) {
        for (const memory::Region& region : map.map())
            if (region.pid == pid) return region.start;
        return -1;
    };
    assert(startOf(firstFit.first, 8) == 10);      // first hole that fits (25)
    assert(startOf(bestFit.first, 8) == 75);       // tightest hole (20)
    assert(startOf(worstFit.first, 8) == 40);      // largest hole (30)
    assert(startOf(firstFit.first, 9) == 40);
    assert(startOf(bestFit.first, 9) == 10);
    assert(startOf(worstFit.first, 9) == 10);
    assert(firstFit.first.freeSpace() == 45);
    assert(bestFit.first.freeSpace() == 45);
    assert(worstFit.first.freeSpace() == 45);
    assert(firstFit.first.largestHole() == 20);
    assert(bestFit.first.largestHole() == 30);
    assert(worstFit.first.largestHole() == 20);
    assert(firstFit.first.externalFragmentation() == 25);
    assert(bestFit.first.externalFragmentation() == 15);
    assert(worstFit.first.externalFragmentation() == 25);
}

void testScenariosAndInteractive() {
    std::ostringstream out;
    apps::priorityScenario(out);
    assert(out.str().find("Scenario 1") != std::string::npos);
    assert(out.str().find("preempts") != std::string::npos);
    assert(out.str().find("P4 (priority 1)") != std::string::npos);

    out.str(""); out.clear();
    apps::ioPriorityComparison(out);
    assert(out.str().find("Editor write wait") != std::string::npos);
    assert(out.str().find("Request service order") == std::string::npos);
    assert(out.str().find("FCFS service order") != std::string::npos);

    out.str(""); out.clear();
    apps::fileDependentScenario(out);
    assert(out.str().find("Search output: searched /docs/system.log for ERROR: 2 match") != std::string::npos);

    out.str(""); out.clear();
    apps::filesystemScenario(out);
    assert(out.str().find("Create directory /apps/work: ok") != std::string::npos);
    assert(out.str().find("REJECTED") != std::string::npos);
    assert(out.str().find("Used blocks after the rejected write") != std::string::npos);

    std::istringstream input("1\n9\ncustom-test\n3\n2\n1\n10\n\n\n2\n96\n");
    std::ostringstream output;
    apps::interactiveLaunch(input, output);
    assert(output.str().find("custom-test") != std::string::npos);
    assert(output.str().find("Interactive run complete") != std::string::npos);
}

void testInvalidInputsAndTransitions() {
    // Illegal PCB transitions are rejected.
    bool threw = false;
    PCB readyProcess{1, "x", 0, 1, 1, 1, State::READY};
    try { setState(readyProcess, State::WAITING, 0, false); }
    catch (const std::logic_error&) { threw = true; }
    assert(threw);
    threw = false;
    PCB deadProcess{1, "x", 0, 1, 1, 1, State::TERMINATED};
    try { setState(deadProcess, State::RUNNING, 0, false); }
    catch (const std::logic_error&) { threw = true; }
    assert(threw);
    assert(canTransition(State::NEW, State::READY));
    assert(canTransition(State::RUNNING, State::WAITING));
    assert(!canTransition(State::READY, State::WAITING));

    processsim::ProcessTable table;
    std::ostringstream sink;
    const int pid = table.create("p", 2, 1, 0, sink);
    threw = false;
    try { table.blockForIO(pid, 1, "disk", sink); }           // READY cannot issue I/O
    catch (const std::logic_error&) { threw = true; }
    assert(threw);
    table.dispatch(pid, 1, sink);
    table.terminate(pid, 2, sink);
    threw = false;
    try { table.terminate(pid, 3, sink); }
    catch (const std::logic_error&) { threw = true; }
    assert(threw);

    // Scheduler validation.
    const std::vector<PCB> empty;
    const cpu::Result idle = cpu::run(empty, cpu::Algorithm::FCFS, 3, false, sink);
    assert(idle.elapsed == 0 && idle.contextSwitches == 0 && idle.ticks.empty());
    const auto expectThrow = [&](const std::vector<PCB>& processes, int quantum) {
        try { cpu::run(processes, cpu::Algorithm::RR, quantum, false, sink); return false; }
        catch (const std::invalid_argument&) { return true; }
    };
    assert(expectThrow({{1, "a", 0, 1, 1, 1}, {1, "b", 0, 1, 1, 1}}, 2));   // duplicate PID
    assert(expectThrow({{1, "a", 0, 0, 0, 1}}, 2));                         // zero burst
    assert(expectThrow({{1, "a", -1, 1, 1, 1}}, 2));                        // negative arrival
    assert(expectThrow({{1, "a", 0, 1, 1, -1}}, 2));                        // negative priority
    assert(expectThrow({{1, "a", 0, 1, 1, 1}}, 0));                         // zero quantum

    assert(memory::run({}, 3, memory::Algorithm::FIFO, false, sink).faults == 0);
    bool framesThrew = false;
    try { memory::run({1}, 0, memory::Algorithm::FIFO, false, sink); }
    catch (const std::invalid_argument&) { framesThrew = true; }
    assert(framesThrew);
    bool pagingThrew = false;
    try { memory::run({-1}, 2, memory::Algorithm::LRU, false, sink); }
    catch (const std::invalid_argument&) { pagingThrew = true; }
    assert(pagingThrew);

    bool diskThrew = false;
    try { disk::schedule({200}, 53, 200, "FCFS", false, sink); }
    catch (const std::invalid_argument&) { diskThrew = true; }
    assert(diskThrew);

    bool simulatorThrew = false;
    try {
        apps::Simulator bad(apps::catalog(), apps::Simulator::Options(), sink);
        std::vector<apps::Program> duplicate;
        duplicate.push_back(apps::makeProgram(apps::Kind::COUNTER, 1));
        duplicate.push_back(apps::makeProgram(apps::Kind::CALCULATOR, 1));
        apps::Simulator rejected(duplicate, apps::Simulator::Options(), sink);
    } catch (const std::invalid_argument&) { simulatorThrew = true; }
    assert(simulatorThrew);

    bool bankerThrew = false;
    try { banker::Banker bad({1, 2}, {{1}}, {}); }
    catch (const std::invalid_argument&) { bankerThrew = true; }
    assert(bankerThrew);
}
} // namespace

int main() {
    testSchedulerAndPaging();
    testBankerAndDisk();
    testMemoryAndDeadlock();
    testSyncIpcAndFileSystem();
    testIntegratedAndProcesses();
    testFileSystemOperations();
    testApplicationsAndKernel();
    testPriorityDecisions();
    testIoPolicies();
    testDeadlockAndAllocation();
    testScenariosAndInteractive();
    testInvalidInputsAndTransitions();
    std::cout << "All Mini Kernel Simulator tests passed.\n";
}
