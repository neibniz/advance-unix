#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_all(int fd, const void *data, size_t size) {
  const unsigned char *cursor = data;
  size_t written = 0;

  while (written < size) {
    const ssize_t count = write(fd, cursor + written, size - written);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (count == 0) {
      errno = EIO;
      return -1;
    }
    written += (size_t)count;
  }
  return 0;
}

static int read_all(int fd, void *data, size_t size) {
  unsigned char *cursor = data;
  size_t received = 0;

  while (received < size) {
    const ssize_t count = read(fd, cursor + received, size - received);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (count == 0) {
      errno = EIO;
      return -1;
    }
    received += (size_t)count;
  }
  return 0;
}

int main(void) {
  static const char payload[] =
      "splice: regular file -> pipe, without a user-space copy\n";
  const size_t payload_size = sizeof(payload) - 1U;
  char path[] = "./zero-copy-XXXXXX";
  char received[sizeof(payload)] = {0};
  int input_fd = -1;
  int pipe_fds[2] = {-1, -1};
  bool path_exists = false;
  int result = EXIT_FAILURE;

  input_fd = mkstemp(path);
  if (input_fd < 0) {
    perror("mkstemp");
    goto cleanup;
  }
  path_exists = true;
  if (unlink(path) < 0) {
    perror("unlink temporary file");
    goto cleanup;
  }
  path_exists = false;

  if (write_all(input_fd, payload, payload_size) < 0) {
    perror("write temporary file");
    goto cleanup;
  }
  if (pipe2(pipe_fds, O_CLOEXEC) < 0) {
    perror("pipe2");
    goto cleanup;
  }

  off_t input_offset = 0;
  size_t moved = 0;
  while (moved < payload_size) {
    const ssize_t count = splice(input_fd, &input_offset, pipe_fds[1], NULL,
                                 payload_size - moved, 0U);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("splice");
      goto cleanup;
    }
    if (count == 0) {
      fprintf(stderr, "splice unexpectedly reached end of file\n");
      goto cleanup;
    }
    moved += (size_t)count;
  }

  if (close(pipe_fds[1]) < 0) {
    perror("close pipe writer");
    pipe_fds[1] = -1;
    goto cleanup;
  }
  pipe_fds[1] = -1;
  if (read_all(pipe_fds[0], received, payload_size) < 0) {
    perror("read pipe");
    goto cleanup;
  }
  if (memcmp(received, payload, payload_size) != 0 ||
      input_offset != (off_t)payload_size) {
    fprintf(stderr, "zero-copy verification failed\n");
    goto cleanup;
  }

  printf("splice transferred %zu bytes and content matched\n", moved);
  result = EXIT_SUCCESS;

cleanup:
  if (pipe_fds[0] >= 0 && close(pipe_fds[0]) < 0) {
    perror("close pipe reader");
    result = EXIT_FAILURE;
  }
  if (pipe_fds[1] >= 0 && close(pipe_fds[1]) < 0) {
    perror("close pipe writer");
    result = EXIT_FAILURE;
  }
  if (input_fd >= 0 && close(input_fd) < 0) {
    perror("close temporary file");
    result = EXIT_FAILURE;
  }
  if (path_exists && unlink(path) < 0) {
    perror("unlink temporary file");
    result = EXIT_FAILURE;
  }
  return result;
}
