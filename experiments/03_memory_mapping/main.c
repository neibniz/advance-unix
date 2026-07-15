#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

static ssize_t pread_full(int fd, void *buffer, size_t length) {
  size_t done = 0;

  while (done < length) {
    ssize_t count =
        pread(fd, (char *)buffer + done, length - done, (off_t)done);
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
  static const char payload[] = "visible through MAP_SHARED";
  char path[] = "./lab_memory_mapping.XXXXXX";
  int fd = -1;
  int file_created = 0;
  int result = EXIT_FAILURE;
  void *mapping = MAP_FAILED;
  size_t mapping_length = 0;

  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    fprintf(stderr, "sysconf(_SC_PAGESIZE) failed\n");
    goto cleanup;
  }
  mapping_length = (size_t)page_size;
  if (sizeof(payload) > mapping_length) {
    fprintf(stderr, "payload does not fit in one page\n");
    goto cleanup;
  }

  fd = mkstemp(path);
  if (fd < 0) {
    perror("mkstemp");
    goto cleanup;
  }
  file_created = 1;

  if (ftruncate(fd, (off_t)mapping_length) < 0) {
    perror("ftruncate");
    goto cleanup;
  }

  mapping =
      mmap(NULL, mapping_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapping == MAP_FAILED) {
    perror("mmap");
    goto cleanup;
  }

  memcpy(mapping, payload, sizeof(payload));
  if (msync(mapping, mapping_length, MS_SYNC) < 0) {
    perror("msync");
    goto cleanup;
  }
  if (munmap(mapping, mapping_length) < 0) {
    perror("munmap");
    goto cleanup;
  }
  mapping = MAP_FAILED;

  char observed[sizeof(payload)] = {0};
  ssize_t count = pread_full(fd, observed, sizeof(observed));
  if (count != (ssize_t)sizeof(observed) ||
      memcmp(observed, payload, sizeof(payload)) != 0) {
    fprintf(stderr, "mapped data was not visible through the file\n");
    goto cleanup;
  }

  printf("memory mapping verified: %s (%zu-byte mapping)\n", observed,
         mapping_length);
  result = EXIT_SUCCESS;

cleanup:
  if (mapping != MAP_FAILED && munmap(mapping, mapping_length) < 0) {
    perror("munmap");
    result = EXIT_FAILURE;
  }
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
