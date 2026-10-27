# System Monitor

**Kind:** `system-monitor` · **Default priority:** 2 · **RAM:** 6 units

Samples the simulated kernel's own state — number of files, directories and used
disk blocks — and writes a snapshot. The numbers come from the live file system,
so they change as other applications create files.

## Workload

| Step | Action |
| --- | --- |
| 1 | 2 CPU ticks: sample file/directory/block counts |
| 2 | `WRITE /apps/monitor.txt` (request priority 2) |
| 3 | 1 CPU tick: finish |

## Files

- Output: `/apps/monitor.txt`
- Allocation: linked

## Expected result

A line such as
`simulated kernel snapshot: files=10 directories=3 used-blocks=55/128`.

## Use it to demonstrate

- An application reading kernel-level state instead of a user file.
- How the block count grows as applications write files.
- A short, mixed CPU/I-O workload at medium priority.
