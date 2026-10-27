# Text Editor

**Kind:** `text-editor` · **Default priority:** 2 · **RAM:** 10 units

Reads a note from the virtual file system, appends an edited line, and writes a
new virtual document. The read blocks the process (WAITING), the scheduler runs
other work, and the completion wakes it (READY).

## Workload

| Step | Action |
| --- | --- |
| 1 | `READ /docs/notes.txt` (request priority 1) |
| 2 | 2 CPU ticks: append a line |
| 3 | `WRITE /apps/notes.txt` (request priority 1) |
| 4 | 1 CPU tick: finish |

## Files

- Input: `/docs/notes.txt` (repository sample copy)
- Output: `/apps/notes.txt`
- Allocation: linked (no index block)

## Expected result

The output contains the original notes plus
`[edited by text-editor]`; the original repository file is unchanged.

## Use it to demonstrate

- READ → WAITING → READY → dispatch.
- Linked allocation for a growing file.
- High-priority I/O: its read and write requests outrank background reads.
