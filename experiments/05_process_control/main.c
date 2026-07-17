#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int make_cloexec_pipe(int pipefd[2]) {
#ifdef __linux__
  return pipe2(pipefd, O_CLOEXEC);
#else
  if (pipe(pipefd) < 0) {
    return -1;
  }
  for (size_t index = 0; index < 2; ++index) {
    int flags = fcntl(pipefd[index], F_GETFD);
    if (flags < 0 || fcntl(pipefd[index], F_SETFD, flags | FD_CLOEXEC) < 0) {
      int saved_errno = errno;
      close(pipefd[0]);
      close(pipefd[1]);
      errno = saved_errno;
      return -1;
    }
  }
  return 0;
#endif
}

static int add_file_actions(posix_spawn_file_actions_t *actions,
                            const int pipefd[2]) {
  int error = posix_spawn_file_actions_addclose(actions, pipefd[0]);
  if (error == 0) {
    error = posix_spawn_file_actions_adddup2(actions, pipefd[1], STDOUT_FILENO);
  }
  if (error == 0 && pipefd[1] != STDOUT_FILENO) {
    error = posix_spawn_file_actions_addclose(actions, pipefd[1]);
  }
  return error;
}

int main(void) {
  static char expected[] = "child: posix_spawnp\n";
  int pipefd[2] = {-1, -1};
  posix_spawn_file_actions_t actions;
  int actions_ready = 0;
  pid_t child = -1;
  int child_started = 0;
  int child_waited = 0;
  int result = EXIT_FAILURE;

  if (make_cloexec_pipe(pipefd) < 0) {
    perror("create close-on-exec pipe");
    goto cleanup;
  }

  int error = posix_spawn_file_actions_init(&actions);
  if (error != 0) {
    fprintf(stderr, "posix_spawn_file_actions_init: %s\n", strerror(error));
    goto cleanup;
  }
  actions_ready = 1;

  error = add_file_actions(&actions, pipefd);
  if (error != 0) {
    fprintf(stderr, "posix_spawn file action: %s\n", strerror(error));
    goto cleanup;
  }

  char *const child_argv[] = {"printf", "%s", expected, NULL};
  error = posix_spawnp(&child, "printf", &actions, NULL, child_argv, environ);
  if (error != 0) {
    fprintf(stderr, "posix_spawnp: %s\n", strerror(error));
    goto cleanup;
  }
  child_started = 1;

  error = posix_spawn_file_actions_destroy(&actions);
  actions_ready = 0;
  if (error != 0) {
    fprintf(stderr, "posix_spawn_file_actions_destroy: %s\n", strerror(error));
    goto cleanup;
  }

  if (close(pipefd[1]) < 0) {
    perror("close pipe writer");
    pipefd[1] = -1;
    goto cleanup;
  }
  pipefd[1] = -1;

  char captured[128];
  size_t used = 0;
  while (used < sizeof(captured) - 1) {
    ssize_t count =
        read(pipefd[0], captured + used, sizeof(captured) - 1 - used);
    if (count > 0) {
      used += (size_t)count;
      continue;
    }
    if (count == 0) {
      break;
    }
    if (errno == EINTR) {
      continue;
    }
    perror("read child output");
    goto cleanup;
  }
  captured[used] = '\0';

  int status;
  pid_t waited;
  do {
    waited = waitpid(child, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited < 0) {
    perror("waitpid");
    goto cleanup;
  }
  child_waited = 1;

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "child did not exit successfully\n");
    goto cleanup;
  }
  if (strcmp(captured, expected) != 0) {
    fprintf(stderr, "unexpected child output: %s\n", captured);
    goto cleanup;
  }

  printf("process control verified; captured: %s", captured);
  result = EXIT_SUCCESS;

cleanup:
  if (actions_ready) {
    (void)posix_spawn_file_actions_destroy(&actions);
  }
  if (pipefd[0] >= 0 && close(pipefd[0]) < 0) {
    perror("close pipe reader");
    result = EXIT_FAILURE;
  }
  if (pipefd[1] >= 0 && close(pipefd[1]) < 0) {
    perror("close pipe writer");
    result = EXIT_FAILURE;
  }
  if (child_started && !child_waited) {
    int cleanup_status;
    while (waitpid(child, &cleanup_status, 0) < 0 && errno == EINTR) {
    }
  }
  return result;
}
