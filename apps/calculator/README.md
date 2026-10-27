# Calculator

**Kind:** `calculator` · **Default priority:** 1 (highest) · **RAM:** 6 units

Evaluates `14 + 28` on the simulated CPU, writes the result to the simulated file
system, then finishes. It is the shortest application and demonstrates a
priority-1 process winning the CPU immediately.

## Workload

| Step | Action |
| --- | --- |
| 1 | 1 CPU tick: compute |
| 2 | `WRITE /apps/calculator.txt` (request priority 2) |
| 3 | 1 CPU tick: finish |

## Files

- Output: `/apps/calculator.txt`
- Allocation: contiguous

## Expected result

`computed 14 + 28 = 42`, file contents `14 + 28 = 42` (12 bytes, one block).

## Use it to demonstrate

- Priority scheduling: this process runs before priority 2 and 3 work.
- File-system write and block allocation.
- A minimal workload with a blocking system call between two CPU steps.
