#include "apps.h"
#include "banker.h"
#include "deadlock.h"
#include "disk.h"
#include "fs.h"
#include "ipc.h"
#include "memory.h"
#include "pcb.h"
#include "process_manager.h"
#include "scheduler.h"
#include "sync.h"
#include "threading.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

int parseInteger(const std::string& line, int minimum, int maximum) {
    std::istringstream in(line);
    int value = 0;
    std::string extra;
    if (!(in >> value) || (in >> extra) || value < minimum || value > maximum)
        throw std::invalid_argument("Enter a whole number in the stated range");
    return value;
}

int askInteger(const std::string& prompt, int minimum, int maximum,
               int defaultValue = -1) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) throw std::runtime_error("Input ended");
        if (line.empty() && defaultValue >= minimum) return defaultValue;
        try { return parseInteger(line, minimum, maximum); }
        catch (const std::invalid_argument& error) { std::cout << error.what() << '\n'; }
    }
}

std::string askLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) throw std::runtime_error("Input ended");
    return line;
}

std::vector<int> askIntegerList(const std::string& prompt, int count, int minimum) {
    while (true) {
        const std::string line = askLine(prompt);
        std::istringstream input(line);
        std::vector<int> values;
        int value = 0;
        while (input >> value) values.push_back(value);
        bool valid = input.eof() && values.size() == static_cast<std::size_t>(count);
        for (int item : values) if (item < minimum) valid = false;
        if (valid) return values;
        std::cout << "Invalid list; enter exactly " << count << " integers.\n";
    }
}

std::vector<int> askReferences() {
    const int count = askInteger("Number of references (1..100): ", 1, 100);
    return askIntegerList("Enter nonnegative page numbers, space separated: ", count, 0);
}

std::vector<PCB> askProcesses() {
    const int count = askInteger("Number of processes (1..20): ", 1, 20);
    std::vector<PCB> processes;
    for (int i = 0; i < count;) {
        const std::string line = askLine("Process (PID name arrival burst priority): ");
        std::istringstream in(line);
        int pid = 0, arrival = 0, burst = 0, priority = 0;
        std::string name, extra;
        if ((in >> pid >> name >> arrival >> burst >> priority) && !(in >> extra) &&
            pid > 0 && arrival >= 0 && burst > 0 && priority >= 0) {
            bool duplicate = false;
            for (const PCB& process : processes) if (process.pid == pid) duplicate = true;
            if (!duplicate) {
                processes.push_back({pid, name, arrival, burst, burst, priority});
                ++i;
                continue;
            }
        }
        std::cout << "Invalid process or duplicate PID. Try again.\n";
    }
    return processes;
}

void processLifecycleDemo() {
    std::cout << "\nProcess creation, I/O wait, wake-up and termination\n";
    processsim::ProcessTable table;
    const int editor = table.create("editor", 5, 1, 0);
    const int compiler = table.create("compiler", 8, 2, 0);
    table.blockForIO(editor, 2, "disk");
    table.completeIO(editor, 5);
    table.terminate(compiler, 7);
    table.terminate(editor, 9);
    table.print();
    processsim::systemCallDemo();
}

void fileSystemMenu(filesystem::FileSystem& fs) {
    while (true) {
        std::cout << "\n=== File-system tools (in-memory, no real files changed) ===\n"
                  << "1. Create directory\n2. Create/write file\n3. Read file\n"
                  << "4. Delete file\n5. List directory\n6. Show blocks and journal\n"
                  << "7. Recover journal\n8. Back\n";
        const int choice = askInteger("Choice: ", 1, 8);
        if (choice == 8) return;
        try {
            if (choice == 1) {
                const std::string path = askLine("Absolute directory path (example /docs): ");
                std::cout << (fs.createDirectory(path) ? "Directory created.\n" : "Could not create directory; check its parent/path.\n");
            } else if (choice == 2) {
                const std::string path = askLine("Absolute file path (example /docs/notes.txt): ");
                const std::string contents = askLine("File contents (one line): ");
                const int method = askInteger("Allocation 1=contiguous 2=linked 3=indexed: ", 1, 3);
                const auto allocation = static_cast<filesystem::Allocation>(method - 1);
                std::cout << (fs.writeFile(path, contents, allocation)
                    ? "File written.\n" : "Write failed (invalid path/parent or insufficient blocks).\n");
            } else if (choice == 3) {
                const std::string path = askLine("Absolute file path: ");
                const int mode = askInteger("Access 1=sequential 2=direct: ", 1, 2);
                if (mode == 1) std::cout << fs.readFile(path) << '\n';
                else {
                    const int offset = askInteger("Byte offset: ", 0, 1000000);
                    const int length = askInteger("Number of bytes: ", 0, 1000000);
                    std::cout << fs.readFile(path, "user", filesystem::Access::DIRECT,
                        static_cast<std::size_t>(offset), static_cast<std::size_t>(length)) << '\n';
                }
                const filesystem::FileEntry& file = fs.entry(path);
                std::cout << "Allocated blocks: ";
                for (int block : file.blocks) std::cout << block << ' ';
                std::cout << '\n';
            } else if (choice == 4) {
                const std::string path = askLine("Absolute file path: ");
                std::cout << (fs.deleteFile(path) ? "File deleted.\n" : "File not found.\n");
            } else if (choice == 5) {
                const std::string path = askLine("Directory path [/]: ");
                fs.list(path.empty() ? "/" : path);
            } else if (choice == 6) {
                fs.printBlockMap();
                fs.printJournal();
            } else {
                fs.recover();
            }
        } catch (const std::exception& error) {
            std::cout << "File-system error: " << error.what() << '\n';
        }
    }
}

