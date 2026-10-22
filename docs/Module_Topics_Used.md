# Mini Kernel Simulator — Syllabus Topic Mapping

**Course:** BCSE303L Operating Systems (syllabus version 1.0)

**Status:** Complete implementation of the defined educational simulator scope, extended for Review 2 with eight integrated applications, visible preemptive priority scheduling, and an independent file/I-O request queue. The simulator models concepts; it is not a real operating system.

| Course module | Syllabus concepts represented | Project implementation |
| --- | --- | --- |
| **2 — OS Principles** | PCB, process states, process creation/management, system-call interface, interrupts, thread models | PCB/state transition logs with illegal transitions rejected; create, dispatch, I/O block/wake, terminate; user/kernel trap explanation; many-to-one and one-to-one mapping illustration. Eight applications create/destroy processes and issue real simulated system calls. |
| **3 — Scheduling** | Preemptive/non-preemptive scheduling, resource management, deadlock avoidance/detection/recovery | FCFS, SJF, SRTF, Priority, RR, static MLQ and MLFQ with a reason for every dispatch; Banker safety/request; wait-for cycle and multi-instance detection; recovery with victim policy. Review 2 adds preemptive priority scheduling of application workloads with deterministic tie-breaking. |
| **4 — Concurrency** | IPC, synchronization primitives, semaphores, classical problems, mutual exclusion | Simulated semaphore/mutex; producer-consumer; dining philosophers; Peterson and Bakery traces; message queue and protected shared memory. |
| **5 — Memory Management** | Main-memory allocation, paging, page replacement, segmentation, TLB, working set/thrashing | First/Best/Worst Fit with utilisation and fragmentation metrics on one identical request sequence; FIFO/LRU/Optimal; segmentation translation and bounds; LRU TLB; working-set pressure trace. Application admission uses the contiguous allocator. |
| **6 — Virtualization and File System Management** | File interface, directory structures, allocation, recovery/journaling | In-memory directories/files; create, read, write, append, rename, copy, move, delete, metadata; sequential/direct reads; contiguous/linked/indexed blocks with real block accounting; owner access checks; transaction journal and recovery. Applications perform file work through this file system and the I/O queue. |
| **7 — Storage, Protection and Security** | Disk scheduling and basic access control | FCFS/SSTF/SCAN/C-SCAN head paths; owner-based file access and privileged `admin`. Review 2 adds a separate file-request scheduler (FCFS and priority) and explains how it differs from disk-head movement. Rotational latency and full access-matrix/capability systems are not modeled. |
| **1 — Introduction** | OS functionality, structures, abstractions and trade-offs | Problem statement and module explanations are in the README; architecture models are discussed, not emulated. |

**Out of simulator scope:** actual hypervisors/containers, real kernel calls, hardware devices, real parallel threads, multiprocessor scheduling, distributed filesystems, soft updates, and production-grade authentication/protection. These are outside the implemented demonstration scope and should not be presented as implemented.

*Source syllabus: `OPERATING-SYSTEMS (2).pdf`, BCSE303L, version 1.0, pages 987–988.*
