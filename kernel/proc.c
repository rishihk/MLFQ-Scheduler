#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC]; // array contains the number of proc structs. Contains information of the processes in the system. (64)

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    p->kstack = KSTACK((int)(p - proc));
  }
}

// Project 1C

// For the MLFQ scheduler, to run, m Ready Queues should be maintained in the kernel space. Each of the queues can be implemented as a DLL where each element contains...
struct MLFQProcessNode // this represents each process in a queue, and how they are linked to the process in front of them and behind them
{
  struct proc *pcb; // Information of a process on the queue. A Pointer to the process struct.
  struct MLFQProcessNode *next; // Pointer to the next node in the queue.
  struct MLFQProcessNode *prev; // Pointer to the previous node in the queue.
}; // As specified

// Now that we have a struct to represent each process in the queue, we can create the queue itself. The queue will be implemented as a doubly linked list. The first process in the queue will be the head of the list, and the last process in the queue will be the tail of the list. Now we have a single queue of processes ready.
struct MLFQQueuePerLevel
{
  struct MLFQProcessNode *head; // Pointer to the first node in the queue.
  struct MLFQProcessNode *tail; // Pointer to the last node in the queue.
}; // As specified

static int mlfqFlag = 0;         // Flag to enable MLFQ scheduler. static so that we can send it to sysproc
static int levelsInMlfq = 1;     // Number of levels in MLFQ default to 1. static so that we can send it to sysproc
static int maxTicksAtBottom = 1; // Max ticks at bottom level default to 1. static so that we can send it to sysproc

// Function to set the MLFQ flag
void setMLFQFlag(int flag)
{
  mlfqFlag = flag;
}

// Function to get the MLFQ flag
int getMLFQFlag(void)
{
  return mlfqFlag;
}

// Function to set the number of levels in MLFQ
void setLevelsInMLFQ(int levels)
{
  levelsInMlfq = levels;
}

// Function to get the number of levels in MLFQ
int getLevelsInMLFQ(void)
{
  return levelsInMlfq;
}

// Function to set the max ticks at bottom level
void setMaxTicksAtBottom(int ticks)
{
  maxTicksAtBottom = ticks;
}

// Function to get the max ticks at bottom level
int getMaxTicksAtBottom(void)
{
  return maxTicksAtBottom;
}

// We have a process, a queue of processes, in proc.h we can create m Ready Queues. Each queue will be a level in the MLFQ scheduler. The highest level will be the first queue, and the lowest level will be the last queue. The queues will be stored in an array of queues. The array holds all the different priority level queues in the MLFQ.
struct MLFQQueuePerLevel mlfqQueues[MLFQ_MAX_LEVEL];

// now we have an empty mlfq ready to be used. A process needs to be added to a queue with a given level. For this we use the enqueue function which inserts a process pointed to by pointer proc to the queue with priority level 'level'.
void mlfq_enqueue(int level, struct proc *proc) // addes a process to the end of a specific priority level queue.
{
  // kalloc() is a function in xv6 that allocates a block of memory of certain size. It is used to allocate memory for the node. It returns a pointer to the beginning of the block of memory. intended for use in the kernel space and its data structures.

  struct MLFQProcessNode *procNode = (struct MLFQProcessNode *)kalloc(); // allocate memory for the new node

  // a queue is FIFO, so the new node will be added to the end of the queue. The new node will be the tail of the queue.
  procNode->pcb = proc; // assign the given process to the new node.
  procNode->next = 0; // new nodes next pointer will be null because it is the last node in the queue.

  struct MLFQQueuePerLevel *targetQueue = &mlfqQueues[level];  // access the queue at the given level.

  if (targetQueue->head == 0) // if the queue is empty, the new node will be the head and tail of the queue.
  {
    procNode->prev = 0;  // new nodes previous pointer will be null because it is the first node in the queue.
    targetQueue->head = procNode; // the head of the queue will be the new node.
    targetQueue->tail = procNode; // the tail of the queue will be the new node.
  }
  else
  {
    targetQueue->tail->next = procNode; // new node will be appended to the tail of the chosen queue when non empty.
    procNode->prev = targetQueue->tail; // the previous pointer of the new node will be the current tail of the queue.
    targetQueue->tail = procNode; // the new node will be the new tail of the queue.
  }
}

