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
  struct proc *curproc = myproc();
  struct shm_page *p, *page = 0;
  char *va;

  // Shared memory IDs must be positive.
  if(id <= 0)
    return -1;

  // Only operate on shm_table while it is locked.
  acquire(&shm_table.lock);

  // Locate a suitable entry in shm_table.
  for(p = shm_table.pages; p < &shm_table.pages[NSHM]; p++){
    // If the requested shared memory entry is found, stop looking.
    if(p->id == id){
      page = p;
      break;
    }

    // If an available shared memory entry is found, store it.
    if(p->id == 0 && page == 0){
      page = p;
    }
  }

  // If the search loop terminates with a null page, the requested shared
  // memory entry was not found and no available entries were found to be
  // allocated.
  if(page == 0){
    cprintf("shm_open: out of shared memory\n");
    release(&shm_table.lock);
    return -1;
  }

  // If the search loop terminates with an available entry, the requested
  // shared memory was not found and must be allocated in the entry.
  if(page->id == 0){
    page->frame = kalloc(); // Allocate a page for sharing.
    if(page->frame == 0){
      release(&shm_table.lock);
      return -1;
    }
    memset(page->frame, 0, PGSIZE); // Initialize the page with zeros.
    page->id = id;
    page->refs = 0;
  }

  // Map the shared page into the address space of the current process,
  // increase its reference count, and return its new virtual address in *ptr.
  // If the shared page exists, increase its reference count, map it into
  va = (char *)PGROUNDUP(curproc->sz); // Use the next available "heap" page.
  if(mappages(curproc->pgdir, va, PGSIZE, V2P(page->frame),
        PTE_W | PTE_U) < 0){
    cprintf("shm_open: unable to map page\n");
    release(&shm_table.lock);
    return -1;
  }
  curproc->sz = (uint)va + PGSIZE;
  *ptr = va;
  page->refs++;

  release(&shm_table.lock);
  return 0;
}

int
shm_close(int id)
{
  struct shm_page *p;

  // Shared memory IDs must be positive.
  if(id <= 0)
    return -1;

  // Only operate on shm_table while it is locked.
  acquire(&shm_table.lock);

  // Locate the shared memory entry with the given ID.
  for(p = shm_table.pages; p < &shm_table.pages[NSHM]; p++){
    // If the entry has been located, decrease its reference count.
    if(p->id == id){
      p->refs--;
      // If there are no more references to the shared memory, mark the entry
      // as available.
      if(p->refs == 0){
        p->id = 0;
        p->frame = 0;
      }
      release(&shm_table.lock);
      return 0;
    }
  }

  // No such entry exists.
  release(&shm_table.lock);
  return -1;
}
