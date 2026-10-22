# Review 2 — Demonstration Guide

A 10–12 minute live demonstration of the Mini Kernel Simulator. Build first:

```sh
g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp -o mini_kernel
./mini_kernel
```

Or open `web/index.html` in a browser. The console and the browser are
independent implementations of the same model; both can be shown.

## Before the demo

- Rebuild the executable (the `mini_kernel` file committed at the repository root
  is an older build).
- Keep the README section "Guided demonstrations" open as a reference.
- If using the browser, open the Kernel dashboard first.

## 1. The main claim (2 minutes)

Say: **"CPU priority decides which process runs. File-request priority decides
which pending file operation the device serves next. They are separate."**

Console: menu **24** (eight applications) or menu **27** (scenario 1).

Point out:

- the launch table: priority 1 highest, burst, RAM, Banker request, output file;
- `scheduler: P6 (file-copy, priority 1) selected over ... lowest priority number`;
- `P4 (priority 1) preempts P1 (priority 3)` when the high-priority process
  arrives late;
- WAITING/READY transitions when an application blocks on a file;
- the per-process TAT/WT/RT and dispatch counts.

Browser: **Applications → 1 · CPU priority**, then Step/Play. The CPU lane shows
the same preemption; the dashboard explains the last decisions.

## 2. CPU priority (2 minutes)

Scenario 1 (menu **27** or the browser button): the priority-3 counter starts at
t=0; the priority-2 editor arrives at t=2 and preempts; the priority-1 calculator
arrives at t=5 and preempts again. The run takes 29 ticks by default.

Mention the deterministic tie-break: **priority → arrival → PID**.

## 3. I/O priority (3 minutes)

Menu **28** or browser **I/O queue**.

- Identical seven requests under both policies.
- FCFS order `#1,#2,#3,#4,#5,#6,#7`; priority order `#1,#6,#7,#2,#3,#4,#5`.
- The editor's write waits **16 ticks** under FCFS and **2 ticks** under priority.
- Average wait can rise for background requests: that is the trade-off.
- The selection reason is printed for every request.
- Relate it to `disk.h`: head movement (SSTF/SCAN) is a different question from
  choosing the next request.

Then show the full application run comparison: same applications and files, two
I/O policies, different service orders and turnaround.

## 4. File-dependent application (1 minute)

Menu **29** or browser **Applications → 3 · File-dependent app**.

- File search issues a READ, enters WAITING, another process runs, the completion
  wakes it, and the result `found 2 occurrence(s) of ERROR` is printed.
- File analyzer prints real `lines/words/characters` for `numbers.csv`.
- The background indexer's priority-3 reads are served last.

## 5. File system and blocks (2 minutes)

Menu **30** or browser **File system**.

- create `/apps/work`, create `one.txt`, append (file grows), copy with indexed
  allocation (data + index block), create/delete `temp.txt`, rename.
- Show the block map and free-block counter changing.
- Show metadata (owner, size, blocks, index block, writes).
- Deliberately write a 2000-byte contiguous file: **REJECTED**, blocks unchanged.
- In the browser, rename/copy/move/create-directory/metadata buttons use the same
  simulated block store.

## 6. Review 1 checks (1 minute)

- Menu **4**: five classic schedulers and their checkpoints.
- Menu **9 / 31**: First/Best/Worst Fit on identical requests; note that P8 is
  placed at 10 / 75 / 40 and fragmentation is 25 / 15 / 25.
- Menu **20**: deadlock, victim selection, before/after tables, verification.
- Menu **7 / 17**: paging and disk comparisons.

## Likely viva questions

| Question | Answer |
| --- | --- |
| Why is I/O priority separate from process priority? | A process can be low priority yet need an urgent write (editor save), while a high-priority process may only issue background reads. The device queue needs its own policy. |
| Is this how a real OS works? | This is a configurable educational policy. Real systems use deadline/I-O schedulers (e.g. BFQ, mq-deadline) and priorities are one input, not a universal rule. |
| What happens on equal priorities? | Earlier arrival time, then lower PID, for both CPU and I/O queues. |
| Does priority scheduling starve processes? | Yes, an endless stream of higher-priority work can starve low priority; the simulator says so. Aging is not implemented. |
| Where is priority inversion? | A high-priority process can wait for a resource held by a low-priority one; priority inheritance is not implemented. |
| Does the browser call the C++ program? | No. It is a separate deterministic JavaScript model. Both are kept consistent by the same rules and sample data. |
| How do you know the applications really ran? | Each result is derived from actual execution: search counts real matches, the analyzer measures the CSV, the copy equals the source, the counter computes 500500. |
| Are files on disk? | No. The file system, blocks and journal are in memory only. |

## If something goes wrong

- Rebuild with the commands in the README; the committed binary may be stale.
- In the browser, use **Reset** then **Run**; each scenario rebuilds its state.
- Large terminal fonts can wrap tables; widen the window or redirect output to a
  file (`./mini_kernel > demo.txt`).
