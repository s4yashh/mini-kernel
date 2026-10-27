# Mini Kernel Simulator

A C++17, menu-driven educational simulator that makes common operating-system
decisions visible through deterministic logs, tables and text charts. It does
**not** modify the host OS, control real hardware, or write to the host file
system. A browser frontend is the primary demonstration and loads its real sample
files and application definitions from this repository.

Course: **BCSE303L Operating Systems**. Team: Suyash Singh, Pratik Tekriwal,
Aditya Sharma.

## Frontend-first demonstration

**Everything needed for the review runs in the browser**, and the browser reads
the actual committed files under `apps/` and `sample_files/`. The C++ program is
kept for reference and regression tests but is **not required** for the demo and is
never invoked by the page.

| Feature | Where in the browser | Runs entirely in the browser? |
| --- | --- | --- |
| Launch / configure / terminate applications | Applications | Yes — live JS kernel |
| Process states, queues, I/O wait and wake-up | Kernel dashboard, Applications | Yes |
| CPU scheduling (FCFS, SJF, SRTF, Priority, RR, MLQ, MLFQ) + decision reasons | CPU scheduling, Dashboard | Yes |
| Gantt charts, metrics, execution logs | CPU scheduling, Dashboard, Integrated run | Yes |
| Repository sample files (read-only) + real content preview | File system | Yes — fetched from the repo |
| Virtual file system (create/read/write/append/rename/copy/move/delete/metadata/blocks/content search) | File system | Yes |
| File/I/O request queue (FCFS vs priority, same requests) | I/O queue, Dashboard | Yes |
| First / Best / Worst Fit + interactive allocator | Memory manager | Yes |
| Banker's algorithm | Deadlock avoidance | Yes |
| Deadlock detection and recovery | Deadlock & IPC | Yes |
| Semaphores, mutex, producer–consumer, dining philosophers, Peterson/Bakery | Synchronization | Yes |
| Message queue and shared memory (IPC) | Deadlock & IPC | Yes |
| FIFO / LRU / Optimal paging | Memory paging | Yes |
| FCFS / SSTF / SCAN / C-SCAN disk scheduling | Disk scheduling | Yes |
| TLB and working-set / thrashing trace | Memory manager | Yes |
| Automated verification of the above | Self-tests | Yes — 18 tests, also runnable from Node |

There is no backend, database or build step. A **local static server is required**
because browsers block `fetch()` from `file://` pages, and the sample files and app
definitions are loaded with `fetch()` rather than duplicated in the HTML.

### Quick start

Windows (recommended):

```bat
run_frontend.bat
```

This starts a dependency-free Node static server, opens
`http://127.0.0.1:8088/web/index.html`, and prints the URL. Manual equivalent:

```sh
node tools/serve.js
```

Then use the tabs:

1. **Kernel dashboard → Run to completion**; then **Step** or **Play** to watch
   queues, RAM, file requests and decisions.
2. **Applications** → edit a priority/burst, press **Run to completion**, and use
   **Terminate** on a live process.
3. **I/O queue** → **Run comparison** for the FCFS-vs-priority proof.
4. **File system** → preview the committed `sample_files/` files, copy them into
   the VFS, edit/rename/delete virtual copies, search contents, inspect blocks.
5. **Self-tests → Run self-tests** → 18 browser tests against the real fixtures.

If Node is not installed, any static server rooted at the repository (not at
`web/`) works, for example `npx serve .` then open `/web/index.html`. The page must
be served from the repository root so `../sample_files` and `../apps` resolve.


## Repository files, manifest and virtual copies

The project contains the actual data the teacher can inspect on GitHub:

```text
apps/
  app-manifest.json          catalogue of applications + sample-file entries
  calculator/                app.json (metadata/workload/files) + README.md
  text-editor/
  counter/
  file-search/
  file-analyzer/
  file-copy/
  background-indexer/
  system-monitor/
sample_files/
  notes.txt  report.txt  numbers.csv  source.c  config.json  system.log  style.txt
  README.md
web/
  index.html                 frontend (kernel core inline between markers)
  js/samples.js              manifest + sample loader/validator (fetch)
  js/selftest.js             the 18-test browser suite
tools/serve.js               dependency-free static server
run_frontend.bat             launcher
```

