# Repository sample files

These are the real, committed files the browser simulator loads into its virtual
file system. They are **inputs**: the frontend copies them into `/docs` in memory
and never writes back to these files.

| File | VFS path | Used by | Purpose |
| --- | --- | --- | --- |
| `notes.txt` | `/docs/notes.txt` | Text Editor | short editable note |
| `report.txt` | `/docs/report.txt` | File Copy Utility | document to copy |
| `numbers.csv` | `/docs/numbers.csv` | File Analyzer | CSV statistics (6 lines, 34 characters) |
| `source.c` | `/docs/source.c` | Background Indexer | small C source |
| `config.json` | `/docs/config.json` | Background Indexer | valid JSON configuration |
| `system.log` | `/docs/system.log` | File Search | log with two ERROR lines |
| `style.txt` | `/docs/style.txt` | Background Indexer | one-line style guide |

The expected application results are derived from these contents, for example:

- File Search reports **2** occurrences of `ERROR` in `system.log`.
- File Analyzer reports **lines=6 words=6 characters=34** for `numbers.csv`.
- File Copy produces a byte-identical copy of `report.txt`.

If you edit a file in the browser, only the in-memory virtual copy changes; these
repository files remain untouched. The self-tests assert exactly that.
