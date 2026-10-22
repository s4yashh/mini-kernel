# Mini Kernel Simulator

A C++17, menu-driven educational simulator that makes common operating-system
decisions visible through deterministic logs, tables and text charts. It does
**not** modify the host OS, control real hardware, or write to the host file
system. A self-contained browser visualizer demonstrates the same concepts.

Course: **BCSE303L Operating Systems**. Team: Suyash Singh, Pratik Tekriwal,
Aditya Sharma.

## Review 2: applications, CPU priority and file-I/O priority

The primary new requirement was to show **multiple files and small applications
competing for the simulated kernel**, and to demonstrate that *CPU process
priority* and *file/I/O request priority* are separate concepts. Review 2 adds:

- Eight functional applications, each a real multi-step program (CPU steps plus
  file-system calls) executed by one kernel, not by its own private scheduler.
- Visible preemptive priority CPU scheduling with a plain-language explanation
  for every dispatch and preemption decision.
- A separate file/I/O request queue with **FCFS** and **priority** policies,
  request IDs, statuses, service duration, service order and a reason for every
  selection.
- A completed in-memory file system: create, read, write, append, rename, copy,
  move, delete, directories, metadata, block accounting and space exhaustion.
- Completion of the Review 1 planned work (lifecycle, deadlock detection and
  recovery, allocation verification, block accounting) without replacing working
  modules.
- Browser tabs for a kernel dashboard, application launch/stepping and I/O
  priorities, driven by a testable JavaScript simulation core.

## Feature-completion table (Review 1 planned work)

| Planned after Review 1 | Status | Implementation |
| --- | --- | --- |
| Dynamic process creation and termination | Complete | `processsim::ProcessTable`, `apps::Simulator` admission/termination |
| READY / RUNNING / WAITING / TERMINATED transitions | Complete, validated | `canTransition()` in `include/pcb.h`; illegal transitions throw |
| Simulated I/O blocks a process; completion wakes it | Complete | File requests are queued; the process enters WAITING and is woken on completion |
| Consistent process table, queues, transition log | Complete | Per-tick queue snapshots and a full transition-history table |
| Cleanup on termination (memory, resources) | Complete | RAM and Banker allocations released on exit |
| Deadlock detection for multiple resource types | Already present, kept | `deadlock::detect` (multi-instance) |
| Deadlock identification and recovery | Completed | `deadlock::recover` with a documented victim policy, before/after tables |
| Banker's safety and request algorithms | Already present, kept | `include/banker.h`, unchanged semantics |
| First / Best / Worst Fit verification | Complete | Identical-request comparison now produces different placements |
| Free-space, utilisation, fragmentation metrics | Added | `usedSpace`, `utilisation`, `largestHole`, `externalFragmentation` |
| File create/grow/copy/delete block accounting | Complete | `blocks` are allocated/released transactionally; failures leave data intact |
| Insufficient-storage handling | Complete | Writes fail cleanly; blocks are unchanged |
| Existing scheduling, paging, disk, sync, IPC modules | Preserved | Behaviour and published checkpoints unchanged |

## Feature summary

