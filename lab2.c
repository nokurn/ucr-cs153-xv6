#include "types.h"
#include "user.h"

static void
testmlfq(void)
{
  int pid;
  int i, j, k;
  int priority;

  printf(1, "Lab 2:\n");
  printf(1, "Testing the priority scheduler and\n");
  printf(1, "setpriority(int pid, int priority)\n");
  setpriority(getpid(), 0);
  for(i = 0; i < 3; i++){
    pid = fork();
    if(pid == 0){ // only the child executes this code
      pid = getpid();
      setpriority(pid, 30 - 10 * i);
      for(j = 0; j < 50000; j++)
        for(k = 0; k < 10000; k++)
          asm("nop");
      priority = getpriority(pid);
      printf(1, "  Child %d with priority %d has finished!\n", pid,
             priority);
      exit(priority);
    }
    else if(pid > 0){ // only the parent executes this code
      continue;
    }
    else{ // something went wrong with fork system call
      printf(2, "  Error using fork\n");
      exit(-1);
    }
  }

  if(pid > 0){
    for(i = 0; i < 3; i++){
      pid = wait(&priority);
      printf(1, "  Reaped child %d with priority %d\n", priority);
    }
  }
}

int
main(int argc, char *argv[])
{
  printf(1, "This program tests the correctness of lab #2\n");
  testmlfq();
  exit(0);
}
