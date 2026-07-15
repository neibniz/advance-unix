#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signalfd.h>
#include <time.h>
#include <unistd.h>

int main(void) {
  sigset_t watched;
  sigset_t previous;
  int mask_changed = 0;
  int signal_sent = 0;
  int signal_consumed = 0;
  int signal_fd = -1;
  int result = EXIT_FAILURE;

  if (sigemptyset(&watched) < 0 || sigaddset(&watched, SIGUSR1) < 0) {
    perror("prepare signal set");
    goto cleanup;
  }
  if (sigprocmask(SIG_BLOCK, &watched, &previous) < 0) {
    perror("sigprocmask block");
    goto cleanup;
  }
  mask_changed = 1;

  signal_fd = signalfd(-1, &watched, SFD_CLOEXEC);
  if (signal_fd < 0) {
    perror("signalfd");
    goto cleanup;
  }

  if (kill(getpid(), SIGUSR1) < 0) {
    perror("kill");
    goto cleanup;
  }
  signal_sent = 1;

  struct signalfd_siginfo information;
  ssize_t count;
  do {
    count = read(signal_fd, &information, sizeof(information));
  } while (count < 0 && errno == EINTR);
  if (count != (ssize_t)sizeof(information)) {
    if (count < 0) {
      perror("read signalfd");
    } else {
      fprintf(stderr, "short read from signalfd\n");
    }
    goto cleanup;
  }
  signal_consumed = 1;

  if (information.ssi_signo != (uint32_t)SIGUSR1 ||
      information.ssi_pid != (uint32_t)getpid()) {
    fprintf(stderr, "signalfd returned unexpected signal metadata\n");
    goto cleanup;
  }

  if (close(signal_fd) < 0) {
    perror("close signalfd");
    signal_fd = -1;
    goto cleanup;
  }
  signal_fd = -1;
  if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0) {
    perror("sigprocmask restore");
    goto cleanup;
  }
  mask_changed = 0;

  printf("signal fd verified: SIGUSR1 from pid %ld\n", (long)getpid());
  result = EXIT_SUCCESS;

cleanup:
  if (signal_sent && !signal_consumed) {
    const struct timespec no_wait = {.tv_sec = 0, .tv_nsec = 0};
    while (sigtimedwait(&watched, NULL, &no_wait) < 0 && errno == EINTR) {
    }
  }
  if (signal_fd >= 0 && close(signal_fd) < 0) {
    perror("close signalfd");
    result = EXIT_FAILURE;
  }
  if (mask_changed && sigprocmask(SIG_SETMASK, &previous, NULL) < 0) {
    perror("sigprocmask restore");
    result = EXIT_FAILURE;
  }
  return result;
}
