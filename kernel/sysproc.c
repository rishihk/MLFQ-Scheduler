#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"


uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Project 1B
// Adding new system calls STEP 6
// functions that implement the system calls are normally placed in this file
// below are the implementations of our system calls.

uint64
sys_getppid(void)
{
  // accessed the current process by using myproc function
  struct proc *currProc = myproc();
  // accessed currentProcs parent field
  struct proc *parentProc = currProc->parent;
  if (parentProc == 0)
  {
    return 0;
  }
  // returned parentProcs pid
  return parentProc->pid;
}

extern struct proc proc[NPROC]; // declare array proc which is defined in proc.c already
uint64
sys_ps(void)
{
  // define the ps_struct for each process and ps[NPROC] for all processes
  struct ps_struct
  {
    int pid;
    int ppid;
    char state[10];
    char name[16];
  } ps[NPROC];

  int numProc = 0; // variable keeping track of the number of processes in the system

  for (int i = 0; i < NPROC; i++)
  {
    struct proc *p = &proc[i]; // get the process at index i
    if (p->state != UNUSED) // if the process is not UNUSED, then we add it to the ps array
    {
      ps[numProc].pid = p->pid; // set the pid of the process
      if (p->parent)
      {
        ps[numProc].ppid = p->parent->pid; // set the ppid of the process
      }
      else
      {
        ps[numProc].ppid = 0; // No parent process
      }

      // Define an array of strings to represent the process states
      const char *stateStrings[] = {"UNUSED", "USED", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE"};
      const int numberOfStates = sizeof(stateStrings) / sizeof(stateStrings[0]); // Calculate the number of states

      // Convert the state to a string
      const char *stateStr = "UNKNOWN"; // Default to UNKNOWN
      if (p->state >= 0 && p->state < numberOfStates) // If the state is within the range of the array
      {
        stateStr = stateStrings[p->state]; // Set the state string
      }

      strncpy(ps[numProc].state, stateStr, sizeof(ps[numProc].state) - 1); // Copy the state string to the ps struct
      ps[numProc].state[sizeof(ps[numProc].state) - 1] = '\0'; // Null-terminate the state string

      safestrcpy(ps[numProc].name, p->name, sizeof(ps[numProc].name)); // set the name of the process

      numProc++; // increment the number of processes
    }
  }

  // save the address of the user space argment to arg_addr
  uint64 arg_addr;
  argaddr(0, &arg_addr);
  // copy array ps to the saved address
  if (copyout(myproc()->pagetable,
              arg_addr,
              (char *)ps,
              numProc * sizeof(struct ps_struct)) < 0)
    return -1;

  // return numProc as well
  return numProc;
}

uint64
sys_getschedhistory(void)
{
  // define the struct for reporting scheduling history
  struct sched_history
  {
    int runCount;
    int systemcallCount;
    int interruptCount;
    int preemptCount;
    int trapCount;
    int sleepCount;
  } my_history;

  // To do: retrieve the current process’ information to fill my_history
  struct proc *p = myproc();

  my_history.runCount = p->runCount;               // set the runCount of the process
  my_history.systemcallCount = p->systemcallCount; // set the systemcallCount of the process
  my_history.interruptCount = p->interruptCount;   // set the interruptCount of the process
  my_history.preemptCount = p->preemptCount;       // set the preemptCount of the process
  my_history.trapCount = p->trapCount;             // set the trapCount of the process
  my_history.sleepCount = p->sleepCount;           // set the sleepCount of the process

  // save the address of the user space argment to arg_addr
  uint64 arg_addr;
  argaddr(0, &arg_addr);
  // copy the struct my_history to the saved address
  if (copyout(p->pagetable,
              arg_addr,
              (char *)&my_history,
              sizeof(struct sched_history)) < 0)
    return -1;
  // To do: return the pid as well
  return p->pid; // return the pid of the process
}

// could have also just defined the 3 variables in proc.c with the extern keyword here. extern is used to declare a variable that is defined in another file. It is used to share state between files. Kind of like atoms in React. But I like the static var, getter, setter approach better to share state since its like Java.

//Project 1C
// argint is a function used in xv6 to retrieve the integer argument provided by a user program. It allows the kernel to safely access arguments passed from the user space.
uint64 sys_startMLFQ(void) { // This function is used to start the MLFQ scheduler
    if(getMLFQFlag() == 1) { // If MLFQ is already enabled
        return -1; // Return -1 since its already started
    }
    int numLevels;
    argint(0, &numLevels); // Get the number of levels passed by the user
    int maxTicks;
    argint(1, &maxTicks); // Get the maximum ticks at the bottom level passed by the user

    setLevelsInMLFQ(numLevels); // Set the number of levels
    setMaxTicksAtBottom(maxTicks); // Set the maximum ticks at the bottom level
    setMLFQFlag(1); // Set the MLFQ flag to 1 to enable MLFQ
    return 0; // Return success
}

// this function is used to stop the MLFQ scheduler
uint64 sys_stopMLFQ(void) {
    if(getMLFQFlag() == 0) { // If MLFQ is not enabled
        return -1; // Return -1 since its not started
    }
    setMLFQFlag(0); // Set the MLFQ flag to 0
    return 0; // Return success
}

// this function gives the MLFQ information
uint64 sys_getMLFQInfo(void) {

  uint64 dest;  // destination address
  argaddr(0, &dest);  // get the destination address from the user space
  struct proc *curr_proc = myproc();  // get the current process
  char *source = (char *) &(curr_proc->report);  // get the source address
  int successFullyCopied = copyout(curr_proc->pagetable, dest, source, sizeof(struct MLFQInfoReport));  // copy the source to the destination
  if (successFullyCopied < 0) { // if the copy was not successful
    return -1;  // return -1
  }
  return 0; // return 0
}



