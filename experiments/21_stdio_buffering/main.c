#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int set_nonblocking(int fd) {
  int flags;
  do {
    flags = fcntl(fd, F_GETFL);
  } while (flags < 0 && errno == EINTR);
  if (flags < 0) {
    return -1;
  }

  int result;
  do {
    result = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  } while (result < 0 && errno == EINTR);
  return result;
}

static int read_exact(int fd, void *buffer, size_t size) {
  unsigned char *bytes = buffer;
  size_t total = 0;

  while (total < size) {
    const ssize_t count = read(fd, bytes + total, size - total);
    if (count > 0) {
      total += (size_t)count;
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count == 0) {
      errno = EPIPE;
    }
    return -1;
  }
  return 0;
}

int main(void) {
  static const char message[] = "buffered payload";
  int pipe_fds[2] = {-1, -1};
  char stdio_buffer[128] = {0};
  char received[sizeof(message) - 1U] = {0};
  FILE *stream = NULL;
  int result = EXIT_FAILURE;

  if (pipe(pipe_fds) < 0) {
    perror("pipe");
    goto cleanup;
  }
  if (set_nonblocking(pipe_fds[0]) < 0) {
    perror("fcntl O_NONBLOCK");
    goto cleanup;
  }

  const int writer_fd = pipe_fds[1];
  stream = fdopen(writer_fd, "w");
  if (stream == NULL) {
    perror("fdopen");
    goto cleanup;
  }
  pipe_fds[1] = -1; /* The FILE stream now owns this descriptor. */

  if (setvbuf(stream, stdio_buffer, _IOFBF, sizeof(stdio_buffer)) != 0) {
    fprintf(stderr, "setvbuf failed\n");
    goto cleanup;
  }
  if (fileno(stream) != writer_fd) {
    fprintf(stderr, "fileno did not return the wrapped descriptor\n");
    goto cleanup;
  }
  if (fwrite(message, 1U, sizeof(message) - 1U, stream) !=
      sizeof(message) - 1U) {
    perror("fwrite");
    goto cleanup;
  }

  size_t before_flush = 0;
  ssize_t probe_count;
  do {
    probe_count = read(pipe_fds[0], received, sizeof(received));
  } while (probe_count < 0 && errno == EINTR);
  if (probe_count > 0) {
    before_flush = (size_t)probe_count;
  } else if (probe_count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
    fprintf(stderr, "unexpected read result before fflush\n");
    goto cleanup;
  }

  if (fflush(stream) != 0) {
    perror("fflush");
    goto cleanup;
  }
  if (read_exact(pipe_fds[0], received + before_flush,
                 sizeof(received) - before_flush) < 0) {
    perror("read after fflush");
    goto cleanup;
  }
  if (memcmp(received, message, sizeof(received)) != 0) {
    fprintf(stderr, "pipe payload mismatch\n");
    goto cleanup;
  }

  printf("before fflush: %zu bytes; after fflush: total %zu bytes via fd %d\n",
         before_flush, sizeof(received), writer_fd);
  result = EXIT_SUCCESS;

cleanup:
  if (stream != NULL && fclose(stream) != 0) {
    perror("fclose");
    result = EXIT_FAILURE;
  }
  if (pipe_fds[0] >= 0 && close(pipe_fds[0]) < 0) {
    perror("close read end");
    result = EXIT_FAILURE;
  }
  if (pipe_fds[1] >= 0 && close(pipe_fds[1]) < 0) {
    perror("close write end");
    result = EXIT_FAILURE;
  }
  return result;
}
