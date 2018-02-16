#include "types.h"
#include "procstat.h"
#include "fcntl.h"
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

static void
testaging(void)
{
  int pid;
  int i, j;
  struct procstat st;

  printf(1, "Lab 2:\n");
  printf(1, "Testing the process aging\n");

  pid = fork();
  if(pid == 0){
    printf(1, "  Initializing priority to 15\n");
    setpriority(getpid(), 15);

    // Use up cycles to reduce priority
    printf(1, "\n  Wasting time to reduce priority\n");
    for(i = 0; i < 50000; i++)
      for(j = 0; j < 10000; j++)
        asm("nop");

    // Sleep to increase priority
    printf(1, "\n  Taking naps to increase priority\n");
    for(i = 0; i < 100; i++)
      sleep(1);

    exit(0);
  }
  else if(pid < 0){
    printf(2, "  Error using fork\n");
    exit(-1);
  }

  printf(1, "Priority:");
  i = 0;
  do{
    if(pstat(pid, &st) < 0){
      printf(2, "  Error using pstat\n");
      exit(-1);
    }

    if(st.epriority != i){
      printf(1, " %d", i);
      i = st.epriority;
    }
  } while (st.state != P_ZOMBIE);
  wait(0); // Reap the child

  printf(1, "\n");
}

static void
testdonation(void)
{
  int fd;
  int lpid, mpid, hpid;
  int i;
  struct procstat st;

  printf(1, "Lab 2:\n");
  printf(1, "Testing the priority donation\n");

  lpid = fork();
  if(lpid == 0){
    setpriority(getpid(), 31); // Start at the lowest priority

    fd = open("testdonation", O_CREATE | O_RDWR);
    if(fd < 0){
      printf(2, "  Error opening test file\n");
      exit(-1);
    }

    for(i = 0; i < 100; i++){
      if(write(fd, "aaaaaaaaaa", 10) != 10){
        printf(2, "  Error writing to test file\n");
        exit(-1);
      }
      if(write(fd, "bbbbbbbbbb", 10) != 10){
        printf(2, "  Error writing to test file\n");
        exit(-1);
      }
    }

    exit(0);
  }
  else if(lpid < 0){
    printf(2, "  Error using fork\n");
    exit(-1);
  }

  hpid = fork();
  if(hpid == 0){
    sleep(25);
    setpriority(getpid(), 0); // Start at the highest priority

    fd = open("testdonation", O_CREATE | O_RDWR);
    if(fd < 0){
      printf(2, "  Error opening test file\n");
      exit(-1);
    }

    for(i = 0; i < 100; i++){
      if(write(fd, "aaaaaaaaaa", 10) != 10){
        printf(2, "  Error writing to test file\n");
        exit(-1);
      }
      if(write(fd, "bbbbbbbbbb", 10) != 10){
        printf(2, "  Error writing to test file\n");
        exit(-1);
      }
    }

    exit(0);
  }
  else if(hpid < 0){
    printf(2, "  Error using fork\n");
    exit(-1);
  }

  mpid = fork();
  if(mpid == 0){
    setpriority(getpid(), 15); // Start at the medium priority

    do{
      if(pstat(lpid, &st) < 0)
        break;
    } while(st.state != P_ZOMBIE);

    exit(0);
  }
  else if(mpid < 0){
    printf(2, "  Error using fork\n");
    exit(-1);
  }

  for(i = 0; i < 3; i++)
    wait(0); // Reap the children
}

int
main(int argc, char *argv[])
{
  printf(1, "This program tests the correctness of lab #2\n");

  if(atoi(argv[1]) == 1)
    testmlfq();
  else if(atoi(argv[1]) == 2)
    testpstat();
  else if(atoi(argv[1]) == 3)
    testaging();
  else if(atoi(argv[1]) == 4)
    testdonation();
  else{
    printf(1, "Run `lab2 1` to test the priority scheduler\n");
    printf(1, "Run `lab2 2` to test the process statistics\n");
    printf(1, "Run `lab2 3` to test the process aging\n");
    printf(1, "Run `lab2 4` to test the process donation\n");
  }

  // End of test
  exit(0);
}
