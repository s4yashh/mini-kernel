# Review 2 — Demonstration Guide (browser-first)

The primary demonstration is the **browser frontend**. No terminal, build step or
C++ executable is required.

## Launch

1. Open `web/index.html` in Chrome, Edge or Firefox (double-click works).
2. If your browser restricts local files, run `npx serve web` in the project
   folder and open the printed `http://localhost` address.
3. Keep the pages at a readable zoom; the dashboard scrolls horizontally on
   narrow screens.

Everything below is done in the browser. The C++ console (menu numbers in the
README) is an optional reference if a viva question asks for it.

## 10-minute script

### 1. The main claim (1 minute)

Open **Kernel dashboard → Run to completion**.

Say: *“One kernel schedules both CPU work and file requests. CPU priority decides
which process runs; file-request priority decides which pending file operation is
served next. They are separate policies.”*

Point at: RUNNING / READY / WAITING / TERMINATED queues, RAM map, pending and
serving file requests, CPU/disk lanes, the explained scheduling decisions, and
the process metrics (CT, TAT, WT, RT, dispatches).

Use **Step** a few times to show the state changing; **Play/Pause** to animate.

### 2. Applications and termination (2 minutes)

**Applications** tab.

- Edit a priority (1 highest), an arrival time, a burst or RAM; press **Run to
  completion** and show that the order and metrics change.
- Point at real outputs: calculator `14 + 28 = 42`, counter `500500`, file search
  `found 2 occurrence(s) of ERROR`, analyzer `lines=6 …`.
- Reset (or Step to a live tick), then press **Terminate** on a running app. It
  becomes TERMINATED, its memory/resources/pending request are released, and the
  other applications continue. Reset restores it.
- Switch CPU policy to *Non-preemptive priority* or *FCFS* and rerun to contrast.

### 3. CPU priority and preemption (2 minutes)

Click **1 · CPU priority** at the bottom of the Applications tab. It switches to
the dashboard with four applications.

Point at the decisions: `P3 (priority 2) preempts P1 (priority 3)` at t=2 and
`P4 (priority 1) preempts …` at t=5, plus `lowest priority number in READY`,
tie-break *priority → arrival → PID*, and the Gantt chart showing the preemption.

Then open **CPU scheduling** for FCFS/SJF/SRTF/Priority/RR/MLQ/MLFQ, the ready
queue line, per-dispatch explanations, Step/Play and the comparison table.

### 4. I/O priority (2 minutes)

**I/O queue → Run comparison**.

- Same seven requests under both policies.
- FCFS service order `#1…#7`; priority order `#1,#6,#7,#2,#3,#4,#5`.
- The editor write waits **16 ticks** under FCFS and **2 ticks** under priority.
- The table shows application, operation, path, priority, submission, queue
  depth, status, service order and wait; the log explains every selection.
- Emphasise that this is a *request* queue, not the disk-head movement shown in
  the Disk scheduling tab.

### 5. File system (2 minutes)

**File system** tab (cwd `/apps` after a kernel run).

Write a file, append, copy with indexed allocation, rename/move, read, delete,
create a directory, show metadata (owner, blocks, index block, writes), and point
at the block map and free-block counter. Deliberately try a huge write: it fails
without changing existing blocks.

Show that the files the applications wrote are here (`calculator.txt`,
`search.txt`, `report-copy.txt`, …).

### 6. OS modules (2 minutes)

- **Memory manager** — First/Best/Worst Fit comparison, then allocate/release in
  the interactive allocator; fragmentation changes with the policy.
- **Deadlock & IPC** — **Detect deadlock** → `P0, P1`; choose the victim policy →
  **Recover** → `Re-run detection: no deadlock remains`; the table shows the
  released resources. Then **Run IPC demo**.
- **Synchronization** — producer–consumer (blocked consumer/producer), dining
  philosophers (deadlock then ordering), semaphore/mutex, Peterson/Bakery.
- **Memory paging / Disk scheduling** — FIFO/LRU/Optimal and FCFS/SSTF/SCAN/C-SCAN.
- **Processes & threads** — lifecycle and thread mappings.

### 7. Close

*“Every result shown is computed by the page at run time — the search counts real
matches, the analyzer measures the CSV, the copy equals the source, the counter
computes 500500, and the same C++ and browser rules produce identical metrics.”*

## Likely viva questions

| Question | Answer |
| --- | --- |
| Does the browser call the C++ program? | No. The page runs its own JavaScript models; both follow the same rules and checkpoints. |
| Why are CPU and I/O priorities separate? | A low-priority process may need an urgent write (saving an editor document) while a high-priority process only issues background reads. The device queue needs its own policy. |
| Is priority I/O universal in real OSes? | No. It is a configurable educational policy; real systems use deadline/CFQ-style schedulers. |
| How are ties broken? | CPU: priority → arrival → PID. I/O: priority → submission time → request ID. |
| Can applications starve? | Yes, an endless stream of higher-priority work can starve low priority; aging is not implemented. |
| What happens when an application is terminated? | It becomes TERMINATED at that tick; RAM, Banker resources and any pending request are released, and other applications continue. |
| Are files real? | No. Directories, files, blocks, journal and metadata are in memory only. |
| Where is the process code? | Applications are multi-step programs (CPU steps and file calls) executed by the kernel in `web/index.html`. |

## Troubleshooting

- The page is self-contained; if styles look different offline, the web font falls
  back to the system font and functionality is unaffected.
- If the browser blocks local files, use `npx serve web`.
- If a table is wide, widen the window or scroll it horizontally.
- Use **Reset** before repeating a scenario; Reset also clears terminations.
