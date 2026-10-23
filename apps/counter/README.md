# Counter

**Kind:** `counter` · **Default priority:** 3 (lowest) · **RAM:** 8 units

A deliberately CPU-heavy application: it sums 1..1000 in five CPU ticks before it
touches the file system. With several higher-priority applications arriving late,
it demonstrates preemption and long waiting times.

## Workload

| Step | Action |
| --- | --- |
| 1 | 5 CPU ticks: sum 1..1000 |
| 2 | `WRITE /apps/counter.txt` (request priority 3) |
| 3 | 1 CPU tick: finish |

## Files

- Output: `/apps/counter.txt`
- Allocation: indexed (data blocks + one index block)

## Expected result

`counted 1..1000, total 500500`, file contents
`1 + 2 + ... + 1000 = 500500`.

## Use it to demonstrate

- CPU bursts and preemption (a late priority-1/2 arrival interrupts it).
- Low-priority I/O sitting behind higher-priority requests.
- Indexed allocation for a small file.
