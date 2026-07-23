#include "kernel/types.h"
#include "user/user.h"

#define STACK_SIZE 4096

int
main(void)
{
  uint64 top;
  volatile char *last_valid;
  volatile char *guard_byte;

  printf("=== Guard Page Test ===\n\n");
  top = alloc_thread_stack_guarded(STACK_SIZE);
  if(top == (uint64)-1){
    printf("guarded allocation failed\n");
    exit(1);
  }
  printf("alloc_thread_stack_guarded(%d) -> top = %lu\n", STACK_SIZE, top);

  last_valid = (volatile char *)(top - 1);
  *last_valid = 'A';
  printf("Wrote to last valid byte at address %lu: ok\n", top - 1);

  printf("Writing one byte inside the guard page; this process should die...\n");
  guard_byte = (volatile char *)(top - STACK_SIZE - 1);
  *guard_byte = 'B';

  printf("ERROR: guard page did not trigger a fault\n");
  exit(1);
}
