#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int wait_for_child(pid_t child, int *status) {
  pid_t waited;
  do {
    waited = waitpid(child, status, 0);
  } while (waited < 0 && errno == EINTR);
  return waited == child ? 0 : -1;
}

static int read_exact_with_poll(int descriptor, void *buffer, size_t size) {
  size_t used = 0;
  while (used < size) {
    struct pollfd watched = {.fd = descriptor, .events = POLLIN};
    int ready;
    do {
      ready = poll(&watched, 1, 3000);
    } while (ready < 0 && errno == EINTR);
    if (ready == 0) {
      errno = ETIMEDOUT;
      return -1;
    }
    if (ready < 0) {
      return -1;
    }
    if ((watched.revents & POLLNVAL) != 0) {
      errno = EBADF;
      return -1;
    }
    if ((watched.revents & POLLERR) != 0) {
      errno = EIO;
      return -1;
    }
    ssize_t count;
    do {
      count = read(descriptor, (char *)buffer + used, size - used);
    } while (count < 0 && errno == EINTR);
    if (count <= 0) {
      if (count == 0) {
        errno = EPIPE;
      }
      return -1;
    }
    used += (size_t)count;
  }
  return 0;
}

int main(void) {
  static const char message[] = "hello through a named FIFO";
  char directory_template[] = "advance_unix_fifo_XXXXXX";
  char fifo_path[sizeof(directory_template) + 16] = {0};
  char received[sizeof(message)] = {0};
  char *directory = NULL;
  int read_descriptor = -1;
  pid_t child = -1;
  int child_reaped = 0;
  int fifo_created = 0;
  int result = EXIT_FAILURE;

  directory = mkdtemp(directory_template);
  if (directory == NULL) {
    perror("mkdtemp");
    goto cleanup;
  }
  const int path_length =
      snprintf(fifo_path, sizeof(fifo_path), "%s/channel", directory);
  if (path_length < 0 || (size_t)path_length >= sizeof(fifo_path)) {
    fprintf(stderr, "FIFO path is too long\n");
    goto cleanup;
  }
  if (mkfifo(fifo_path, S_IRUSR | S_IWUSR) < 0) {
    perror("mkfifo");
    goto cleanup;
  }
  fifo_created = 1;

  struct stat information;
  if (lstat(fifo_path, &information) < 0) {
    perror("verify FIFO");
    goto cleanup;
  }
  if (!S_ISFIFO(information.st_mode)) {
    fprintf(stderr, "created path is not a FIFO\n");
    goto cleanup;
  }
  /* A nonblocking reader can open before a writer without deadlocking. */
  read_descriptor = open(fifo_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (read_descriptor < 0) {
    perror("open FIFO reader");
    goto cleanup;
  }
  child = fork();
  if (child < 0) {
    perror("fork");
    goto cleanup;
  }
  if (child == 0) {
    (void)close(read_descriptor);
    const int writer = open(fifo_path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (writer < 0) {
      _exit(120);
    }
    ssize_t count;
    do {
      count = write(writer, message, sizeof(message));
    } while (count < 0 && errno == EINTR);
    const int write_ok = count == (ssize_t)sizeof(message);
    const int close_ok = close(writer) == 0;
    _exit(write_ok && close_ok ? EXIT_SUCCESS : 121);
  }
  /* The child uses only nonblocking calls; reaping first guarantees data or a
     reported child failure before polling the FIFO. */
  int child_status = 0;
  if (wait_for_child(child, &child_status) < 0) {
    const int wait_error = errno;
    if (wait_error == ECHILD) {
      child_reaped = 1;
    }
    errno = wait_error;
    perror("waitpid");
    goto cleanup;
  }
  child_reaped = 1;
  if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != EXIT_SUCCESS) {
    fprintf(stderr, "FIFO writer failed\n");
    goto cleanup;
  }
  if (read_exact_with_poll(read_descriptor, received, sizeof(received)) < 0) {
    perror("poll/read FIFO");
    goto cleanup;
  }
  if (memcmp(received, message, sizeof(message)) != 0) {
    fprintf(stderr, "FIFO payload mismatch\n");
    goto cleanup;
  }
  result = EXIT_SUCCESS;

cleanup:
  if (child > 0 && !child_reaped) {
    if (kill(child, SIGTERM) < 0 && errno != ESRCH) {
      perror("kill child");
      result = EXIT_FAILURE;
    }
    int cleanup_status = 0;
    if (wait_for_child(child, &cleanup_status) < 0 && errno != ECHILD) {
      perror("waitpid cleanup");
      result = EXIT_FAILURE;
    }
  }
  if (read_descriptor >= 0 && close(read_descriptor) < 0) {
    perror("close FIFO reader");
    result = EXIT_FAILURE;
  }
  if (fifo_created && unlink(fifo_path) < 0) {
    perror("unlink FIFO");
    result = EXIT_FAILURE;
  }
  if (directory != NULL && rmdir(directory) < 0) {
    perror("rmdir FIFO directory");
    result = EXIT_FAILURE;
  }
  if (result == EXIT_SUCCESS) {
    printf("FIFO delivered %zu bytes through poll and was cleaned up\n",
           sizeof(message));
  }
  return result;
}
