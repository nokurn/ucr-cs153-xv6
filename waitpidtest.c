#include "types.h"
#include "user.h"

#define NUMBER_PIDS 4

int
main(int argc, char *argv[])
{
  int i;
  int pids[NUMBER_PIDS] = {0};
  int status;

  for(i = 0; i < NUMBER_PIDS && (pids[i] = fork()) > 0; i++){
    printf(1, "waitpidtest: forked child %d\n", i);
  }

  if(i < NUMBER_PIDS){
    if(pids[i] < 0){
      printf(2, "waitpidtest: fork %d failed\n", i);
      exit(-1);
    }

    if(pids[i] == 0){
      exit(i);
    }
  }
  else{
    for(i = 0; i < NUMBER_PIDS; i++){
      printf(1, "waitpidtest: waiting for pid %d (child %d)\n", pids[i], i);

      if(waitpid(pids[i], &status, 0) != pids[i]){
        printf(2, "waitpidtest: waitpid failed\n");
        continue;
      }

      if(status != i){
        printf(2, "waitpidtest: returned wrong status %d\n", status);
        continue;
      }

      printf(1, "waitpidtest: received correct status %d\n", status);
    }
  }

  exit(0);
}
