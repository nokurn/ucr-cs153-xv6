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
  // These values are 0 if they have not yet been sampled
  int atready;   // Average time in the ready queue
  int atwait;    // Average time in the wait queue
  int nready;    // Number of times moving from ready to running
  int nwait;     // Number of times moving from running to wait
  int nyield;    // Number of times being preempted
};
