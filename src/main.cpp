#include "banker.h"
#include "disk.h"
#include "memory.h"
#include "pcb.h"
#include "scheduler.h"
#include "sync.h"

#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Parse one whole line as one integer; extra characters count as bad input.
int parseInteger(const std::string& line, int minimum, int maximum) {
    std::istringstream in(line);
    int value;
    std::string extra;
    if (!(in >> value) || (in >> extra) || value < minimum || value > maximum)
        throw std::invalid_argument("Enter a whole number in the stated range");
    return value;
}

// Keep prompting after nonnumeric input; Enter selects a documented default.
int askInteger(const std::string& prompt, int minimum, int maximum,
               std::optional<int> defaultValue = std::nullopt) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) throw std::runtime_error("Input ended");
        if (line.empty() && defaultValue) return *defaultValue;
        try { return parseInteger(line, minimum, maximum); }
        catch (const std::invalid_argument& error) { std::cout << error.what() << '\n'; }
    }
}

// Read exactly the requested count of nonnegative page references.
std::vector<int> askReferences() {
    int count = askInteger("Number of references (1..100): ", 1, 100);
    while (true) {
        std::cout << "Enter " << count << " nonnegative page numbers, space separated: ";
        std::string line; if (!std::getline(std::cin, line)) throw std::runtime_error("Input ended");
        std::istringstream in(line);
        std::vector<int> values;
        int page; while (in >> page) values.push_back(page);
        std::string extra;
        if (values.size() == static_cast<std::size_t>(count) && !(in.clear(), in >> extra)) {
            bool valid = true; for (int x : values) if (x < 0) valid = false;
            if (valid) return values;
        }
        std::cout << "Invalid reference list. Try again.\n";
    }
}

// Accept one process per line and replace the active process set only when all are valid.
std::vector<PCB> askProcesses() {
    int count = askInteger("Number of processes (1..20): ", 1, 20);
    std::vector<PCB> processes;
    for (int i = 0; i < count;) {
        std::cout << "Process " << i + 1 << " (PID name arrival burst priority): ";
        std::string line; if (!std::getline(std::cin, line)) throw std::runtime_error("Input ended");
        std::istringstream in(line);
        int pid, arrival, burst, priority; std::string name, extra;
        if ((in >> pid >> name >> arrival >> burst >> priority) && !(in >> extra) &&
            pid > 0 && arrival >= 0 && burst > 0 && priority >= 0) {
            bool duplicate = false;
            for (const PCB& p : processes) if (p.pid == pid) duplicate = true;
            if (!duplicate) {
                processes.push_back({pid, name, arrival, burst, burst, priority});
                ++i; continue;
            }
        }
        std::cout << "Invalid process or duplicate PID. Try again.\n";
    }
    return processes;
}

// Present numbered actions; sample data is already loaded when the program starts.
void menu() {
    std::cout << "\n=== MINI KERNEL SIMULATOR ===\n"
              << " 1. Enter processes manually\n 2. Load sample processes\n"
              << " 3. Run scheduler (trace + Gantt)\n 4. Compare schedulers\n"
              << " 5. Run page replacement\n 6. Compare page replacement\n"
              << " 7. Set reference string and frames\n 8. Banker's safety check\n"
              << " 9. Banker's request demo\n10. Producer-consumer demo\n"
              << "11. Dining philosophers demo\n12. Disk scheduling sample paths\n"
              << "13. Compare disk schedulers\n14. Exit\nChoice: ";
}

// Dispatch one menu selection and catch bad input without ending the simulator.
int main() {
    std::vector<PCB> processes = sampleProcesses();
    std::vector<int> references = memory::sampleReferences();
    int frames = 3;
    while (true) {
        menu();
        std::string line;
        if (!std::getline(std::cin, line)) break;
        int choice;
        try { choice = parseInteger(line, 1, 14); }
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
                    int a = askInteger("Algorithm 1=FCFS 2=SJF 3=SRTF 4=Priority 5=RR [1]: ", 1, 5, 1);
                    int quantum = a == 5 ? askInteger("RR quantum [3]: ", 1, 1000, 3) : 3;
                    cpu::run(processes, static_cast<cpu::Algorithm>(a - 1), quantum);
                    break;
                }
                case 4:
                    cpu::compare(processes, 3);
                    break;
                case 5: {
                    int a = askInteger("Algorithm 1=FIFO 2=LRU 3=Optimal [1]: ", 1, 3, 1);
                    memory::run(references, frames, static_cast<memory::Algorithm>(a - 1));
                    break;
                }
                case 6:
                    memory::compare(references, frames);
                    break;
                case 7:
                    references = askReferences();
                    frames = askInteger("Frame count (1..20): ", 1, 20);
                    std::cout << "Reference string and frames updated.\n";
                    break;
                case 8:
                    banker::sample().safety();
                    break;
                case 9:
                    banker::requestDemo();
                    break;
                case 10:
                    syncsim::producerConsumerDemo();
                    break;
                case 11:
                    syncsim::diningDemo();
                    break;
                case 12: {
                    std::vector<int> requests = disk::sampleRequests();
                    std::cout << "Requests: 98 183 37 122 14 124 65 67; head=53; disk size=200\n";
                    for (const std::string& a : {"FCFS", "SSTF", "SCAN", "C-SCAN"})
                        disk::schedule(requests, 53, 200, a);
                    break;
                }
                case 13:
                    disk::compare(disk::sampleRequests(), 53, 200);
                    break;
                case 14:
                    return 0;
            }
        } catch (const std::exception& error) {
            std::cout << "Error: " << error.what() << '\n';
            if (!std::cin) break;
        }
    }
    return 0;
}
