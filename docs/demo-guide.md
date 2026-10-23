# Review — demonstration guide (browser-first)

The primary demonstration is the **browser frontend**. The page loads the real
application definitions and sample files committed in this repository. No
terminal menu, C++ build or console input is needed.

## Launch

1. Run **`run_frontend.bat`** (or `node tools/serve.js`).
2. Open `http://127.0.0.1:8088/web/index.html` (the launcher opens it).
3. Wait for the green banner: *“Loaded 8 application definitions and 7 sample files…”*

A server is required because browsers block `fetch()` from `file://`, and the page
reads `apps/` and `sample_files/` over HTTP. The banner reports exact missing paths
if anything is wrong; it never invents content.

## 10-minute script

### 1. What the repository contains (1 minute)

Open **File system**.

- Top card: the **actual committed files** under `sample_files/` with their real
  contents (press **Preview** on `report.txt`, `numbers.csv`, `system.log`).
- Bottom card: the **virtual file system**. After boot, `/docs` contains the same
  seven files tagged `repository sample`; `/apps` will contain application output.
- Say: *“The committed files are inputs. The kernel works on an independent
  in-memory copy; editing or deleting a virtual file never touches GitHub.”*

### 2. Multi-file, multi-application kernel (2 minutes)

**Applications → Run to completion**, then **Kernel dashboard**.

- Eight applications from `apps/app-manifest.json`, each with a real workload and
  output file.
- Point at the queues, RAM map, serving request, CPU/disk lanes and decisions.
- Real results: calculator 42, counter 500500, file search 2 ERROR matches,
  analyzer `lines=6 words=6 characters=34`, copy equals the source, indexer read
  five files, monitor reports live block usage.
- **Terminate** a live app, then **Reset** to restore everything.

### 3. CPU priority (2 minutes)

- Applications → **1 · CPU priority**: priority-3 work starts first, priority-2
  and priority-1 arrivals preempt it; the decisions are explained.
- **CPU scheduling** tab: FCFS/SJF/SRTF/Priority/RR/MLQ/MLFQ, ready queue,
  Gantt, Step/Play, per-dispatch explanations and metrics.

### 4. I/O priority (2 minutes)

**I/O queue → Run comparison**.

- Same seven requests, two policies.
- FCFS `1,2,3,4,5,6,7`; priority `1,6,7,2,3,4,5`.
- Editor write wait **16 → 2 ticks**.
- Show application, operation, path, request priority, submission, queue depth,
  status and service order. Emphasise that this queue is independent of CPU
  priority and of disk-head movement.

### 5. Real file contents and edits (1 minute)

In **File system**:

- Preview `system.log` (repository, 2 ERROR lines).
- Press **Open** on the virtual `/docs/system.log`, append
  `ERROR: added in the browser`, press **Write**.
- Run **Applications → 3 · File-dependent app**: File Search now reports
  **3** occurrences, while the repository preview still shows the original.
- Copy a repository sample with **Copy into VFS**; show its own blocks and source.
- Search `ERROR`: hits are listed with `/docs/...` (VFS) and `sample_files/...`
  (repository) paths, proving the two are distinct.

### 6. Other OS modules (1 minute)

- **Memory manager** — First/Best/Worst comparison + interactive allocate/release.
- **Deadlock & IPC** — detect `P0, P1`, recover, no residual deadlock; then IPC.
- **Synchronization** — producer–consumer, dining, semaphore, Peterson/Bakery.
- **Memory paging / Disk scheduling** — FIFO/LRU/Optimal; FCFS/SSTF/SCAN/C-SCAN.
- **Processes & threads** — lifecycle and mappings.

### 7. Prove it works (30 seconds)

**Self-tests → Run self-tests**: `18 passed · 0 failed`, with each test name and
pass/fail. Failures would show expected vs actual.

## Likely viva questions

| Question | Answer |
| --- | --- |
| Where do the sample files live? | `sample_files/` in the repository; the frontend fetches them over HTTP. |
| Are they modified when I edit in the browser? | No. The VFS is an in-memory copy tagged `repository sample`; edits mark it `(edited)` and keep the origin path. |
| Why is a server required? | Browsers block `fetch()` on `file://`. `run_frontend.bat` starts a zero-dependency Node static server. |
| Does the browser call the C++ program? | No. Both are independent implementations with the same rules and checkpoints. |
| Why are CPU and I/O priorities separate? | A low-priority process may need an urgent write; the device queue has its own policy. |
| How are ties broken? | CPU: priority → arrival → PID. I/O: priority → submission time → request ID. |
| Can applications starve? | Yes, endless higher-priority work can starve low priority; aging is not implemented. |
| What does Terminate do? | TERMINATED at that tick; RAM, Banker resources and pending requests are released; other apps continue. |
| Are the files real OS files? | No. Directories, files, blocks and journal are in memory; only the committed inputs are real repository files. |

## Troubleshooting

- Red boot banner → serve the repository root, not `web/`; the banner lists the
  exact failing path. Use `run_frontend.bat`.
- Port 8088 busy → `$env:PORT=9000; node tools/serve.js`.
- Self-tests after manual edits → **Reset demo & rerun**.
- Wide tables → widen the window or scroll horizontally.