| Area | Implemented demonstrations |
| --- | --- |
| Processes | PCB, NEW/READY/RUNNING/WAITING/TERMINATED states, validated transitions, create/dispatch/I-O-wait/wake/terminate, system-call boundary |
| CPU scheduling | FCFS, non-preemptive SJF, preemptive SRTF, preemptive Priority, Round Robin, static MLQ, three-level MLFQ; per-decision explanation; Gantt, per-process metrics, utilisation, dispatches |
| Applications | Calculator, text editor, counter, file search, file analyzer, file copy, background indexer, system monitor — all executed by the kernel with real file access |
| File/I/O prioritisation | Request IDs, priorities, statuses, service duration/order; FCFS and priority selection with reasons; controlled side-by-side comparison |
| Main/virtual memory | First/Best/Worst Fit, fragmentation metrics, segmentation bounds and translation, LRU TLB, working-set/thrashing trace |
| Demand paging | FIFO, LRU, Optimal with per-reference frames, hits, faults, eviction and comparison |
| Deadlocks | Banker safety/request; wait-for graph cycle detection; multi-instance detection; recovery with selectable victim policy and before/after tables |
| Concurrency and IPC | Simulated semaphores/mutex, bounded-buffer producer/consumer, dining philosophers, Peterson and Bakery traces, message queues, protected shared memory |
| File system | In-memory directories/files, create/read/write/append/rename/copy/move/delete, metadata, sequential/direct access, contiguous/linked/indexed allocation, owner checks, block map, journal/recovery |
| Disk | FCFS, SSTF, SCAN, C-SCAN; service order, head path, total movement, average seek |
| Integration | Eight applications overlap preemptive priority CPU scheduling with the file-I/O queue, allocate RAM and Banker resources, block/resume on I/O and release everything on exit |

## Applications

All eight use the same kernel. "Burst" is the total CPU occupancy: CPU work plus
one tick per file-system call (the trap into the kernel). File requests carry
their own priority, independent of the process priority.

| PID (default) | Application | Work performed | Files | CPU pri. | RAM |
| --- | --- | --- | --- | --- | --- |
| 1 | Calculator | computes 14 + 28 | writes `/apps/calculator.txt` | 1 | 6 |
| 2 | Text editor | reads a note, appends a line, saves it | reads `/docs/notes.txt`, writes `/apps/notes.txt` | 2 | 10 |
| 3 | Counter | sums 1..1000 (CPU-heavy) | writes `/apps/counter.txt` | 3 | 8 |
| 4 | File search | counts `ERROR` occurrences | reads `/docs/system.log`, writes `/apps/search.txt` | 2 | 7 |
| 5 | File analyzer | counts lines/words/characters | reads `/docs/numbers.csv`, writes `/apps/analysis.txt` | 3 | 6 |
| 6 | File copy | copies a report through the block allocator | reads `/docs/report.txt`, writes `/apps/report-copy.txt` | 1 | 9 |
| 7 | Background indexer | reads five files, builds an index | reads five files under `/docs`, writes `/apps/index.txt` | 3 | 8 |
| 8 | System monitor | samples simulated process/file-system state | writes `/apps/monitor.txt` | 2 | 6 |

Results are derived from execution: the search reports the matches it actually
found, the analyzer measures the actual CSV, the copy's contents equal the
source, and the indexer's output reflects the files it read.

### Custom applications

Menu 26 (`Launch applications yourself`) lets you choose any catalogue
application or a custom one and configure its **priority (1–3), arrival time,
CPU burst, memory** and **I/O policy**. The web Applications tab exposes the same
controls, including an editable burst that scales the application's CPU steps.

## CPU priority scheduling

- **Priority 1 is the highest**, 3 is the lowest. Lower numbers run first.
- The application kernel uses **preemptive priority**: if a higher-priority
  process becomes READY, it preempts the running one on the next tick.
- **Tie-break is deterministic**: equal priority → earlier arrival time → lower
  PID. The same rule is stated in every explained decision.
- Every dispatch prints/logs a reason, for example:
  `P6 (file-copy, priority 1) selected over P2 (priority 2), P5 (priority 3):
  lowest priority number in READY (no tie at priority 1)`.
- Preemption is logged separately:
  `P4 (priority 1) preempts P1 (priority 3) because a higher-priority process
  became READY`.
- The classic schedulers (menu 3–5, 25) keep their own policies: FCFS, SJF,
  SRTF, Priority, Round Robin, MLQ and MLFQ. Their dispatch decisions are also
  explained, with the same tie-break.
- A continuous stream of priority-1 work could starve priority-3 work; the
  simulator states this limitation instead of hiding it.

## File/I/O request prioritisation

CPU scheduling chooses **which READY process runs**. I/O scheduling chooses
**which pending file/disk request is serviced next**. They are separate queues
with separate priorities.

