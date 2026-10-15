# Mini Kernel Simulator — Syllabus Modules and Topics

**Course:** BCSE303L Operating Systems  
**Stage:** Review 1 (approximately 50% implemented)

## Topics used in the current project

| Module No. and name | Syllabus topic used | Project implementation |
| --- | --- | --- |
| **2 — OS Principles** | **Processes; Process Control Block; Ready List** | PCB fields, process states and transition log. |
| **3 — Scheduling** | **Process scheduling; pre-emptive and non-pre-emptive CPU scheduling** | FCFS, SJF, SRTF, Priority and Round Robin; Gantt chart and metrics. |
| **3 — Scheduling** | **Deadlocks; resource allocation and management; deadlock avoidance** | Banker's safety and resource-request algorithms. |
| **4 — Concurrency** | **Synchronization; semaphores; classical synchronization problems; dining philosophers problem** | Simulated semaphore/mutex, producer–consumer and dining philosophers. The fixed solution uses resource ordering, not a monitor. |
| **5 — Memory Management** | **Demand paging; page faults; page replacement** | FIFO, LRU and Optimal replacement with frame-by-frame logs. |
| **7 — Storage Management, Protection and Security** | **Disk scheduling algorithms — seek time** | FCFS, SSTF, SCAN and C-SCAN; service order and head movement. Rotational latency is not modeled. |

**Module 1 — Introduction** provides background concepts (*OS functionality; processes and resources*), but is not a separate implemented algorithm. **Modules 6 and 8 are not used in the current build.**

## Topics planned for the next 50%

| Module No. | Syllabus topic | Planned addition |
| --- | --- | --- |
| **2** | Process creation and management | Simulated creation, termination and I/O-related WAITING/READY transitions. |
| **3** | Deadlock detection and recovery | Detect blocked resource states and demonstrate recovery. |
| **5** | Memory allocation strategies | First Fit, Best Fit and Worst Fit with fragmentation display. |
| **6** | File system interface and implementation | Simple in-memory files, directories and block allocation. |

*Source: “OPERATING-SYSTEMS (2).pdf”, BCSE303L syllabus version 1.0, Modules 1–8, pages 987–988.*