// unlinks a process node from the queue and frees the memory allocated for the process node.
void unlink_and_free(struct MLFQQueuePerLevel *queue, struct MLFQProcessNode *node){ 

  if(node->prev!=0){ // this means its not the head
    node->prev->next = node->next; // unlink the node from the previous node
  }
  else{  // this means we are at the head
    queue->head = node->next; // set the head of the queue to the next node
  }

  if(node->next!=0){ // this means we are not at the tail
    node->next->prev = node->prev; // unlink the node from the next node
  }
  else{ // this means we are at the tail
    queue->tail = node->prev; // set the tail of the queue to the previous node
  }

  // kfree() is a function in xv6 that deallocates a block of memory. It is used to deallocate memory for the node. It is used to free memory that was previously allocated using kalloc(). makes the memory available for future use.
  kfree((void *)node);  // free the memory allocated for the node

}

// delete process pointed to by pointer proc from the queue with priority level 'level'.
void mlfq_delete(int level, struct proc *proc)
{
  struct MLFQQueuePerLevel *targetQueue = &mlfqQueues[level]; // access the queue at the given level.
  struct MLFQProcessNode *procNode = targetQueue->head; // start at the head of the queue

  while(procNode != 0){ // as long as theres a node, we keep iterating to find the node we want to delete

    if(procNode->pcb == proc){ // if we found the node
      unlink_and_free(targetQueue, procNode); // unlink it from the queue and free the memory
      break; // return
    }
    procNode = procNode->next; // move to the next node
  }
}

