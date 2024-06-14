# MLFQ-Scheduler

Published By Hrishikesha H Kyathsandra
rishihk@iastate.edu

## Project Overview
This project enhances the xv6 operating system by implementing a Multi-Level Feedback Queue (MLFQ) scheduler, designed to efficiently manage process scheduling with dynamic priorities. The MLFQ scheduler coexists with the existing Round-Robin (RR) scheduler, providing flexibility in process scheduling based on specified rules. This implementation includes the integration of system calls to start/stop the MLFQ scheduler and to retrieve scheduling information.

## Features
- **MLFQ Scheduler**: Implements an MLFQ scheduler with dynamic priority adjustments based on the rules.
- **System Calls**: Adds system calls `startMLFQ`, `stopMLFQ`, and `getMLFQInfo` to control the scheduler's operation and gather process scheduling data.
- **Integration with RR Scheduler**: Seamlessly switches between MLFQ and RR schedulers, maintaining system stability and performance.
- **Test Program (`testsyscall.c`)**: A user-level test program to evaluate the MLFQ scheduler's performance under various workloads.

## File Modifications and Implementation (Please look at the files for more details on implementation)

- `syscall.h`: Declared the unique system call numbers  for SYS_startMLFQ, SYS_stopMLFQ and SYS_getMLFQInfo
- `syscall.c`: Declare the prototypes for functions sys_startMLFQ, sys_stopMLFQ and sys_getMLFQInfo. Mapped the respective syscall.h numbers to the prototypes.
- `proc.h`: Added the MLFQInfoReport struct inside proc.h. Added the inQueue, lvl, currLvlTicks, report, maxLvlTicks fields required for the mlfq scheduler function inside the proc struct. Added function signatures of the getters and setters to set the fields we added.
- `user.h`: Declared the signatures of the 3 system calls we are implementing inside user.h, and added the MLFQInfoReport struct.
- `usys.pl`: Added the entries for the 3 system calls we are implementing. This file generates the assembly code needed for the user programs to invoke these system calls
- `sysproc.c`: Implemented the start, stop and getInfo system calls.
- `testsyscall.c`: Added the user program for testing purposes.
- `Makefile`: Added testsyscall under UPROGS so that it can be ran.
- `proc.c`: Setup the Process Node for the DLL with the proc, next and prev pointers. Setup the DLL with a head process node and a tail process node.  
- Added static variables for the flag, levels, and max ticks and then set up their getters and setters so that these can be accessed in sysproc.c. 
- Initialized the mlfq as an array of DLL's. Implemented the enqueue function which adds a process to a queue at a given level. 
- Implemented an unlink and free method which is a helper for the delete function which deletes a node from a queue on a particular level, and then frees that memory. Referred to my 228 DLL notes. 
- Initialized the fields of our mlfq inside allocproc function and then reset them back inside freeproc.
- Added the RR scheduler function given in the pdf.
- Commented out the original scheduler code and added the scheduler code given which switches between RR scheduler and our mlfq scheduler based on the flag.
- Implemented the mlfq scheduler function according to the structure provided.
- Implemented Rule 4: Once a process uses up its time quantum at a its current priority level (regardless of how many times it has given up the CPU), its priority is degraded (i.e., its priority number is
incremented by one and it moves down one queue) if its priority number is not m-1 yet.
- Implemented Rule 5: After a process has been of priority number m-1 (i.e., has stayed at Queue m-1) for n
ticks, its priority number is boosted to 0. 
- Implemted Rule 1: Totally m levels of priority, denoted as 0, 1, ..., and m-1, are defined in the system. Each process has a priority. The lower is the priority number, the higher is the priority. Accordingly, m
Ready Queues, denoted as Queue 0, Queue 1, ..., and Queue m-1, are maintained, where Queue x contains the processes whose priority is x.
- Implemented Rule 2: When a process enters the system, its starting priority is 0. 
- Implemented Rule 3: When the scheduler needs to pick a process to run, it picks the process of higer priority than others. If there are multiple processes of the same highest priority, they should be scheduled
using the Round-Robin policy. Once a process with priority x is scheduled to run, it is assigned to run for a time quantum of 2(x+1) ticks. That is, it can run for the quantum unless it is blocked. 


## Additional System Calls

  This project also extends the xv6 operating system by introducing three new system calls: getppid, ps, and getschedhistory. These system calls enable users to retrieve the parent process ID of the current process, list all processes in the system along with their details, and get the scheduling history of the current process, respectively.

  The implementation involved modifications to several files within the xv6 kernel and user libraries to define, implement, and test the new system calls.

## Modified Files

Kernel Side

    kernel/syscall.h: Added new system call numbers for getppid, ps, and getschedhistory.
    kernel/syscall.c: Included the declarations and function pointers for the new system calls in the syscalls[] array.
    kernel/proc.h: Enhanced the proc struct to include new fields (runCount, systemcallCount, interruptCount, preemptCount, trapCount, sleepCount) necessary for the getschedhistory system call.
    kernel/proc.c: Adjusted to initialize and clean the new fields in the proc struct. Modifications were also made to the scheduler and other process management functions to correctly update these fields.
    kernel/sysproc.c: Implemented the new system calls sys_getppid, sys_ps, and sys_getschedhistory.
    kernel/trap.c: Added code to increment trapCount, systemcallCount, interruptCount, and preemptCount in appropriate sections, particularly in usertrap and kerneltrap functions.

  User Side

    user/usys.pl: Updated the Perl script to generate assembly code for invoking the new system calls from user space.
    user/user.h: Declared the interfaces for the new system calls, allowing user programs to call them.
    user/broadcast.c : Included a sample program to demonstrate and test the new system calls, showcasing their usage and output.
    The broadcast.c sample program has been updated to include calls to the new system calls, demonstrating their functionality. 




