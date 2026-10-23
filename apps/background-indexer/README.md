# Background Indexer

**Kind:** `indexer` · **Default priority:** 3 (lowest) · **RAM:** 8 units

Models a background service that scans five files and indexes them. All of its
requests use I/O priority 3, so they visibly queue behind the editor's write and
the search's read when the priority policy is selected.

## Workload

| Step | Action |
| --- | --- |
| 1–5 | `READ` source.c, config.json, numbers.csv, style.txt, report.txt (all priority 3) |
| 6 | 2 CPU ticks: build the index |
| 7 | `WRITE /apps/index.txt` (priority 3) |
| 8 | 1 CPU tick: finish |

## Files

- Inputs: five files under `/docs`
- Output: `/apps/index.txt`
- Allocation: indexed

## Expected result

`indexed 5 files into /apps/index.txt`; the output lists the five file names.

## Use it to demonstrate

- A burst of low-priority I/O requests.
- The FCFS-vs-priority contrast: all five reads move behind a priority-1 write.
- Repeated READ → WAITING → READY transitions (five times).
