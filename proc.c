#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  struct proc *pq[NPRIORITY][NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// Marks a process as runnable and pushes it onto the priority queue for
// its priority level.  Must be called with ptable.lock held.
static void
pushproc(struct proc *p)
{
  struct proc **pq;
  int i;

  if(p == 0 || p->stat.epriority < 0 || p->stat.epriority >= NPRIORITY)
    panic("pushproc: bad process or priority");

  pq = ptable.pq[p->stat.epriority];
  for(i = 0; i < NPROC && pq[i] != 0; i++){
    // If the process is already in the queue, don't push it.
    if(pq[i] == p){
      cprintf("found p in pq\n");
      return;
    }
  }

  if(i == NPROC)
    panic("pushproc: full process queue");

  p->stat.state = P_RUNNABLE;
  p->stat.tenter = ticks;
  if(p->stat.tfirst == -1)
    p->stat.tfirst = p->stat.tenter;
  pq[i] = p;
}

// Removes a process from the priority queue for its priority level.
// Must be called with ptable.lock held.  Does not change the state of
// the process.
static void
popproc(struct proc *p)
{
  struct proc **pq;
  int i;
  int shift;

  if(p == 0 || p->stat.epriority < 0 || p->stat.epriority >= NPRIORITY)
    panic("popproc: bad process or priority");

  pq = ptable.pq[p->stat.epriority];
  for(i = 0, shift = 0; i < NPROC && pq[i] != 0; i++){
    if(pq[i] == p)
      shift = 1;
    if(shift){
      if(i == NPROC - 1)
        pq[i] = 0;
      else{
        pq[i] = pq[i + 1];
      }
    }
  }
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->stat.state == P_UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->stat.state = P_EMBRYO;
  p->stat.pid = nextpid++;
  p->stat.status = 0;
  // Start at the highest priority
  p->stat.rpriority = 0;
  p->stat.epriority = 0;
  p->stat.tcreate = ticks;
  p->stat.texit = -1;
  p->stat.tfirst = -1;
  p->stat.tlast = -1;
  p->stat.tenter = -1;
  p->stat.atready = 0;
  p->stat.atwait = 0;
  p->stat.nready = 0;
  p->stat.nwait = 0;
  p->stat.nyield = 0;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->stat.state = P_UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->stat.sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->stat.parent = p->stat.pid; // To users, init is its own parent
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->stat.name, "initcode", sizeof(p->stat.name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  pushproc(p);

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->stat.sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->stat.sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->stat.sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->stat.state = P_UNUSED;
    return -1;
  }
  np->stat.sz = curproc->stat.sz;
  np->parent = curproc;
  np->stat.parent = curproc->stat.pid;
  *np->tf = *curproc->tf;
  // Inherit the priority of the parent process
  np->stat.rpriority = curproc->stat.rpriority;
  np->stat.epriority = curproc->stat.rpriority;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->stat.name, curproc->stat.name,
      sizeof(curproc->stat.name));

  pid = np->stat.pid;

  acquire(&ptable.lock);

  pushproc(np);

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(int status)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Store the provided exit status and the time of the exit.
  curproc->stat.status = status;
  curproc->stat.texit = ticks;

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->stat.state == P_ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->stat.state = P_ZOMBIE;

  // Dump procstat
  cprintf("+-----------------------------\n");
  cprintf("| PROCESS EXIT-TIME STATISTICS\n");
  cprintf("|   pid       = %d\n", curproc->stat.pid);
  cprintf("|   parent    = %d\n", curproc->stat.parent);
  cprintf("|   name      = %s\n", curproc->stat.name);
  cprintf("|   state     = %d\n", curproc->stat.state);
  cprintf("|   status    = %d\n", curproc->stat.status);
  cprintf("|   sz        = %d\n", curproc->stat.sz);
  cprintf("|   rpriority = %d\n", curproc->stat.rpriority);
  cprintf("|   epriority = %d\n", curproc->stat.epriority);
  cprintf("|   tcreate   = %d\n", curproc->stat.tcreate);
  cprintf("|   texit     = %d\n", curproc->stat.texit);
  cprintf("|   tfirst    = %d\n", curproc->stat.tfirst);
  cprintf("|   tlast     = %d\n", curproc->stat.tlast);
  cprintf("|   tenter    = %d\n", curproc->stat.tenter);
  cprintf("|   atready   = %d\n", curproc->stat.atready);
  cprintf("|   atwait    = %d\n", curproc->stat.atwait);
  cprintf("|   nready    = %d\n", curproc->stat.nready);
  cprintf("|   nwait     = %d\n", curproc->stat.nwait);
  cprintf("|   nyield    = %d\n", curproc->stat.nyield);
  if(curproc->stat.texit > 0){
    cprintf("|   turn      = %d\n",
        curproc->stat.texit - curproc->stat.tcreate);
    if(curproc->stat.nyield > 0){
      cprintf("|   norm turn = %d\n",
          (curproc->stat.texit - curproc->stat.tcreate) /
          curproc->stat.nyield);
    }
  }
  cprintf("+-----------------------------\n");

  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(int *status)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->stat.state == P_ZOMBIE){
        // Found one.
        pid = p->stat.pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->stat.pid = 0;
        p->parent = 0;
        p->stat.name[0] = 0;
        p->killed = 0;
        p->stat.state = P_UNUSED;
        if(status != 0){
          *status = p->stat.status;
        }
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Wait for a process with the given pid to exit.
// Return -1 if the pid doesn't exist or if there was an error.
int
waitpid(int pid, int *status, int options)
{
  struct proc *p;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->stat.pid == pid){
      // Once the process is found, sleep while waiting for it to become a
      // zombie.  In case the ZOMBIE state is consumed somewhere else, also
      // check for an UNUSED state to ensure that we do not wait forever.
      while(p->stat.state != P_UNUSED && p->stat.state != P_ZOMBIE){
        sleep(curproc, &ptable.lock);
      }
      if(p->stat.state == P_ZOMBIE){
        // If we caught the process in a ZOMBIE state, clean up.
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->parent = 0;
        p->stat.name[0] = 0;
        p->killed = 0;
        p->stat.state = P_UNUSED;
      }
      // Either way, reset the pid and copy the status out (if requested).
      // Even if we caught the process in an UNUSED state, only the alloc and
      // exit functions modify the status, so it should still be the one set
      // when the process exited and is safe to copy.
      p->stat.pid = 0;
      if(status != 0){
        *status = p->stat.status;
      }
      release(&ptable.lock);
      return pid;
    }
  }

  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  int priority;
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process priority queues looking for process to run.
    acquire(&ptable.lock);
    for(priority = 0; priority < NPRIORITY; priority++){
      p = ptable.pq[priority][0];
      if(p == 0)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      popproc(p); // Remove from the priority queue while running.
      p->stat.tlast = ticks;
      p->stat.atready = (p->stat.tenter + p->stat.nready *
          p->stat.atready) / (p->stat.nready + 1);
      p->stat.nready++;
      p->stat.state = P_RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      break;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->stat.state == P_RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p;
  acquire(&ptable.lock);  //DOC: yieldlock
  p = myproc();
  if(p->stat.epriority < NPRIORITY - 1)
    p->stat.epriority++; // Reduce priority after being preempted.
  p->stat.nyield++;
  pushproc(p);
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->stat.tenter = ticks;
  p->stat.state = P_SLEEPING;
  if(p->stat.epriority > 0)
    p->stat.epriority--; // Increase priority when waiting.

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->stat.state == P_SLEEPING && p->chan == chan){
      p->stat.atwait = (p->stat.tenter + p->stat.nwait * p->stat.atwait)
        / (p->stat.nwait + 1);
      p->stat.nwait++;
      pushproc(p);
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->stat.pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->stat.state == P_SLEEPING)
        pushproc(p);
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [P_UNUSED]    "unused",
  [P_EMBRYO]    "embryo",
  [P_SLEEPING]  "sleep ",
  [P_RUNNABLE]  "runble",
  [P_RUNNING]   "run   ",
  [P_ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->stat.state == P_UNUSED)
      continue;
    if(p->stat.state >= 0 && p->stat.state < NELEM(states) &&
        states[p->stat.state])
      state = states[p->stat.state];
    else
      state = "???";
    cprintf("%d %s %s %d %d", p->stat.pid, state, p->stat.name,
        p->stat.rpriority, p->stat.epriority);
    if(p->stat.state == P_SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int
getpriority(int pid)
{
  struct proc *p;
  int priority;

  acquire(&ptable.lock);
  priority = -1;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->stat.pid == pid){
      // Only retrieve the priority if the process is found and is not a
      // zombie.
      if(p->stat.status != P_ZOMBIE)
        priority = p->stat.rpriority;
      break;
    }
  }
  release(&ptable.lock);
  return priority;
}

