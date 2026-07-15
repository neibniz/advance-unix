#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t signal_received = 0;

static void handle_sigusr1(int signal_number) {
  (void)signal_number;
  signal_received = 1;
}

static int wait_for_child(pid_t child, int *status) {
  pid_t waited;
  do {
    waited = waitpid(child, status, 0);
  } while (waited < 0 && errno == EINTR);
  return waited == child ? 0 : -1;
}

int main(void) {
  struct sigaction action = {0};
  struct sigaction old_action;
  sigset_t blocked;
  sigset_t old_mask;
  sigset_t wait_mask;
  pid_t child = -1;
  int child_reaped = 0;
  int mask_changed = 0;
  int action_changed = 0;
  int result = EXIT_FAILURE;

  action.sa_handler = handle_sigusr1;
  if (sigemptyset(&action.sa_mask) < 0 || sigemptyset(&blocked) < 0 ||
      sigaddset(&blocked, SIGUSR1) < 0) {
    perror("prepare signal set");
    goto cleanup;
  }
  /* Block before installing the handler and forking, closing the early window
     in which an unrelated SIGUSR1 could run the new handler. */
  if (sigprocmask(SIG_BLOCK, &blocked, &old_mask) < 0) {
    perror("sigprocmask block");
    goto cleanup;
  }
  mask_changed = 1;
  if (sigaction(SIGUSR1, &action, &old_action) < 0) {
    perror("sigaction");
    goto cleanup;
  }
  action_changed = 1;

  child = fork();
  if (child < 0) {
    perror("fork");
    goto cleanup;
  }
  if (child == 0) {
    _exit(kill(getppid(), SIGUSR1) == 0 ? EXIT_SUCCESS : 127);
  }

  int child_status = 0;
  if (wait_for_child(child, &child_status) < 0) {
    perror("waitpid");
    goto cleanup;
  }
  child_reaped = 1;
  if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != EXIT_SUCCESS) {
    fprintf(stderr, "child could not send SIGUSR1\n");
    goto cleanup;
  }

  sigset_t pending;
  if (sigpending(&pending) < 0) {
    perror("sigpending");
    goto cleanup;
  }
  const int is_pending = sigismember(&pending, SIGUSR1);
  if (is_pending < 0) {
    perror("sigismember");
    goto cleanup;
  }
  if (is_pending == 0 || signal_received != 0) {
    fprintf(stderr, "SIGUSR1 was not held pending as expected\n");
    goto cleanup;
  }

  wait_mask = old_mask;
  if (sigdelset(&wait_mask, SIGUSR1) < 0) {
    perror("sigdelset");
    goto cleanup;
  }
  while (signal_received == 0) {
    if (sigsuspend(&wait_mask) < 0 && errno != EINTR) {
      perror("sigsuspend");
      goto cleanup;
    }
  }

  printf("SIGUSR1 was pending, then sigsuspend delivered it atomically\n");
  result = EXIT_SUCCESS;

cleanup:
  if (child > 0 && !child_reaped) {
    int cleanup_status = 0;
    if (wait_for_child(child, &cleanup_status) < 0 && errno != ECHILD) {
      perror("waitpid cleanup");
      result = EXIT_FAILURE;
    }
  }
  if (mask_changed && sigprocmask(SIG_SETMASK, &old_mask, NULL) < 0) {
    perror("sigprocmask restore");
    result = EXIT_FAILURE;
  }
  if (action_changed && sigaction(SIGUSR1, &old_action, NULL) < 0) {
    perror("sigaction restore");
    result = EXIT_FAILURE;
  }
  return result;
}