### How loading works

1. `web/index.html` calls `SampleLoader.load('../')`.
2. The loader fetches `apps/app-manifest.json` from the repository root.
3. It fetches every `definition` and every `source` path listed there, then
   validates the JSON fields, unique paths/kinds, allowed allocations and
   non-empty contents.
4. On success, the repository samples are copied into the in-memory VFS at their
   declared `/docs/...` paths. Each entry is tagged `repository sample` with its
   `origin` path.
5. On failure (missing path, empty file, invalid JSON, `file://`), the page shows
   the exact failing path and disables the simulation; it never invents contents.

**The originals are never modified.** The VFS is a copy: editing `/docs/notes.txt`
marks it `repository sample (edited)` and keeps its `origin`; deleting or copying
affects only memory. The File system tab shows the committed files in a separate
read-only table with their real contents and a "Copy into VFS" action. **Reset** on
the Applications tab (or "Reset demo & rerun" on Self-tests) restores the committed
copies.

The application kernel and the explorer share one VFS: edits become inputs to the
next run (edit an `ERROR` line in `system.log` and File Search reports a different
count), and app-written files carry the source `virtual`.

### Sample file purposes

| File | VFS path | Used by | Expected |
| --- | --- | --- | --- |
| `notes.txt` | `/docs/notes.txt` | Text Editor | edited copy gains `[edited by text-editor]` |
| `report.txt` | `/docs/report.txt` | File Copy Utility | byte-identical copy |
| `numbers.csv` | `/docs/numbers.csv` | File Analyzer | `lines=6 words=6 characters=34` |
| `source.c` | `/docs/source.c` | Background Indexer | read and indexed |
| `config.json` | `/docs/config.json` | Background Indexer | read and indexed |
| `system.log` | `/docs/system.log` | File Search | 2 `ERROR` lines |
| `style.txt` | `/docs/style.txt` | Background Indexer | read and indexed |

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

The browser frontend needs no build. The C++ console program is optional and is
kept for reference, course submission and regression tests.

### Browser frontend (primary)

```bat
run_frontend.bat
```

or manually:

```sh
node tools/serve.js
# open http://127.0.0.1:8088/web/index.html
```

The server must be rooted at the repository (not `web/`) so the page can fetch
`sample_files/` and `apps/`. Opening `web/index.html` via `file://` cannot load
those files; the page then shows an error instead of inventing data.

### C++ console (optional reference)

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
- Browser core: 234 assertions in `tests/web/kernel_core_test.js` (application
  kernel, scheduling, I/O policies, file system, deadlock, synchronization, IPC,
  TLB/working set, termination, queue depth).

## Browser frontend (primary demonstration)

Serve the repository root (`run_frontend.bat` or `node tools/serve.js`) and open
`http://127.0.0.1:8088/web/index.html`. The page fetches the committed sample
files and app definitions, validates them, and reports exact errors instead of
falling back to invented content. It executes its own JavaScript simulation models
and renders the results directly; the C++ executable is not involved.

Tabs and controls:

- **Kernel dashboard** — process counts by state, RUNNING/READY/WAITING/
  TERMINATED queues, RAM map, pending and currently-served file requests, CPU and
  disk lanes, recent explained scheduling decisions, process metrics. Controls:
  Run to completion, Step, Play/Pause, Reset. The serving request is shown with
  its remaining ticks; the pending table shows priority and path.
- **Applications** — the launch table is populated from `apps/app-manifest.json`
  (name, workload description, CPU priority, arrival, burst, RAM, output file).
  CPU policy (preemptive priority, non-preemptive priority, FCFS) and I/O policy
  (priority/FCFS), Run/Step/Play/Reset, per-application state, progress, I/O wait,
  real result, **Terminate** action, transition log, written files and four guided
  scenario buttons. Terminating a process at the current tick releases its RAM,
  Banker resources and any pending file request; **Reset** re-copies the committed
  samples and replays from t=0.