int
setpriority(int pid, int priority)
{
  struct proc *p;
  int prev;

  if(priority < 0 || priority >= NPRIORITY)
    return -1;

  acquire(&ptable.lock);
  prev = -1;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->stat.pid == pid){
      prev = p->stat.rpriority;

      // Prevent double-setting of the same priority.
      // If this was allowed, setpriority(pid, getpriority(pid)) would
      // cause the process to move to the back of its process queue
      // rather than being idempotent.
      if(prev == priority)
        break;

      if(p->stat.state == P_RUNNABLE)
        // popproc will not change the state from RUNNABLE.  We can use
        // this to check whether the process was queued again after
        // changing the priority to requeue it.
        popproc(p); // Does not change state so we can requeue.

      p->stat.rpriority = priority;
      p->stat.epriority = priority;

      // If the process was queued to be run before the priority was
      // changed, push it onto its new priority queue.
      if(p->stat.state == P_RUNNABLE)
        pushproc(p);

      break;
    }
  }
  release(&ptable.lock);
  return prev;
}

int
pstat(int pid, struct procstat *st)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->stat.pid == pid){
      memmove(st, &p->stat, sizeof(*st));
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Return the struct proc for a given PID or 0 if no such process
// can be found
struct proc*
getproc(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->stat.pid == pid){
      release(&ptable.lock);
      return p;
    }
  }
  release(&ptable.lock);
  return 0;
}
