enum procstate {
  P_UNUSED,
  P_EMBRYO,
  P_SLEEPING,
  P_RUNNABLE,
  P_RUNNING,
  P_ZOMBIE
};

struct procstat {
  int pid;       // Process ID
  int parent;    // Parent process ID
  char name[16]; // Process name
  int state;     // Current state in the scheduler
  int status;    // Exit status
  uint sz;       // Size of process memory (in bytes)
  int rpriority; // Requested priority
  int epriority; // Effective priority
  // These values are -1 if their associated event has not yet occurred
  int tcreate;   // Time of creation
  int texit;     // Time of exit/kill
  int tfirst;    // Time of first schedule
  int tlast;     // Time of most recent schedule
  int tenter;    // Time of entering the current queue
  int atr;       // Average time in the ready queue
  int atw;       // Average time in the wait queue
};
