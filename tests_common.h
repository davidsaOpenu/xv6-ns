#ifndef TESTS_COMMON_H
#define TESTS_COMMON_H
#include "wstatus.h"
#include "user.h"

/*
*   Common defines and functions used in pouch and pid_ns tests
*/
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#define CLONE_NEWPID 2
#define NULL 0

typedef   signed int          pid_t;
typedef   signed int          size_t;

typedef int (*test_func_t)(void);

int stderr = 2;

int check(int r, const char *msg) {
  if (r < 0) {
    printf(stderr, "%s\n", (char *)msg);
    exit(1);
  }
  return r;
}

void assert_msg(int r, const char *msg) {
  if (r) {
    return;
  }
  printf(stderr, "%s\n", (char *)msg);
  exit(1);
}

static int child_exit_status(int pid) {
  int changed_pid = -1;
  int wstatus;
  do {
    changed_pid = check(wait(&wstatus), "failed to waitpid");
  } while (changed_pid != pid);

  return WEXITSTATUS(wstatus);
}

int run_test(test_func_t func, const char *name) {

  int pid = -1;
  int childstatus;

  int ret = check(fork(), "fork failed");
  if (ret == 0) {
    exit(func());
  }

  pid = ret;
  childstatus = child_exit_status(pid);

  return childstatus;
}




#endif // TESTS_COMMON_H
