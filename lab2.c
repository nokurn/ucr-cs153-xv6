#include "types.h"
#include "procstat.h"
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
      printf(1, "  Reaped child %d with priority %d\n", pid, priority);
    }
  }
}

static void
testpstat(void)
{
  int pid;
  int i, j;
  struct procstat st;

  printf(1, "Lab 2:\n");
  printf(1, "Testing the process statistics and\n");
  printf(1, "pstat(int pid, struct procstat *st)\n");

  pid = fork();
  if(pid == 0){
    printf(1, "  Running child...\n");
    for(i = 0; i < 50000; i++)
      for(j = 0; j < 10000; j++)
        asm("nop");
    exit(0);
  }
  else if(pid > 0){
    do{
      sleep(50);
      if(pstat(pid, &st) < 0){
        printf(2, "  Error using pstat\n");
      }
      printf(1, "  procstat snapshot\n");
      printf(1, "    pid       = %d\n", st.pid);
      printf(1, "    parent    = %d\n", st.parent);
      printf(1, "    name      = %s\n", st.name);
      printf(1, "    state     = %d\n", st.state);
      printf(1, "    status    = %d\n", st.status);
      printf(1, "    sz        = %d\n", st.sz);
      printf(1, "    rpriority = %d\n", st.rpriority);
      printf(1, "    epriority = %d\n", st.epriority);
      printf(1, "    tcreate   = %d\n", st.tcreate);
      printf(1, "    texit     = %d\n", st.texit);
      printf(1, "    tfirst    = %d\n", st.tfirst);
      printf(1, "    tlast     = %d\n", st.tlast);
      printf(1, "    tenter    = %d\n", st.tenter);
      printf(1, "    atready   = %d\n", st.atready);
      printf(1, "    atwait    = %d\n", st.atwait);
      printf(1, "    nready    = %d\n", st.nready);
      printf(1, "    nwait     = %d\n", st.nwait);
      printf(1, "    nyield    = %d\n", st.nyield);
      if(st.texit > 0){
        printf(1, "    turn      = %d\n", st.texit - st.tcreate);
        if(st.nyield > 0){
          printf(1, "    norm turn = %d\n", (st.texit - st.tcreate) /
              st.nyield);
        }
      }
    } while (st.state != P_ZOMBIE);
    wait(0); // Reap the child
    printf(1, "  Child exited\n");
  }
  else{
      printf(2, "  Error using fork\n");
      exit(-1);
  }
}

int
main(int argc, char *argv[])
{
  printf(1, "This program tests the correctness of lab #2\n");
  testmlfq();
  testpstat();
  exit(0);
}