void MLFQ_scheduler(struct cpu *c)
{
  struct proc *p = 0; // p is the process scheduled to run; initially it is none.

  while (mlfqFlag) // each iteration is run everytime when the scheduler gains control.
  {
    
    if (p > 0 && (p->state == RUNNABLE || p->state == RUNNING)){
      // if the current process is still runnable

      /* Rule 4 */

      // incrememnt the tick count of p on the current queue
      p->currLvlTicks++;

      // also store the tick count of proc on the current queue in the report
      p->report.tickCounts[p->lvl] = p->currLvlTicks;

      if (p->currLvlTicks >= 2 * (p->lvl + 1)){  // 1. check if p's time quantum for the current queue expired or not.

        // (2)  move p to the queue below it (if p is not at the bottom queue yet)

        if (p->lvl < levelsInMlfq - 1) // check if p is not at the bottom queue yet
        {
          mlfq_delete(p->lvl, p); // if it was not at the bottom queue yet, we remove it from the current queue

          // increment the level of p
          p->lvl++;

          p->currLvlTicks = 0; // reset the ticks on the current queue level since it changed

          mlfq_enqueue(p->lvl, p); // add the process to the queue below it
        }

        //(3) p is set to 0 (to indicate another process should be find to run next) since its time quantum expired and it was at the bottom queue
        p = 0;
      }
    }

    // Rule 5: If there are any processes in the bottom level, we check if they have reached the maximum number of ticks allowed at the bottom level. If they have, we perform a priority boost.

    //increment the tick counts for the processes at the bottom queue and
    //move each process who has stay there for n ticks to the top queue

    // access the bottom level queue since we will be checking if there are any processes in the bottom level
    struct MLFQQueuePerLevel *bottomLvlQueue = &mlfqQueues[levelsInMlfq - 1]; 

    // iterate starting from the head of the bottom level queue.
    if (bottomLvlQueue->head != 0) // if there are any processes in the bottom level queue
    {
      struct MLFQProcessNode *procNode = bottomLvlQueue->head; // start at the head of the queue
      
      struct proc *bottomproc = procNode->pcb; // get the process at the head of the queue

      // iterate through the queue to check if any process has reached the maximum number of ticks allowed at the bottom level
      while (procNode != 0)
      {
        bottomproc->maxLvlTicks++; // increment the max level ticks of the process

        if (bottomproc->maxLvlTicks >= maxTicksAtBottom) // if the process has reached the maximum number of ticks allowed at the bottom level
        {
          mlfq_delete(bottomproc->lvl, bottomproc); // remove the process from the bottom level

          bottomproc->maxLvlTicks = 0; // reset the max level ticks of the process
          bottomproc->currLvlTicks = 0; // reset the current level ticks of the process
          bottomproc->lvl = 0; // set the level of the process to 0 to boost its priority

          mlfq_enqueue(0, bottomproc); // add the process to the top level queue
        }
        procNode = procNode->next; // move to the next node
      }
    }

    // Rule 1: Check for any processes that haven't been queued by the MLFQ scheduelr as of yet.
    //add new processes which havent been queued on any level yet to queue 0 (highest priority)

    for (int i = 0; i < NPROC; i++) // iterate through all the processes
    {
      struct proc *candidateProc = &proc[i]; // get the process at index i

      // we check that the process is runnable and hasn't been queued by the MLFQ scheduler as of yet
      if (candidateProc->state == RUNNABLE && candidateProc->inQueue == 0)
      {
        candidateProc->inQueue = 1; // set the inQueue flag to 1 to indicate that the process has been queued by the MLFQ scheduler

        /* Rule 2 */

        mlfq_enqueue(0, candidateProc); // add the process to the top level queue

      }
    }

    if (p == 0){ // need to find another process to run

      int foundProcess = 0; // flag to indicate if a process has been found to run
      // find the RUNNABLE process with the highest priority to run, and set p to point to it

      for (int i = 0; i < levelsInMlfq && !foundProcess; i++) // for each level in the mlfq, we start from the highest priority level since we want to find the process with the highest priority to run
      {

        /* Rule 3 */

        struct MLFQQueuePerLevel *currQueue = &mlfqQueues[i];  // access the queue at the given level

        struct MLFQProcessNode *procNode = currQueue->head; // start at the head of the queue

        while (procNode != 0 && !foundProcess) // for each process in the queue
        {
          if (procNode->pcb->state == RUNNABLE) // if the process is runnable
          {
            p = procNode->pcb; // set p to point to the process
            foundProcess = 1; // set the flag to indicate that a process has been found to run
          }
          else
          {
            procNode = procNode->next; // move to the next node
          }
        }
      }
    }
    if (p != 0)
    {
      acquire(&p->lock);
      p->state = RUNNING;
      c->proc = p;
      swtch(&c->context, &p->context);
      c->proc = 0;
      release(&p->lock);
    }
  }
}

// Round Robin scheduler which is used when the MLFQ flag is not set. It schedules the processes in a round robin fashion. As requried by Project 1C.
void RR_scheduler(struct cpu *c)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == RUNNABLE)
    {
      // Switch to chosen process. It is the process's job
      // to release its lock and then reacquire it
      // before jumping back to us.
      p->state = RUNNING;
      c->proc = p;
      swtch(&c->context, &p->context);
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&p->lock);
  }
}