- **I/O queue** — identical request set under FCFS and priority; service orders,
  per-request application, operation, path, priority, submission time, queue
  depth, status, service order and wait; selection reasons; the requests produced
  by the running applications.
- **CPU scheduling** — FCFS, SJF, SRTF, Priority, Round Robin, MLQ and MLFQ;
  editable processes, Gantt, scrubber, Play/Pause, **Step**, state-transition log
  with a reason for every dispatch and preemption, RUNNING/READY/WAITING/
  TERMINATED queues at the current tick, per-process CT/TAT/WT/RT and dispatches,
  and the five-policy comparison.
- **Memory paging** — FIFO/LRU/Optimal, per-reference frames, faults, eviction,
  comparison bars (executed in JS).
- **Deadlock avoidance** — Banker's safety and interactive resource requests with
  deny/wait/grant decisions and a safety trace.
- **Synchronization** — semaphore/mutex wait–signal, producer–consumer,
  dining philosophers (deadlock and ordering fix), Peterson and Bakery; each run
  builds a live trace from the simulated primitives.
- **Disk scheduling** — FCFS, SSTF, SCAN, C-SCAN with service order, head path,
  movement, average seek and comparison.
- **Memory manager** — First/Best/Worst Fit on an identical request sequence,
  interactive allocate/release with the three policies (the same JS functions the
  self-tests exercise), segmented address translation, LRU TLB, and a
  working-set/thrashing trace.
- **Processes & threads** — process lifecycle (create, dispatch, I/O wait, wake,
  terminate) and user/kernel thread mapping, generated by a small state machine.
- **Deadlock & IPC** — multi-instance deadlock detection (allocation/request/
  available table), wait-for cycle, victim selection (lowest PID or fewest
  resources held), recovery with before/after state; bounded message queue and
  permission-checked shared memory.
- **File system** — a **read-only repository sample table** with the actual
  committed paths, types, byte counts, virtual destinations and real content
  previews; a **virtual file system** table with name, type, bytes, blocks,
  allocation, owner, source (`repository sample`, `repository sample (edited)`,
  `virtual`, `virtual copy`) and an **Open** action; an editor, content search,
  create directory, write/read/delete/rename/copy/move/metadata, block map and
  journal. Copying a repository sample creates an independent virtual file with
  its own blocks; editing marks the virtual copy without touching the original.
- **Integrated run** — the same eight-application kernel rendered as one
  CPU/disk timeline with RAM, files and blocks.
- **Self-tests** — 18 automated checks with pass/fail counts, failed test names,
  expected/actual values, and a reset-and-rerun control.

The browser models are deterministic JavaScript demonstrations. They do **not**
call the C++ executable; the two implementations are kept consistent by sharing
the same rules, sample data and expected checkpoints (for example, both produce
83 ticks and a 37.88 average turnaround for the default eight-application run).

### Verification

Two layers run against the real code:

1. **In the browser** — the **Self-tests** tab runs the actual core and fixtures
   (no DOM checks). It shows passed/failed counts and expected/actual on failures.
2. **From Node** —

```sh
node tests/web/kernel_core_test.js        # 234 core assertions
node tests/web/frontend_selftest_node.js  # the 18 browser-suite tests
```

See `docs/frontend-testing.md` for the exact checks and the recorded results, and
`docs/demo-guide.md` for the review script.

### Testable browser core

The pure simulation core is the marked block `KERNEL-CORE:BEGIN … END` in
`web/index.html`; the loader and browser suite are separate modules. All three are
loaded by the page and by Node:

```sh
node tests/web/kernel_core_test.js        # 234 core assertions
node tests/web/frontend_selftest_node.js  # 18 browser-suite tests
```

