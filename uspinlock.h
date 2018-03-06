struct uspinlock {
  uint locked;
};

void uacquire(struct uspinlock*);
void urelease(struct uspinlock*);
