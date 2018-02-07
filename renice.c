#include "param.h"
#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid;
  int priority;

  if(argc == 1 || argc > 3){
    printf(0, "Usage: %s <+n | -n | n> <pid>\n", argv[0]);
    printf(0, "Purpose: Adjusts the priority of a running process.\n");
    printf(0, "Arguments:\n");
    printf(0, "   +n  Increment the process priority by n.\n");
    printf(0, "   -n  Decrement the process priority by n.\n");
    printf(0, "    n  Set the process priority to n.\n");
    printf(0, "  pid  PID of the process to adjust the priority of.\n");
    exit(0);
  }

  pid = atoi(argv[argc - 1]);
  priority = getpriority(pid);
  if(priority < 0){
    printf(1, "renice: unable to get priority of process %d\n", pid);
    exit(1);
  }

  if(argc == 2){
    printf(0, "%d\n", priority);
    exit(0);
  }

  switch(argv[1][0]){
    case '+': priority += atoi(argv[1] + 1); break;
    case '-': priority -= atoi(argv[1] + 1); break;
    default: priority = atoi(argv[1]); break;
  }

  // Bind the priority to the acceptable range.
  if(priority < 0)
    priority = 0;
  else if(priority >= NPRIORITY)
    priority = NPRIORITY - 1;

  if(setpriority(pid, priority) < 0){
    printf(1, "renice: unable to set priority of process %d to %d\n", pid,
        priority);
    exit(1);
  }

  exit(0);
}