Each request records: unique ID, originating PID and application, path,
operation (READ/WRITE/COPY/DELETE), request priority (1 highest), submission
tick, status (PENDING/SERVING/COMPLETED/FAILED), start/completion time, service
duration, service order, blocks touched and outcome.

- **FCFS** serves by submission time, then request ID.
- **Priority** serves the lowest priority number first, then submission time,
  then request ID.
- Service duration is simulated at one tick per 16-byte block.
- The console prints pending/completed tables, the service order and the reason
  for each selection; the web I/O tab shows the same information side by side.

Worked example (menu 28, fixed request set, identical inputs):

| Metric | FCFS | Priority |
| --- | --- | --- |
| Service order | #1,#2,#3,#4,#5,#6,#7 | #1,#6,#7,#2,#3,#4,#5 |
| Editor write wait | 16 ticks | 2 ticks |
| Average wait | 11.29 ticks | 11.86 ticks |

Priority reduces the important request's latency and can increase the average
wait of background requests — the trade-off is shown numerically. The full
application run is also compared with the same applications and seed files.

**Relationship to disk scheduling:** `disk.h` answers a different question — how
the head moves between cylinders (FCFS/SSTF/SCAN/C-SCAN). `ioqueue.h` decides
which queued request is chosen. Both are preserved and documented separately.

**Priority inversion:** a high-CPU-priority process can wait for a resource held
by a low-priority one. That is visible in the simulation but priority
inheritance is *not* implemented; no claim is made that it is.

## File system

The file system is entirely in memory (no host files are touched).

- Directories: `/`, `/docs`, `/apps` and any directories you create.
- Files: `notes.txt`, `report.txt`, `numbers.csv`, `source.c`, `config.json`,
  `system.log`, `style.txt`, plus application outputs.
- Operations: create directory (with parents), create/write, append, read
  (sequential or direct with offset), rename, copy, move, delete, list, metadata,
  block map, free-space summary and journal recovery.
- Allocation: contiguous, linked and indexed (one index block); block size 16
  bytes; the interactive file system has 64 blocks, the application kernel 128.
- Guarantees: a failed write (invalid path, directory collision, access denial,
  insufficient space) leaves existing data and the block map unchanged. Deleting
  a file returns every block. Copying allocates new blocks. Metadata shows owner,
  size, allocation method, block list, index block and write count.
- Protection: only the owner or `admin` may read/write/delete; `guest` is denied.
- The block map is printed/rendered after operations so allocation and release
  are visible.

Menu 30 (Scenario 4) walks through create → append → copy → delete → rename →
storage exhaustion with block counts before and after each step.

## Memory allocation

First Fit, Best Fit and Worst Fit use the same 100-unit memory and the same
request sequence: allocate 10, 25, 5, 30, 5, 20 and 5 units; release P2, P4 and
P6; then request 18 and 12 units. Three holes (25, 30 and 20 units) remain in
address order, so First Fit takes the first hole, Best Fit the tightest and Worst
Fit the largest. Every request succeeds, but P8 starts at 10 / 75 / 40
respectively and the external fragmentation is 25 / 15 / 25 units. Metrics:
used/free, largest hole, external fragmentation (free − largest hole),
utilisation, and per-policy memory maps. Allocation failures are handled without
corrupting state.

## Deadlock detection and recovery

Kept from Review 1 and extended:

- Banker's safety and request algorithms (avoidance).
- Multi-instance deadlock detection with allocation/request/available matrices.
- Wait-for graph cycle detection (three-colour DFS).
- Recovery by terminating one victim and releasing its resources, with two
  documented policies (lowest-numbered deadlocked process, or the one holding the
  fewest resources), before/after resource tables and a verification pass.

Avoidance and detection remain distinct: avoidance refuses unsafe requests;
detection finds an already deadlocked state and recovers from it.

## Build and run

### GCC / MinGW

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/main.cpp -o mini_kernel
./mini_kernel
```

Windows PowerShell:

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/main.cpp -o mini_kernel.exe
.\mini_kernel.exe
```

