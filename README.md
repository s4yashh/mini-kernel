# Mini Kernel Simulator

A C++17, menu-driven educational simulator that makes common operating-system decisions visible through deterministic logs, tables, and text charts. It does not modify the host OS or control real hardware.

## Implementation status

The complete planned simulator build is implemented in the console application. It combines the original process/scheduling/paging/Banker modules with process lifecycle, advanced memory demonstrations, MLFQ, synchronization, deadlock detection and recovery, IPC, a simulated file system, disk scheduling, and prioritized applications. Automated regression tests verify the published textbook checkpoints.

“Complete” refers to this project's defined simulator scope, not every BCSE303L syllabus topic. Virtual machines/hypervisors, distributed file systems, real-time synchronization, real threads/devices, persistent storage, and a production security model are not emulated.

## Features

| Area | Implemented demonstrations |
| --- | --- |
| Processes | PCB, NEW/READY/RUNNING/WAITING/TERMINATED states, logged transitions, create/I/O-wait/wake/terminate, system-call boundary illustration |
| CPU scheduling | FCFS, non-preemptive SJF, preemptive SRTF, preemptive Priority, Round Robin, static Multilevel Queue (MLQ), and three-level Multilevel Feedback Queue (MLFQ); Gantt trace, per-process metrics, averages, utilization, dispatches |
| Main/virtual memory | First Fit/Best Fit/Worst Fit with fragmentation, segmentation bounds and address translation, LRU TLB, working-set and thrashing-pressure trace |
| Demand paging | FIFO, LRU, Optimal; per-reference frames, hits, faults, eviction, rate, and comparisons |
| Deadlocks | Banker's safety/request algorithms; wait-for graph cycle detection; multiple-instance detection; recovery by terminating a victim and releasing resources |
| Concurrency and IPC | Simulated semaphores/mutex, bounded-buffer producer/consumer, dining philosophers deadlock/prevention, Peterson and Bakery traces, message queues and permission-checked shared memory |
| File system | In-memory directories and files, create/read/write/delete/list, sequential/direct access, contiguous/linked/indexed allocation, owner-based access checks, block map, transaction journal and recovery demonstration |
| Disk | FCFS, SSTF, SCAN, C-SCAN, custom requests, service order, path, total movement and average seek |
| Applications/integration | A discrete-event run overlaps priority-scheduled CPU ticks with FCFS disk ticks; calculator, text editor and counter processes request Banker-managed resources, allocate RAM, enter WAITING/READY around file I/O, and release resources/memory on exit |

## Build and run

### GCC / MinGW

Linux or macOS:

```sh
g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp -o mini_kernel
./mini_kernel
```

Windows PowerShell with MinGW:

```powershell
g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp -o mini_kernel.exe
.\mini_kernel.exe
```

### CMake

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The source has no external C++ dependencies. The direct GCC command above was verified with `-Wall -Wextra -Wpedantic`; CMake could not be run in the development environment because CMake is not installed there.

Run the regression suite without CMake:

```sh
g++ -std=c++17 -Wall -Wextra -Iinclude tests/simulator_tests.cpp -o simulator_tests
./simulator_tests
```

## Menu guide

1–5 process entry, classic schedulers, comparison, and MLFQ; 6–9 paging and advanced memory; 10–11 Banker's algorithm; 12–14 semaphore and mutual-exclusion demonstrations; 15–17 sample/custom disk scheduling; 18–21 process lifecycle, thread models, deadlock, and IPC; 22–23 interactive and fixed file-system demos; 24 integrated priority-app run; 25 MLQ; 26 exit.

All demonstrations work with built-in sample data. The file-system menu keeps data in memory for the current program run. Use `admin` only in the API/demo for privileged reads; interactive files are owned by `user`.

## Verified sample checkpoints

- CPU comparison (average waiting / turnaround / response / dispatches; Round Robin quantum 3): FCFS `11.00/16.60/11.00/5`; SJF `7.80/13.40/7.80/5`; SRTF `6.40/12.00/3.80/7`; Priority `9.20/14.80/7.40/6`; RR `14.20/19.80/4.20/11`.
- Page faults with the built-in string and 3 frames: FIFO `15`, LRU `12`, Optimal `9`.
- Banker's safe sequence: `P1 -> P3 -> P4 -> P0 -> P2`; request demo: P1 granted, P4 waits, P0 denied as unsafe.
- Disk sample (head 53, disk size 200): FCFS `640`, SSTF `236`, SCAN `236`, C-SCAN `382` cylinders. SCAN starts toward 0; C-SCAN counts the return jump.
- Integrated app run: calculator priority 1, text editor priority 2, counter priority 3; all execute, write simulated files, and produce a disk-I/O path.

## Browser visualizer

Open `web/index.html` directly in a modern browser. The website has interactive tabs for CPU scheduling (including MLQ/MLFQ), paging, Banker’s algorithm, synchronization, disk scheduling, allocation/segmentation/TLB, process and thread states, deadlock recovery/IPC, and file operations. Its integrated-app demo makes priority visible and renders one tick-by-tick CPU lane alongside disk I/O, process state logs, RAM allocations, resource requests, and generated files. The browser models are deterministic JavaScript demonstrations and do not call the C++ executable or write to the host file system.

### Suggested website demo order

1. **CPU scheduling:** change the algorithm and scrub/play the Gantt chart; point out priority values.
2. **Memory paging / manager:** compare FIFO/LRU/Optimal, then show fit allocation, fragmentation, segmentation bounds, and TLB hits/misses.
3. **Banker / deadlock & IPC:** make a safe/unsafe request, then show cycle detection/recovery and queue/shared-memory behavior.
4. **Synchronization / disk:** run producer-consumer or dining philosophers, then compare disk paths.
5. **File system:** write and read a sample file using contiguous, linked, or indexed allocation.
6. **Integrated run:** show calculator priority 1, editor priority 2, counter priority 3; run the demo and explain CPU/disk overlap, WAITING→READY, resource release, RAM, and files.

## Source layout

```text
include/       Header-only simulator modules
src/main.cpp   Validated interactive menu and demos
tests/         Dependency-free C++ regression tests
web/           Single-file browser visualizer
docs/          Course-topic mapping and source syllabus PDF
```

Run `ctest --test-dir build --output-on-failure` after configuring with CMake, or compile `tests/simulator_tests.cpp` directly with the same C++17 warning flags.
