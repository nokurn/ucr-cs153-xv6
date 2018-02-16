// Sleeping locks

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"

void
initsleeplock(struct sleeplock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->proc = 0;
}

void
acquiresleep(struct sleeplock *lk)
{
  struct proc *p;
  int donate;

  p = myproc();
  acquire(&lk->lk);
  donate = 0;
  while (lk->locked) {
    if(p->stat.rpriority < lk->proc->stat.rpriority){
      if(!donate){
        cprintf("POSSIBLE PRIORITY INVERSION\n");
        cprintf("  Donating rpriority %d from sleeper %d\n",
            p->stat.rpriority, p->stat.pid);
        cprintf("  to holder %d with rpriority %d\n",
            lk->proc->stat.pid, lk->proc->stat.rpriority);
        setpriority(lk->proc->stat.pid, p->stat.rpriority);
        donate = 1;
      }
    }
    sleep(lk, &lk->lk);
  }
  lk->locked = 1;
  lk->proc = p;
  lk->priority = p->stat.rpriority;
  release(&lk->lk);
}

void
releasesleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  setpriority(lk->proc->stat.pid, lk->priority);
  lk->locked = 0;
  lk->proc = 0;
  lk->priority = 0;
  wakeup(lk);
  release(&lk->lk);
}

int
holdingsleep(struct sleeplock *lk)
{
  int r;
  
  acquire(&lk->lk);
  r = lk->locked;
  release(&lk->lk);
  return r;
}



