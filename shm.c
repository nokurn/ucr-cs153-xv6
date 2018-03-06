#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct shm_page {
  uint id;
  char *frame;
  int refs;
};

struct {
  struct spinlock lock;
  struct shm_page pages[NSHM];
} shm_table;

void
shminit()
{
  int i;

  initlock(&shm_table.lock, "shm_table");
  acquire(&shm_table.lock);
  for(i = 0; i < NSHM; i++){
    shm_table.pages[i].id = 0;
    shm_table.pages[i].frame = 0;
    shm_table.pages[i].refs = 0;
  }
  release(&shm_table.lock);
}

int
shm_open(int id, char **ptr)
{
  return 0;
}

int
shm_close(int id)
{
  return 0;
}
