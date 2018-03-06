#include "types.h"
#include "stat.h"
#include "user.h"
#include "uspinlock.h"

struct shm_cnt {
  struct uspinlock lock;
  int cnt;
};

int main(int argc, char *argv[])
{
  int pid;
  int i;
  struct shm_cnt *cnt;

  pid = fork();
  sleep(1);

  // shm_open: first process will create the page, the second will just attach
  // to the same page.  We get the virtual address of the page returned into
  // cnt which we can now use but will be shared between the two processes.
  shm_open(1, (char **)&cnt);

  for(i = 0; i < 10000; i++){
    uacquire(&cnt->lock);
    cnt->cnt++;
    urelease(&cnt->lock);

    if(i%1000 == 0){
      printf(1, "Counter in %s is %d at address %x\n", pid ? "parent" :
          "child", cnt->cnt, cnt);
    }
  }

  if(pid){
    printf(1, "Counter in parent is %d\n", cnt->cnt);
    wait();
  }
  else{
    printf(1, "Counter in child is %d\n", cnt->cnt);
  }

  // shm_close: first process will just detach, next one will free u p the
  // shm_table entry
  shm_close(1);
  exit();
}
