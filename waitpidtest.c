#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

#define TEST_STATUS 3

int
main(int argc, char *argv[])
{
  int pid;
  int status;

  pid = fork();
  if(pid < 0){
    printf(2, "waitpidtest: fork failed\n");
    exit(-1);
  }

  if(pid == 0){
    // In child
    printf(1, "waitpidtest: exiting child\n");
    exit(TEST_STATUS);
  }
  else{
    // In parent
    printf(1, "waitpidtest: waiting for pid %d\n", pid);

    if(waitpid(pid, &status, 0) != pid){
      printf(2, "waitpidtest: waitpid failed\n");
      exit(-1);
    }

    if(status != TEST_STATUS){
      printf(2, "waitpidtest: returned wrong status %d\n", status);
      exit(-1);
    }

    printf(1, "waitpidtest: received correct status %d\n", status);
    exit(status);
  }
}