// Commented out the original default scheduler function on line 794 below and added a new modified scheduler function to handle MLFQ scheduling. It chooses between MLFQ and RR scheduling based on the flag set.
void scheduler(void)
{
  struct cpu *c = mycpu(); // Get the current CPU.

  c->proc = 0; // No current process running on this CPU.
  for (;;)   // Infinite loop to keep the scheduler running.
  {
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on(); // Enable interrupts.

    if (mlfqFlag)
    {
      MLFQ_scheduler(c); // Run the MLFQ scheduler if the flag is set.
    }
    else
    {
      RR_scheduler(c); // Run the Round Robin scheduler if the flag is not set.
    }
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
// Function that returns a pointer to the proc struct of the current process. Useful to retrieve the current process.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == UNUSED)
    {
      goto found;
    }
    else
    {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  // Project 1C
  // alloc is called when a new process is created. We need to initialize the values of the fields we added to the proc struct.
  for (int i = 0; i < MLFQ_MAX_LEVEL; i++)
  {
    p->report.tickCounts[i] = 0; // initialize the tick counts of the process
  }
  // initialize the values of the fields we added to the proc struct.
  p->inQueue = 0; 
  p->lvl = 0;
  p->currLvlTicks = 0;
  p->maxLvlTicks = 0;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.

// the proc structs in array proc[NPROC]  should be reusable after the process that use them have terminated
// cleans the values of all fields in the structure

static void
freeproc(struct proc *p)
{
  if (p->trapframe)
    kfree((void *)p->trapframe);
  p->trapframe = 0;
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;

  // reset and clean the values which we included as well below
  p->runCount = 0;
  p->sleepCount = 0;
  p->systemcallCount = 0;
  p->trapCount = 0;
  p->interruptCount = 0;
  p->preemptCount = 0;

  // free is called when a process is terminated. We need to reset the values of the fields we added to the proc struct.
  // freeing up mlfq specific fields which we added to the proc struct.
  for (int i = 0; i < MLFQ_MAX_LEVEL; i++)
  {
    p->report.tickCounts[i] = 0; // reset the tick counts of the process
  }
  // reset the values of the fields we added to the proc struct.
  p->inQueue = 0;
  p->lvl = 0;
  p->maxLvlTicks = 0;
  p->currLvlTicks = 0;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE,
               (uint64)trampoline, PTE_R | PTE_X) < 0)
  {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE,
               (uint64)(p->trapframe), PTE_R | PTE_W) < 0)
  {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
    0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
    0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// Set up first user process.
void userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;     // user program counter
  p->trapframe->sp = PGSIZE; // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0)
  {
    if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0)
    {
      return -1;
    }
  }
  else if (n < 0)
  {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++)
  {
    if (pp->parent == p)
    {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{
  struct proc *p = myproc();

  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd])
    {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);

  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (pp = proc; pp < &proc[NPROC]; pp++)
    {
      if (pp->parent == p)
      {
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if (pp->state == ZOMBIE)
        {
          // Found one.
          pid = pp->pid;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                   sizeof(pp->xstate)) < 0)
          {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || killed(p))
    {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock); // DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.

// implements the xv6 scheduler. Default is round robin
// it checks the proc structs in array proc[NPROC] and picks one process that is in state RUNNABLE to run next
// we need to increment runCount after such a process is picked and before the context switch happens in the function.

// void
// scheduler(void)
// {
//   struct proc *p;
//   struct cpu *c = mycpu();

//   c->proc = 0;
//   for(;;){
//     // Avoid deadlock by ensuring that devices can interrupt.
//     intr_on();

//     for(p = proc; p < &proc[NPROC]; p++) {
//       acquire(&p->lock);
//       if(p->state == RUNNABLE) {
//         // Switch to chosen process.  It is the process's job
//         // to release its lock and then reacquire it
//         // before jumping back to us.
//         p->state = RUNNING;
//         c->proc = p;

//         // increment the runcount of the proc before the switch.
//         p->runCount++;

//         swtch(&c->context, &p->context);

//         // Process is done running for now.
//         // It should have changed its p->state before coming back.
//         c->proc = 0;
//       }
//       release(&p->lock);
//     }
//   }
// }

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first)
  {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.

// called when the currently running process cannot proceed as waiting for a certain event to complete.
// current process voluntarily gives up the CPU, so we must increment the curr process sleepCount before sched() is invoked

void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock); // DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  // increment the sleepCount field of the curr proc before sched
  p->sleepCount++;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p != myproc())
    {
      acquire(&p->lock);
      if (p->state == SLEEPING && p->chan == chan)
      {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->pid == pid)
    {
      p->killed = 1;
      if (p->state == SLEEPING)
      {
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst)
  {
    return copyout(p->pagetable, dst, src, len);
  }
  else
  {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src)
  {
    return copyin(p->pagetable, dst, src, len);
  }
  else
  {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.

// this function prints the information of the current process. Helpful for implementation of system call ps.

void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [USED] "used",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
