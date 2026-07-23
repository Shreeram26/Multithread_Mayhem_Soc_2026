#include "kernel/types.h"
#include "user/user.h"

static void
show(const char *who)
{
  struct thread_info info;

  if(getthreadinfo(&info) < 0){
    printf("%s: getthreadinfo failed\n", who);
    exit(1);
  }
  printf("[%s] pid=%d  tgid=%d  is_thread=%d\n",
         who, info.pid, info.tgid, info.is_thread);
}

int
main(void)
{
  int pid;

  printf("=== Thread Info Test ===\n\nBefore fork:\n");
  show("before fork");

  printf("\nForking...\n\n");
  pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }
  if(pid == 0){
    show("child");
    exit(0);
  }

  show("parent");
  wait(0);
  exit(0);
}
