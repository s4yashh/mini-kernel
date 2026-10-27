# File Copy Utility

**Kind:** `file-copy` · **Default priority:** 1 (highest) · **RAM:** 9 units

Copies a repository sample copy (`/docs/report.txt`) to a new virtual file using
the simulated block allocator. The destination receives its own blocks; the
source is untouched.

## Workload

| Step | Action |
| --- | --- |
| 1 | `COPY /docs/report.txt → /apps/report-copy.txt` (request priority 1) |
| 2 | 1 CPU tick: finish |

## Files

- Input: `/docs/report.txt`
- Output: `/apps/report-copy.txt`
- Allocation: contiguous (the destination must fit in one run)

## Expected result

`copied /docs/report.txt to /apps/report-copy.txt`; both files have identical
contents and different block lists.

## Use it to demonstrate

- Copying through the file system rather than a host command.
- A new file consuming new blocks.
- Priority-1 I/O overtaking background reads.