The core test covers the application kernel, scheduling, I/O policies, file
system, deadlock detection/recovery, synchronization primitives, IPC, TLB and
working set, user termination, and queue-depth accounting. The frontend suite
additionally validates the manifest/app definitions and the committed sample
contents end to end.

## Source layout

```text
apps/                       application definitions (app.json + README per app)
  app-manifest.json         catalogue used by the frontend loader
sample_files/               committed inputs for the virtual file system
web/index.html              browser frontend (kernel core inline between markers)
web/js/samples.js           manifest/sample loader + validator (fetch)
web/js/selftest.js          the 18-test browser verification suite
tools/serve.js              dependency-free static server
run_frontend.bat            Windows launcher
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
tests/web/kernel_core_test.js       Node test for the browser core
tests/web/frontend_selftest_node.js Node runner for the browser self-test suite
docs/frontend-testing.md    verification instructions + recorded results
docs/demo-guide.md          browser-first review script
docs/Module_Topics_Used.md  course-topic mapping
```

## Frontend verification

The browser was tested directly (Chrome, served locally) and its simulation core
is covered by both Node runners.

| Check | Result |
| --- | --- |
| Page and all 14 tabs load; console clean | Pass — no errors or warnings |
| Manifest + 8 app definitions + 7 sample files load from the repo | Pass — exact paths and byte counts |
| Repository preview shows committed contents; originals are never modified | Pass — virtual edits do not change `sample_files/` |
| Copy/edit/rename/move/delete/create-directory in the VFS | Pass — blocks and source labels update |
| Launch 8 applications, change priority/burst/RAM, run to completion | Pass — order and metrics change with the inputs |
| Edited `system.log` changes File Search's result | Pass — 2 → 3 matches after adding an `ERROR` line |
| Preemptive priority and non-preemptive/FCFS CPU policies | Pass — preemption only in preemptive mode |
| Terminate a live application | Pass — state TERMINATED, RAM/resources/pending request released, other applications continue |
| Step / Play / Pause / Reset | Pass — Reset restores repository copies and clears terminations |
| Self-tests tab | Pass — 18 passed, 0 failed (expected/actual shown on failures) |
| File operations and virtual storage allocation | Pass |
| FCFS vs priority I/O on the same request set | Pass — editor write wait 16 → 2 ticks |
| FIFO/LRU/Optimal paging, FCFS/SSTF/SCAN/C-SCAN | Pass — expected fault and movement counts |
| First/Best/Worst Fit comparison and interactive allocator | Pass — same JS functions as the self-tests |
| Banker request grant/wait/deny | Pass |
| Deadlock detect → recover → detect | Pass — no residual deadlock |
| Semaphores, producer–consumer, dining, Peterson/Bakery, IPC | Pass — traces generated by the JS primitives |
| TLB hit/miss/page-fault and working-set pressure | Pass |
| Process WAITING → READY resumption | Pass — visible in the transition log and dashboard |

## Known limitations

- Educational model: no real processes, threads, devices, interrupts or host I/O.
- One CPU and one simulated file device; scheduling is deterministic.
- The frontend implements every algorithm in JavaScript; the C++ console is an
  independent reference implementation and is not called by the page.
- The page requires a local HTTP server to fetch the repository sample files;
  `file://` is rejected with an explicit error. Use `run_frontend.bat`.
- Repository samples are inputs: the virtual copies are in memory and disappear
  when the page is reloaded; Reset restores the committed contents.
- Priority inheritance is not implemented (priority inversion can be observed).
- The Banker demo uses a fixed nine-row resource matrix; admission retries when
  RAM or resources are unavailable.
- The `mini_kernel` file committed at the repository root is an older Linux build.
  It is not needed for the frontend demonstration.
- CMake was not available in the development environment, so CMake and MSVC
  builds were not executed here; the direct GCC builds and both test suites were.
- Page replacement, segmentation and disk scheduling are standalone
  demonstrations, not applied to the application workloads.

