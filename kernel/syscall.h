// System call numbers
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_pipe    4
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_fstat   8
#define SYS_chdir   9
#define SYS_dup    10
#define SYS_getpid 11
#define SYS_sbrk   12
#define SYS_sleep  13
#define SYS_uptime 14
#define SYS_open   15
#define SYS_write  16
#define SYS_mknod  17
#define SYS_unlink 18
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21

// Project 1B
// Adding system calls STEP 1
// to add new system calls on the kernel side, we define their identifier in here. 
// the system call numbers are used as an index into the syscalls[] array of function pointers inside syscall.c
// we basically assign the system calls we add a unique number, allowing the kernel to identify it when called. 
#define SYS_getppid 22
#define SYS_ps 23
#define SYS_getschedhistory 24

// Project 1C
// Setting the unique indexes for the requiremed system calls for a MLFQ scheduler. This will be used as an index into the syscalls array in syscall.c
#define SYS_startMLFQ 25
#define SYS_stopMLFQ 26
#define SYS_getMLFQInfo 27





