#include "kernel/types.h"
#include "user/user.h"

#define STACK_SIZE 4096

int
main(void)
{
  uint64 first, second;

  printf("=== Thread Stack Allocator Test ===\n\n");
  printf("Requesting first stack of %d bytes...\n", STACK_SIZE);
  first = alloc_thread_stack(STACK_SIZE);
  if(first == (uint64)-1){
    printf("first allocation failed\n");
    exit(1);
  }
  printf("alloc_thread_stack(%d) -> top = %lu\n\n", STACK_SIZE, first);

  printf("Requesting second stack of %d bytes...\n", STACK_SIZE);
  second = alloc_thread_stack(STACK_SIZE);
  if(second == (uint64)-1){
    printf("second allocation failed\n");
    exit(1);
  }
  printf("alloc_thread_stack(%d) -> top = %lu\n\n", STACK_SIZE, second);
  printf("Difference between returned tops: %lu bytes\n", second - first);
  printf("Expected: %d bytes (adjacent stacks, no guard page).\n", STACK_SIZE);
  exit(0);
}
