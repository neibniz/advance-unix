#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef P_PIDFD
#define P_PIDFD ((idtype_t)3)
#endif

static int pidfd_open_compat(pid_t pid, unsigned int flags) {
#if defined(SYS_pidfd_open)
  const long value = syscall(SYS_pidfd_open, pid, flags);
#elif defined(__NR_pidfd_open)
  const long value = syscall(__NR_pidfd_open, pid, flags);
#else
  (void)pid;
  (void)flags;
  errno = ENOSYS;
  return -1;
#endif
  if (value < 0) {
    return -1;
  }
  if (value > INT_MAX) {
    errno = EOVERFLOW;
    return -1;
  }
  return (int)value;
}

static int poll_retry(struct pollfd *descriptor, int timeout_ms) {
  int result;
  do {
    result = poll(descriptor, 1U, timeout_ms);
  } while (result < 0 && errno == EINTR);
  return result;
}

int main(void) {
  enum { CHILD_STATUS = 42 };
  int pidfd = -1;
  pid_t child = -1;
  bool child_reaped = false;
  int result = EXIT_FAILURE;

  child = fork();
  if (child < 0) {
    perror("fork");
    goto cleanup;
  }
  if (child == 0) {
    _exit(CHILD_STATUS);
  }

  pidfd = pidfd_open_compat(child, 0U);
  if (pidfd < 0) {
    perror("pidfd_open");
    goto cleanup;
  }

  struct pollfd descriptor = {
      .fd = pidfd,
      .events = POLLIN,
      .revents = 0,
  };
  const int poll_result = poll_retry(&descriptor, 2000);
  if (poll_result < 0) {
    perror("poll pidfd");
    goto cleanup;
  }
  if (poll_result == 0) {
    fprintf(stderr, "timed out waiting for child through pidfd\n");
    goto cleanup;
  }
  if ((descriptor.revents & POLLIN) == 0) {
    fprintf(stderr, "unexpected pidfd poll events: %#x\n",
            (unsigned int)descriptor.revents);
    goto cleanup;
  }

  siginfo_t info = {0};
  if (waitid(P_PIDFD, (id_t)pidfd, &info, WEXITED) < 0) {
    perror("waitid P_PIDFD");
    goto cleanup;
  }
  child_reaped = true;
  if (info.si_code != CLD_EXITED || info.si_status != CHILD_STATUS ||
      info.si_pid != child) {
    fprintf(stderr, "unexpected child status from waitid\n");
    goto cleanup;
  }

  printf("pidfd=%d became readable; child %ld exited with status %d\n", pidfd,
         (long)child, info.si_status);
  result = EXIT_SUCCESS;

cleanup:
  if (pidfd >= 0) {
    (void)close(pidfd);
  }
  if (child > 0 && !child_reaped) {
    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
  }
  return result;
}
