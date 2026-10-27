# File Analyzer

**Kind:** `file-analyzer` · **Default priority:** 3 · **RAM:** 6 units

Reads `numbers.csv` and computes line, word and character counts from the actual
bytes in the virtual file system. The sample CSV has a header plus five rows, so
the expected output is `lines=6 words=6 characters=34`.

## Workload

| Step | Action |
| --- | --- |
| 1 | `READ /docs/numbers.csv` (request priority 3) |
| 2 | 3 CPU ticks: count lines/words/characters |
| 3 | `WRITE /apps/analysis.txt` (request priority 3) |
| 4 | 1 CPU tick: finish |

## Files

- Input: `/docs/numbers.csv`
- Output: `/apps/analysis.txt`
- Allocation: linked

## Use it to demonstrate

- Statistics that depend on the file's real contents (edit the CSV and rerun).
- Background (priority-3) I/O behaviour.
- Two different file-dependent applications competing for the device.
