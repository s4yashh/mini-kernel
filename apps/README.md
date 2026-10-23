# Application definitions

Each application used by the browser simulator has a directory here with:

- `app.json` — machine-readable metadata used by `apps/app-manifest.json` and the
  frontend (name, description, workload steps, defaults, files, operations).
- `README.md` — a human-readable explanation of the workload, files, expected
  output and what it demonstrates.

| Directory | Kind | Priority | Input | Output |
| --- | --- | --- | --- | --- |
| `calculator/` | `calculator` | 1 | — | `/apps/calculator.txt` |
| `text-editor/` | `text-editor` | 2 | `/docs/notes.txt` | `/apps/notes.txt` |
| `counter/` | `counter` | 3 | — | `/apps/counter.txt` |
| `file-search/` | `file-search` | 2 | `/docs/system.log` | `/apps/search.txt` |
| `file-analyzer/` | `file-analyzer` | 3 | `/docs/numbers.csv` | `/apps/analysis.txt` |
| `file-copy/` | `file-copy` | 1 | `/docs/report.txt` | `/apps/report-copy.txt` |
| `background-indexer/` | `indexer` | 3 | five files in `/docs` | `/apps/index.txt` |
| `system-monitor/` | `system-monitor` | 2 | — | `/apps/monitor.txt` |

`app-manifest.json` is the single catalogue the frontend loads. It also lists the
repository sample files and their virtual paths. The loader validates every path
and refuses to start the simulation if a file is missing; it never invents
content.

The **workload** entries describe what the shared JavaScript kernel executes. The
workload implementation lives in `web/index.html` (the `KERNEL-CORE` block); the
JSON files are the metadata source of truth for the catalogue and defaults.

## Adding an application

1. Add a directory with `app.json` and `README.md`.
2. Add an entry to `app-manifest.json`.
3. Implement its steps in the kernel core and expose the kind in `KIND_INFO`.
4. Add it to the self-test fixture expectations if it has a fixed result.
