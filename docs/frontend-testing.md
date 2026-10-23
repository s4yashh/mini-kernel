# Frontend testing and verification

This document records how the browser frontend is launched, how its automated
tests are run, and the results observed.

## 1. Launch

```bat
run_frontend.bat
```

or:

```sh
node tools/serve.js
```

The server serves the **repository root** and the page opens at
`http://127.0.0.1:8088/web/index.html`. Serving the root (not `web/`) is required
because the page fetches `/apps/app-manifest.json` and `/sample_files/...`.

Opening `web/index.html` via `file://` cannot load those files. The page explicitly
shows an error with the failing paths and the launch command; it does not
substitute invented data.

## 2. Run the tests in the browser

1. Open the **Self-tests** tab.
2. Press **Run self-tests**.
3. The page shows `N passed · M failed`, a per-test table, and expected/actual
   values for each failure.
4. **Reset demo & rerun** restores the committed sample copies and the initial
   application run, then reruns the suite. (Test 17 itself resets the demo, so any
   run of the suite leaves the demo at t=0 with the repository copies.)

## 3. The 18 tests

All tests execute the real JavaScript core against the real repository fixtures
(`apps/app-manifest.json`, eight `app.json` files, seven sample files). None of
them merely checks that an HTML element exists.

| # | Test | What is asserted |
| --- | --- | --- |
| 1 | Application definitions load | ≥5 definitions, each with name, description, workload, priority 1–3, memory and file lists |
| 2 | Sample files load with content | ≥6 required files present and non-empty |
| 3 | VFS initialises from samples | every sample is present, blocks are allocated, source tag is `repository sample` |
| 4 | Reads return committed contents | byte-for-byte equality with the fixtures |
| 5 | Editing a virtual copy leaves the original alone | VFS changed, fixture unchanged, source becomes `repository sample (edited)`, origin kept |
| 6 | Copy preserves content with independent blocks | equal bytes, disjoint block lists, used blocks increase by the destination's blocks |
| 7 | Rename/move updates the path | old path gone, new path holds the same bytes and origin |
| 8 | Delete releases blocks | used blocks decrease by exactly the file's block count |
| 9 | File Search counts real log contents | result equals an independently computed `ERROR` count; the output file contains that count |
| 10 | File Analyzer computes real statistics | lines/words/characters match an independent calculation on `numbers.csv` |
| 11 | Priority scheduling picks the highest priority | the priority-1 process is first on the Gantt chart |
| 12 | Preemption works | a late priority-1 arrival runs at its arrival tick and a preemption is logged |
| 13 | FCFS and priority I/O orders | `1,2,3,4,5,6,7` vs `1,6,7,2,3,4,5` for the same requests |
| 14 | I/O blocks and wakes processes | `RUNNING→WAITING` and `WAITING→READY` appear for the same process |
| 15 | First/Best/Worst Fit allocate valid partitions | starts `0/30/0`, contiguous regions, used + free = capacity |
| 16 | Deadlock detection and recovery stay consistent | `P0,P1` detected, victim resources returned, no residual deadlock |
| 17 | Reset restores the initial demonstration state | a fresh VFS signature is reproduced and the live view matches after Reset |
| 18 | Missing files and invalid requests fail safely | missing reads throw; invalid writes/copies/renames are rejected; duplicate PIDs throw; storage is unchanged |

## 4. Run the same tests from Node

```sh
node tests/web/frontend_selftest_node.js
```

Observed result (this repository, Windows, Node v24.14.1):

```text
PASS 1. application definitions load
...
PASS 18. missing files and invalid requests fail safely

18 passed, 0 failed, 18 total (8 apps, 7 sample files)
```

The Node runner also validates every manifest path and the JSON fields, and fails
if a definition, README, or sample file is missing.

The lower-level core suite remains available:

```sh
node tests/web/kernel_core_test.js
# All 234 browser kernel-core checks passed.
```

C++ regression tests:

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude tests/simulator_tests.cpp -o build/simulator_tests.exe
.\build\simulator_tests.exe
# All Mini Kernel Simulator tests passed.
```

## 5. Browser checks performed manually

Run in Chrome against `http://127.0.0.1:8088/web/index.html` on the working copy,
with the browser console captured:

| Check | Observed |
| --- | --- |
| Page load | `Loaded 8 application definitions and 7 sample files from the repository (523 bytes)`; console empty |
| Tabs | all 14 panels render; no errors |
| Repository table | 7 rows with committed path, type, bytes, `/docs` path and allocation |
| Preview `report.txt` | exact committed text (`Mini Kernel Simulator report…`) |
| Copy `report.txt` → `/apps/repo-copy-check.txt` | 89 bytes, 6 blocks, message says the committed file is unchanged |
| Edit `/docs/notes.txt` | source becomes `repository sample (edited)`, original remains 113 bytes in the repository preview |
| Search `ERROR` | 4 hits: 2 in the VFS `system.log`, 2 in the repository sample |
| Reset | `/docs/notes.txt` back to 113 bytes and `repository sample`; the extra `/apps` copy is gone |
| Default 8-application run | 83 ticks; all eight results present; `System Monitor` reports `used-blocks=76/128` |
| Edit `system.log` then run scenario 3 | File Search reports `found 3 occurrence(s) of ERROR` (the edited content) while the repository preview still shows 2 |
| Terminate a live app at t=4 | `P2 terminated at t=4`, state TERMINATED, other apps continue |
| Self-tests | `18 passed · 0 failed` |
| CPU scheduling | Step/Play/scrub work; decisions and queues update |
| Deadlock | Detect `P0, P1`; recover; no residual deadlock |
| Synchronization / IPC / TLB | traces generated by the JS primitives |
| Memory allocator | allocate/release updates the map, free space and fragmentation |
| I/O queue | editor wait 16 (FCFS) vs 2 (priority) |

## 6. Failure output

A failing test shows its name, `FAIL`, and the expected and actual values, for
example:

```text
FAIL 9. file search counts the real log contents
     expected: found 2 occurrence(s) of ERROR and "2 match(es)" in the file
     actual:   found 1 occurrence(s) of ERROR
```

The Node runner prints the same information and exits with code 1.

## 7. Troubleshooting

| Symptom | Cause / fix |
| --- | --- |
| `The repository sample files could not be loaded` | The page was opened via `file://` or served from `web/` instead of the repo root. Use `run_frontend.bat`. |
| `HTTP 404 for …/sample_files/…` | A manifest path does not exist; the banner lists the exact path. |
| Port 8088 busy | `$env:PORT=9000; node tools/serve.js` |
| Self-tests fail after manual edits | Expected for tests that assume the committed fixtures; press **Reset demo & rerun**. |
| C++ binary is stale | Rebuild it; the frontend does not use it. |
