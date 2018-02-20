#include "param.h"
#include "types.h"
#include "user.h"

#define MAXARGS 16

int
main(int argc, char *argv[])
{
  int priority;
  int i;
  char *pargv[MAXARGS];

  if(argc < 3){
    printf(0, "Usage: %s <n> <prog> [<arg>...]\n", argv[0]);
    printf(0, "Purpose: Executes a program at a specific priority.\n");
    printf(0, "Arguments:\n");
    printf(0, "     n  Set the process priority to n.\n");
    printf(0, "  prog  Executable to run.\n");
    printf(0, "   arg  Arguments to pass to the program.\n");
    exit(0);
  }

  priority = atoi(argv[1]);
  if(priority < 0)
    priority = 0;
  else if(priority >= NPRIORITY)
    priority = NPRIORITY - 1;
  for(i = 2; i < argc; i++)
    pargv[i - 2] = argv[i];

  setpriority(getpid(), priority);
  exec(pargv[0], pargv);
  exit(0);
}