void menu() {
    std::cout << "\n========== MINI KERNEL SIMULATOR =========="
              << "\n 1. Enter processes manually"
              << "\n 2. Load sample processes"
              << "\n 3. Run FCFS/SJF/SRTF/Priority/Round Robin"
              << "\n 4. Compare the five classic schedulers"
              << "\n 5. Run Multilevel Feedback Queue (MLFQ)"
              << "\n 6. Run page replacement"
              << "\n 7. Compare FIFO/LRU/Optimal paging"
              << "\n 8. Set reference string and frame count"
              << "\n 9. Memory allocation, segmentation, TLB and working set"
              << "\n10. Banker's safety check"
              << "\n11. Banker's resource-request demo"
              << "\n12. Producer-consumer semaphore demo"
              << "\n13. Dining philosophers: deadlock and prevention"
              << "\n14. Peterson and Bakery mutual-exclusion demos"
              << "\n15. Disk scheduling sample paths"
              << "\n16. Disk scheduling with custom requests"
              << "\n17. Compare disk schedulers"
              << "\n18. Process creation, I/O wait, system-call demo"
              << "\n19. User/kernel thread models"
              << "\n20. Deadlock detection and recovery"
              << "\n21. Inter-process communication demo"
              << "\n22. File-system operations (create/read/write/delete/list)"
              << "\n23. File-system allocation and protection demo"
              << "\n24. Launch prioritized apps (integrated kernel run)"
              << "\n25. Run static Multilevel Queue (MLQ)"
              << "\n26. Exit\nChoice: ";
}

int main() {
    std::vector<PCB> processes = sampleProcesses();
    std::vector<int> references = memory::sampleReferences();
    int frames = 3;
    filesystem::FileSystem fs(64, 16);
    fs.createDirectory("/apps");
    fs.createDirectory("/docs");

    while (true) {
        menu();
        std::string line;
        if (!std::getline(std::cin, line)) break;
        int choice = 0;
        try { choice = parseInteger(line, 1, 26); }
        catch (const std::invalid_argument&) { std::cout << "Invalid menu choice.\n"; continue; }
        try {
            switch (choice) {
                case 1:
                    processes = askProcesses();
                    std::cout << "Loaded " << processes.size() << " processes.\n";
                    break;
                case 2:
                    processes = sampleProcesses();
                    std::cout << "Loaded five sample processes.\n";
                    break;
                case 3: {
                    const int algorithm = askInteger("1=FCFS 2=SJF 3=SRTF 4=Priority 5=RR: ", 1, 5);
                    const int quantum = algorithm == 5 ? askInteger("RR quantum [3]: ", 1, 1000, 3) : 3;
                    cpu::run(processes, static_cast<cpu::Algorithm>(algorithm - 1), quantum);
                    break;
                }
                case 4:
                    cpu::compare(processes, 3);
                    break;
                case 5:
                    cpu::runMLFQ(processes);
                    break;
                case 6: {
                    const int algorithm = askInteger("1=FIFO 2=LRU 3=Optimal: ", 1, 3);
                    memory::run(references, frames, static_cast<memory::Algorithm>(algorithm - 1));
                    break;
                }
                case 7:
                    memory::compare(references, frames);
                    break;
                case 8:
                    references = askReferences();
                    frames = askInteger("Frame count (1..20): ", 1, 20);
                    std::cout << "Reference string and frame count updated.\n";
                    break;
                case 9:
                    memory::advancedDemo();
                    break;
                case 10:
                    banker::sample().safety();
                    break;
                case 11:
                    banker::requestDemo();
                    break;
                case 12:
                    syncsim::producerConsumerDemo();
                    break;
                case 13:
                    syncsim::diningDemo();
                    break;
                case 14:
                    syncsim::petersonDemo();
                    syncsim::bakeryDemo();
                    break;
                case 15: {
                    const std::vector<int> requests = disk::sampleRequests();
                    std::cout << "Requests: 98 183 37 122 14 124 65 67; head=53; disk size=200\n";
                    for (const std::string& algorithm : {"FCFS", "SSTF", "SCAN", "C-SCAN"})
                        disk::schedule(requests, 53, 200, algorithm);
                    break;
                }
                case 16: {
                    const int count = askInteger("Request count (1..100): ", 1, 100);
                    const std::vector<int> requests = askIntegerList("Enter request cylinders: ", count, 0);
                    const int head = askInteger("Initial head position: ", 0, 1000000);
                    const int size = askInteger("Disk size (cylinders): ", 1, 1000001);
                    for (const std::string& algorithm : {"FCFS", "SSTF", "SCAN", "C-SCAN"})
                        disk::schedule(requests, head, size, algorithm);
                    break;
                }
                case 17:
                    disk::compare(disk::sampleRequests(), 53, 200);
                    break;
                case 18:
                    processLifecycleDemo();
                    break;
                case 19:
                    threadsim::modelDemo();
                    break;
                case 20:
                    deadlock::demo();
                    break;
                case 21:
                    ipc::demo();
                    break;
                case 22:
                    fileSystemMenu(fs);
                    break;
                case 23:
                    filesystem::demo();
                    break;
                case 24:
                    apps::integratedDemo();
                    break;
                case 25:
                    cpu::runMLQ(processes);
                    break;
                case 26:
                    return 0;
            }
        } catch (const std::exception& error) {
            std::cout << "Error: " << error.what() << '\n';
            if (!std::cin) break;
        }
    }
    return 0;
}
