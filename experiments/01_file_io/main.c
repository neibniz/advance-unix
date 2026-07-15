#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

static ssize_t pread_full(int fd, void *buffer, size_t length, off_t offset) {
  size_t done = 0;

  while (done < length) {
    ssize_t count =
        pread(fd, (char *)buffer + done, length - done, offset + (off_t)done);
    if (count > 0) {
      done += (size_t)count;
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    return count < 0 ? -1 : (ssize_t)done;
  }

  return (ssize_t)done;
}

int main(void) {
  static const char expected[] = "hello UNIX!";
  char path[] = "./lab_file_io.XXXXXX";
  int fd = -1;
  int result = EXIT_FAILURE;
  int file_created = 0;

  fd = mkstemp(path);
  if (fd < 0) {
    perror("mkstemp");
    goto cleanup;
  }
  file_created = 1;

  struct iovec output[] = {
      {.iov_base = (void *)"hello", .iov_len = 5},
      {.iov_base = (void *)" ", .iov_len = 1},
      {.iov_base = (void *)"world", .iov_len = 5},
  };
  ssize_t count;
  do {
    count = writev(fd, output, 3);
  } while (count < 0 && errno == EINTR);
  if (count != (ssize_t)(sizeof(expected) - 1)) {
    fprintf(stderr, "writev: expected %zu bytes, got %zd\n",
            sizeof(expected) - 1, count);
    goto cleanup;
  }

  do {
    count = pwrite(fd, "UNIX!", 5, 6);
  } while (count < 0 && errno == EINTR);
  if (count != 5) {
    perror("pwrite");
    goto cleanup;
  }

  off_t position = lseek(fd, 0, SEEK_CUR);
  if (position != (off_t)(sizeof(expected) - 1)) {
    fprintf(stderr, "pwrite unexpectedly changed the file offset\n");
    goto cleanup;
  }

  char positioned[sizeof(expected)] = {0};
  count = pread_full(fd, positioned, sizeof(expected) - 1, 0);
  if (count != (ssize_t)(sizeof(expected) - 1) ||
      memcmp(positioned, expected, sizeof(expected) - 1) != 0) {
    fprintf(stderr, "pread verification failed\n");
    goto cleanup;
  }
  if (lseek(fd, 0, SEEK_CUR) != position) {
    fprintf(stderr, "pread unexpectedly changed the file offset\n");
    goto cleanup;
  }

  if (lseek(fd, 0, SEEK_SET) < 0) {
    perror("lseek");
    goto cleanup;
  }
  char first[5];
  char separator[1];
  char last[5];
  struct iovec input[] = {
      {.iov_base = first, .iov_len = sizeof(first)},
      {.iov_base = separator, .iov_len = sizeof(separator)},
      {.iov_base = last, .iov_len = sizeof(last)},
  };
  do {
    count = readv(fd, input, 3);
  } while (count < 0 && errno == EINTR);
  if (count != (ssize_t)(sizeof(expected) - 1)) {
    fprintf(stderr, "readv: expected %zu bytes, got %zd\n",
            sizeof(expected) - 1, count);
    goto cleanup;
  }

  char gathered[sizeof(expected)] = {0};
  memcpy(gathered, first, sizeof(first));
  memcpy(gathered + sizeof(first), separator, sizeof(separator));
  memcpy(gathered + sizeof(first) + sizeof(separator), last, sizeof(last));
  if (strcmp(gathered, expected) != 0) {
    fprintf(stderr, "readv verification failed: %s\n", gathered);
    goto cleanup;
  }

  printf("file I/O verified: %s\n", gathered);
  result = EXIT_SUCCESS;

cleanup:
  if (fd >= 0 && close(fd) < 0) {
    perror("close");
    result = EXIT_FAILURE;
  }
  if (file_created && unlink(path) < 0) {
    perror("unlink");
    result = EXIT_FAILURE;
  }
  return result;
}
