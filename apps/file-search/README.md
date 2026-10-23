# File Search

**Kind:** `file-search` · **Default priority:** 2 · **RAM:** 7 units

Reads `system.log`, counts the occurrences of `ERROR` in the actual virtual
contents, and writes a short report. Arriving at t=2 shows a process entering the
READY queue while older work is running.

## Workload

| Step | Action |
| --- | --- |
| 1 | `READ /docs/system.log` (request priority 2) |
| 2 | 2 CPU ticks: count matches |
| 3 | `WRITE /apps/search.txt` (request priority 2) |
| 4 | 1 CPU tick: finish |

## Files

- Input: `/docs/system.log` (repository sample copy)
- Output: `/apps/search.txt`
- Allocation: indexed

## Expected result

The sample log contains two `ERROR` lines, so the result is
`found 2 occurrence(s) of ERROR` and the file contains a `2 match(es)` line.
If you edit the log first, the count changes — nothing is hardcoded.

## Use it to demonstrate

- File-dependent CPU work: the count cannot be computed before the READ completes.
- READ blocking and resumption.
- Actual content search rather than a prepared answer.
