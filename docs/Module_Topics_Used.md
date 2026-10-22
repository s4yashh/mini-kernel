# Mini Kernel Simulator — Syllabus Topic Mapping

**Course:** BCSE303L Operating Systems (syllabus version 1.0)

**Status:** Complete implementation of the defined educational simulator scope. The simulator models concepts; it is not a real operating system.

| Course module | Syllabus concepts represented | Project implementation |
| --- | --- | --- |
| **2 — OS Principles** | PCB, process states, process creation/management, system-call interface, interrupts, thread models | PCB/state transition logs; create, I/O block/wake, terminate; user/kernel trap explanation; many-to-one and one-to-one mapping illustration. |
| **3 — Scheduling** | Preemptive/non-preemptive scheduling, resource management, deadlock avoidance/detection/recovery | FCFS, SJF, SRTF, Priority, RR, static MLQ and MLFQ; Banker safety/request; wait-for cycle and multi-instance detection; termination recovery. |
| **4 — Concurrency** | IPC, synchronization primitives, semaphores, classical problems, mutual exclusion | Simulated semaphore/mutex; producer-consumer; dining philosophers; Peterson and Bakery traces; message queue and protected shared memory. |
| **5 — Memory Management** | Main-memory allocation, paging, page replacement, segmentation, TLB, working set/thrashing | First/Best/Worst Fit and fragmentation; FIFO/LRU/Optimal; segmentation translation and bounds; LRU TLB; working-set pressure trace. |
| **6 — Virtualization and File System Management** | File interface, directory structures, allocation, recovery/journaling | In-memory directories/files; sequential/direct reads; contiguous/linked/indexed blocks; owner access check; transaction journal and recovery example. |
| **7 — Storage, Protection and Security** | Disk scheduling and basic access control | FCFS/SSTF/SCAN/C-SCAN paths; owner-based file access and privileged `admin` read. Rotational latency and full access-matrix/capability systems are not modeled. |
| **1 — Introduction** | OS functionality, structures, abstractions and trade-offs | Problem statement and module explanations are in the README; architecture models are discussed, not emulated. |

**Out of simulator scope:** actual hypervisors/containers, real kernel calls, hardware devices, real parallel threads, multiprocessor scheduling, distributed filesystems, soft updates, and production-grade authentication/protection. These are outside the implemented demonstration scope and should not be presented as implemented.

*Source syllabus: `OPERATING-SYSTEMS (2).pdf`, BCSE303L, version 1.0, pages 987–988.*
