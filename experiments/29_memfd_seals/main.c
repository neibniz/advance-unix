#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
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

static int expect_resize_eperm(int fd, off_t size, const char *operation) {
  errno = 0;
  if (ftruncate(fd, size) == 0) {
    fprintf(stderr, "%s unexpectedly succeeded\n", operation);
    return -1;
  }
  if (errno != EPERM) {
    perror(operation);
    return -1;
  }
  return 0;
}

int main(void) {
  static const char payload[] = "sealed in-memory file";
  const size_t payload_size = sizeof(payload) - 1U;
  const off_t file_size = (off_t)payload_size;
  const int data_seals = F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK;
  const int expected_seals = data_seals | F_SEAL_SEAL;
  int fd = -1;
  int result = EXIT_FAILURE;

  fd = memfd_create("advance-unix-seals", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) {
    perror("memfd_create");
    goto cleanup;
  }
  if (write_all(fd, payload, payload_size) < 0) {
    perror("write memfd");
    goto cleanup;
  }

  if (fcntl(fd, F_ADD_SEALS, data_seals) < 0) {
    perror("F_ADD_SEALS data seals");
    goto cleanup;
  }
  if (fcntl(fd, F_ADD_SEALS, F_SEAL_SEAL) < 0) {
    perror("F_ADD_SEALS F_SEAL_SEAL");
    goto cleanup;
  }

  const int observed_seals = fcntl(fd, F_GET_SEALS);
  if (observed_seals < 0) {
    perror("F_GET_SEALS");
    goto cleanup;
  }
  if ((observed_seals & expected_seals) != expected_seals) {
    fprintf(stderr, "expected seals %#x, observed %#x\n", expected_seals,
            observed_seals);
    goto cleanup;
  }

  errno = 0;
  if (fcntl(fd, F_ADD_SEALS, F_SEAL_FUTURE_WRITE) >= 0 || errno != EPERM) {
    fprintf(stderr, "F_SEAL_SEAL did not reject an additional seal\n");
    goto cleanup;
  }

  errno = 0;
  if (pwrite(fd, "X", 1U, 0) >= 0 || errno != EPERM) {
    fprintf(stderr, "pwrite on sealed memfd did not fail with EPERM\n");
    goto cleanup;
  }
  if (expect_resize_eperm(fd, file_size + 1, "grow sealed memfd") < 0) {
    goto cleanup;
  }
  if (expect_resize_eperm(fd, file_size - 1, "shrink sealed memfd") < 0) {
    goto cleanup;
  }

  char buffer[sizeof(payload)] = {0};
  const ssize_t read_size = pread(fd, buffer, payload_size, 0);
  if (read_size != (ssize_t)payload_size ||
      memcmp(buffer, payload, payload_size) != 0) {
    fprintf(stderr, "sealed memfd was not readable\n");
    goto cleanup;
  }

  printf("memfd seals %#x blocked write, grow, and shrink\n", observed_seals);
  result = EXIT_SUCCESS;

cleanup:
  if (fd >= 0 && close(fd) < 0) {
    perror("close");
    result = EXIT_FAILURE;
  }
  return result;
}