### CMake

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

CMake also registers the browser-core test when `node` is found.

### Tests

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude tests/simulator_tests.cpp -o simulator_tests
./simulator_tests

node tests/web/kernel_core_test.js
```

## Menu guide

| Menu | Action |
| --- | --- |
| 1–2 | Enter processes manually / load the five sample processes |
| 3–5 | Run a classic scheduler / compare five policies / run MLFQ |
| 6–9 | Page replacement / compare FIFO-LRU-Optimal / set references / advanced memory |
| 10–11 | Banker's safety / resource-request demo |
| 12–14 | Producer–consumer / dining philosophers / Peterson + Bakery |
| 15–17 | Disk sample / custom requests / comparison |
| 18–19 | Process lifecycle + system call / thread models |
| 20–21 | Deadlock detection and recovery / IPC |
| 22 | Interactive file system (create, write, append, read, delete, rename, copy, move, metadata, block map, recovery, navigation) |
| 23 | File-system allocation/protection demo |
| 24 | Integrated run of eight prioritized applications |
| 25 | Static Multilevel Queue |
| 26 | Launch applications yourself (priority, arrival, burst, RAM, I/O policy) |
| 27 | Scenario 1: CPU priority with late high-priority arrivals |
| 28 | Scenario 2: FCFS vs priority file-I/O ordering |
| 29 | Scenario 3: file-dependent application (I/O wait and wake-up) |
| 30 | Scenario 4: file system, copy and block allocation |
| 31 | Scenarios 5–7: memory allocation, deadlock, paging and disk comparisons |
| 32 | Run all seven guided scenarios in sequence |
| 33 | Exit |

All demonstrations work with built-in sample data. The interactive file system
keeps data for the current program run only.

## Guided demonstrations

1. **CPU priority (menu 27 / web “1 · CPU priority”)** — the priority-3 counter
   starts first; the priority-2 editor arrives at t=2 and preempts it; the
   priority-1 calculator arrives at t=5 and preempts again. 29 ticks in the
   default run.
2. **I/O priority (menu 28 / web “2 · I/O priority”)** — identical request sets
   under FCFS and priority; compare service order, editor wait and averages.
3. **File-dependent application (menu 29)** — file search and file analyzer
   block on READ, other work runs, they wake on completion and print real output.
4. **File system and blocks (menu 30 / web “4”)** — directory, create, append,
   copy, delete, rename, block map, free-space change and a rejected oversized
   write.
5. **Memory allocation (menu 31)** — First/Best/Worst Fit on the same requests.
6. **Deadlock (menu 31 or 20)** — genuine deadlock, victim selection, recovery,
   before/after tables, verification.
7. **Paging and disk (menu 31 or 6–7/17)** — FIFO/LRU/Optimal and
   FCFS/SSTF/SCAN/C-SCAN on identical inputs.

Every scenario rebuilds its state from scratch and can be repeated.

## Verified checkpoints

- Classic schedulers (RR quantum 3): FCFS `11.00/16.60/11.00/5`; SJF
  `7.80/13.40/7.80/5`; SRTF `6.40/12.00/3.80/7`; Priority `9.20/14.80/7.40/6`;
  RR `14.20/19.80/4.20/11` (avg WT/TAT/RT, dispatches).
- Page faults, 3 frames: FIFO `15`, LRU `12`, Optimal `9`.
- Banker's safe sequence `P1 → P3 → P4 → P0 → P2`; then P1 granted, P4 waits,
  P0 denied as unsafe.
- Disk (head 53, size 200): FCFS `640`, SSTF `236`, SCAN `236`, C-SCAN `382`.
- Integrated run (menu 24, eight applications): 83 ticks, 16 file requests,
  average WT/TAT/RT `6.75 / 37.88 / 6.25`, CPU utilisation `49.40%`,
  26 dispatches.
- I/O comparison: service orders `1,2,3,4,5,6,7` (FCFS) and `1,6,7,2,3,4,5`
  (priority); editor write wait 16 → 2 ticks.
- Browser core: 200 assertions in `tests/web/kernel_core_test.js`.

## Browser visualizer

Open `web/index.html` in a modern browser (or serve the folder over HTTP). It is
one self-contained file: no package installation, no server-side code and no
host file access.

Tabs:

- **Kernel dashboard** — process counts by state, RUNNING/READY/WAITING/
  TERMINATED queues, RAM map, pending file requests, CPU and disk lanes, recent
  explained scheduling decisions, process metrics. Controls: Run, Step, Play/
  Pause, Reset.
- **Applications** — editable launch table (name, kind, CPU priority, arrival,
  burst, RAM), CPU and I/O policy selection, Run/Step/Play/Reset, per-application
  state/progress/I-O wait/result, transition log, written files, and the four
  guided scenario buttons.
- **I/O queue** — identical request set under FCFS and priority; service orders,
  per-request order/wait, selection reasons and the requests produced by the
  current application run.
- **CPU scheduling, Memory paging, Deadlock avoidance, Synchronization, Disk
  scheduling, Memory manager, Processes & threads, Deadlock & IPC** — the
  existing interactive modules, unchanged in intent.
- **File system** — directory navigation, create directory, write, read
  (sequential/direct), delete, rename, copy, move, metadata, block map, journal.
  Running the application kernel refreshes this file system, so the files the
  applications wrote appear under `/apps`.
- **Integrated run** — the same eight-application kernel rendered as one
  CPU/disk timeline with RAM, files and blocks.

The browser models are deterministic JavaScript demonstrations. They do **not**
call the C++ executable; the two implementations are kept consistent by sharing
the same rules, sample data and expected checkpoints.

### Testable browser core

The pure simulation core is the marked block `KERNEL-CORE:BEGIN … END` in
`web/index.html` and is loaded by both the page and the Node test:

```sh
node tests/web/kernel_core_test.js   # 200 checks
```

## Source layout

```text
include/pcb.h               PCB, states, validated transitions
include/process_manager.h   Process table, dispatch, I/O wait/wake, terminate
include/scheduler.h         Classic policies, MLQ/MLFQ, Gantt, metrics, explanations
include/apps.h              Applications, kernel simulator, guided scenarios, interactive launch
include/ioqueue.h           File/I/O request queue, FCFS/priority selection, comparison
include/fs.h                In-memory file system, allocation, metadata, recovery
include/memory.h            Paging, fit policies, segmentation, TLB, working set
include/banker.h            Banker's safety and requests
include/deadlock.h          Multi-instance detection, wait-for cycles, recovery
include/sync.h, ipc.h       Semaphores, classical problems, message queues, shared memory
include/threading.h         Thread mapping illustration
include/disk.h              Disk-head scheduling
src/main.cpp                Validated menu and demonstrations
tests/simulator_tests.cpp   Dependency-free C++ regression tests
tests/web/kernel_core_test.js  Node test for the browser core
web/index.html              Single-file browser visualizer
docs/                       Course-topic mapping and syllabus PDF
```

## Known limitations

- Educational model: no real processes, threads, devices, interrupts or host I/O.
- One CPU and one simulated file device; scheduling is deterministic.
- The C++ application kernel uses preemptive priority only; the classic menu
  provides the other CPU policies. The web application launcher also offers
  non-preemptive priority and FCFS.
- Priority inheritance is not implemented (priority inversion can be observed).
- The Banker demo uses a fixed nine-row resource matrix; admission retries when
  RAM or resources are unavailable.
- No persistence: the file system and all logs reset when the program/page is
  restarted.
- The `mini_kernel` file committed at the repository root is an older Linux build.
  Rebuild it from source with the commands above before a demo.
- CMake was not available in the development environment, so CMake and MSVC
  builds were not executed here; the direct GCC builds and both test suites were.
- Page replacement, segmentation and disk scheduling are standalone
  demonstrations, not applied to the application workloads.
