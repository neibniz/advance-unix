#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct shared_state {
  sem_t value_ready;
  int input;
  int output;
};

static int wait_for_semaphore(sem_t *semaphore) {
  int result;
  do {
    result = sem_wait(semaphore);
  } while (result == -1 && errno == EINTR);
  return result;
}

static void terminate_and_reap(pid_t child) {
  if (kill(child, SIGKILL) == -1 && errno != ESRCH) {
    perror("kill child");
  }
  while (waitpid(child, NULL, 0) == -1 && errno == EINTR) {
  }
}

int main(void) {
  char name[64];
  if (snprintf(name, sizeof(name), "/advance_unix_shm_%ld", (long)getpid()) >=
      (int)sizeof(name)) {
    fputs("shared-memory name is too long\n", stderr);
    return EXIT_FAILURE;
  }

  int descriptor = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
  if (descriptor == -1) {
    perror("shm_open");
    return EXIT_FAILURE;
  }
  if (ftruncate(descriptor, (off_t)sizeof(struct shared_state)) == -1) {
    perror("ftruncate");
    close(descriptor);
    shm_unlink(name);
    return EXIT_FAILURE;
  }

  struct shared_state *state = mmap(
      NULL, sizeof(*state), PROT_READ | PROT_WRITE, MAP_SHARED, descriptor, 0);
  if (state == MAP_FAILED) {
    perror("mmap");
    close(descriptor);
    shm_unlink(name);
    return EXIT_FAILURE;
  }

  close(descriptor);
  if (shm_unlink(name) == -1) {
    perror("shm_unlink");
    munmap(state, sizeof(*state));
    return EXIT_FAILURE;
  }
  if (sem_init(&state->value_ready, 1, 0) == -1) {
    perror("sem_init");
    munmap(state, sizeof(*state));
    return EXIT_FAILURE;
  }

  state->input = 21;
  state->output = 0;
  const pid_t child = fork();
  if (child == -1) {
    perror("fork");
    sem_destroy(&state->value_ready);
    munmap(state, sizeof(*state));
    return EXIT_FAILURE;
  }
  if (child == 0) {
    if (wait_for_semaphore(&state->value_ready) == -1) {
      _exit(2);
    }
    state->output = state->input * 2;
    munmap(state, sizeof(*state));
    _exit(0);
  }

  if (sem_post(&state->value_ready) == -1) {
    const int saved_errno = errno;
    terminate_and_reap(child);
    errno = saved_errno;
    perror("sem_post");
    sem_destroy(&state->value_ready);
    munmap(state, sizeof(*state));
    return EXIT_FAILURE;
  }

  int child_status = 0;
  pid_t waited;
  do {
    waited = waitpid(child, &child_status, 0);
  } while (waited == -1 && errno == EINTR);
  if (waited == -1) {
    const int saved_errno = errno;
    terminate_and_reap(child);
    errno = saved_errno;
    perror("waitpid");
    sem_destroy(&state->value_ready);
    munmap(state, sizeof(*state));
    return EXIT_FAILURE;
  }

  const int verified = waited == child && WIFEXITED(child_status) &&
                       WEXITSTATUS(child_status) == 0 && state->output == 42;
  printf("input=%d output=%d child=%s verification=%s\n", state->input,
         state->output,
         waited == child && WIFEXITED(child_status) ? "exited" : "failed",
         verified ? "ok" : "failed");

  if (sem_destroy(&state->value_ready) == -1) {
    perror("sem_destroy");
    munmap(state, sizeof(*state));
    return EXIT_FAILURE;
  }
  if (munmap(state, sizeof(*state)) == -1) {
    perror("munmap");
    return EXIT_FAILURE;
  }
  return verified ? EXIT_SUCCESS : EXIT_FAILURE;
}
