# Mini Kernel Simulator

## Problem statement

Kernel process, CPU, memory, and resource decisions are hard to see in a real OS. This C++17 console program makes each decision visible through deterministic logs, tables, and text charts. It models textbook algorithms and does not control actual hardware.

## Review 1 scope

Implemented: PCB states and transitions; five CPU schedulers; FIFO, LRU, and Optimal page replacement; Banker's safety and resource requests; simulated semaphores, bounded-buffer producer-consumer, and dining philosophers; four disk schedulers. This is approximately 50% of the intended educational simulator. Future work can add I/O queues, process creation and termination controls, memory allocation, deadlock detection/recovery, and persistent experiment reports. Real threads, real devices, and a real kernel are outside this simulator's scope.

## Files

| File | Purpose |
| --- | --- |
| `include/pcb.h` | PCB and logged state transitions |
| `include/scheduler.h` | Tick-based CPU policies, Gantt and metrics |
| `include/memory.h` | Demand paging and replacement traces |
| `include/banker.h` | Safety and provisional resource requests |
| `include/sync.h` | Simulated semaphores and synchronization demos |
| `include/disk.h` | Disk head paths and seek comparison |
| `src/main.cpp` | Menu, sample data and input validation |
| `web/index.html` | Browser demo for CPU scheduling, paging and Banker; synchronization and disk are heading-only tabs |
| `docs/Module_Topics_Used.docx` | Concise faculty table of BCSE303L module numbers and project topics |

## Build and run

Linux/macOS with GCC:

```sh
g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp -o mini_kernel
./mini_kernel
```

Windows PowerShell with MinGW GCC:

```powershell
g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp -o mini_kernel.exe
.\mini_kernel.exe
```

CMake on either platform:

```sh
cmake -S . -B build
cmake --build build
```

Run `build/mini_kernel` on Linux/macOS or `build\Debug\mini_kernel.exe` (multi-config generators) / `build\mini_kernel.exe` (MinGW Makefiles) on Windows.

## Web interface

Open `web/index.html` in any modern browser. It is one HTML file with embedded CSS and JavaScript; no package installation or server is needed. CPU scheduling, paging and Banker's algorithm have working browser demos. Synchronization and disk scheduling appear only as headings for the next web phase. The browser does not execute the C++ binary.

## Sample checkpoints

- Menu 4, scheduler comparison with RR quantum 3: FCFS `11.00/16.60/11.00/5`, SJF `7.80/13.40/7.80/5`, SRTF `6.40/12.00/3.80/7`, Priority `9.20/14.80/7.40/6`, RR `14.20/19.80/4.20/11`. Values are average wait, turnaround, response, and dispatch count. The first dispatch is counted as a context switch for this project.
- Menu 6, page comparison with three frames: FIFO 15, LRU 12, Optimal 9 faults.
- Menu 8: safe sequence `P1 -> P3 -> P4 -> P0 -> P2`.
- Menu 9: P1 granted, P4 waits, P0 denied as unsafe.
- Menu 10: C1 blocks on empty; P1 wakes C1; P2 blocks on full; C2 wakes P2.
- Menu 11: all five deadlock with left-then-right forks; all five eat with resource ordering.
- Menu 13: FCFS 640, SSTF 236, SCAN 236, C-SCAN 382 cylinders.

SCAN starts toward cylinder zero. C-SCAN starts toward cylinder 199 and counts the return from 199 to 0. Averages divide movement by eight requests. The sample data is loaded at startup, so the comparison and demonstration options can run immediately.
# mini-kernel
