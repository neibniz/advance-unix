#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct extents {
  off_t first_data;
  off_t first_hole;
  off_t second_data;
  off_t end_hole;
};

static int pwrite_all(int fd, const void *buffer, size_t size, off_t offset) {
  const unsigned char *bytes = buffer;
  size_t written = 0;

  while (written < size) {
    ssize_t count =
        pwrite(fd, bytes + written, size - written, offset + (off_t)written);
    if (count > 0) {
      written += (size_t)count;
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count == 0) {
      errno = EIO;
    }
    return -1;
  }
  return 0;
}

static bool extent_query_unsupported(int error_number) {
  return error_number == EINVAL || error_number == ENOTSUP ||
         error_number == EOPNOTSUPP;
}

/* Returns 1 on success, 0 when the filesystem lacks the API, and -1 on error.
 */
static int query_extents(int fd, struct extents *found) {
  found->first_data = lseek(fd, (off_t)0, SEEK_DATA);
  if (found->first_data < 0) {
    return extent_query_unsupported(errno) ? 0 : -1;
  }
  found->first_hole = lseek(fd, found->first_data, SEEK_HOLE);
  if (found->first_hole < 0) {
    return extent_query_unsupported(errno) ? 0 : -1;
  }
  return 1;
}

/* Returns 1 for two detailed extents, 0 for a legal coarse answer, -1 on
   unexpected errors. */
static int query_second_extent(int fd, off_t file_size, struct extents *found) {
  if (found->first_data != (off_t)0 || found->first_hole >= file_size) {
    return 0;
  }
  found->second_data = lseek(fd, found->first_hole, SEEK_DATA);
  if (found->second_data < 0) {
    return errno == ENXIO || extent_query_unsupported(errno) ? 0 : -1;
  }
  found->end_hole = lseek(fd, found->second_data, SEEK_HOLE);
  if (found->end_hole < 0) {
    return extent_query_unsupported(errno) ? 0 : -1;
  }
  return 1;
}

int main(void) {
  static const char head[] = "HEAD";
  static const char tail[] = "TAIL";
  const off_t tail_offset = (off_t)8 * 1024 * 1024;
  const off_t expected_size = tail_offset + (off_t)(sizeof(tail) - 1U);
  char path[] = "./sparse-file-XXXXXX";
  struct stat status = {0};
  struct extents found = {0};
  bool file_created = false;
  int fd = -1;
  int result = EXIT_FAILURE;

  fd = mkstemp(path);
  if (fd < 0) {
    perror("mkstemp");
    goto cleanup;
  }
  file_created = true;

  if (pwrite_all(fd, head, sizeof(head) - 1U, (off_t)0) < 0 ||
      pwrite_all(fd, tail, sizeof(tail) - 1U, tail_offset) < 0) {
    perror("pwrite");
    goto cleanup;
  }
  if (fstat(fd, &status) < 0) {
    perror("fstat");
    goto cleanup;
  }
  if (status.st_size != expected_size || status.st_blocks < 0) {
    fprintf(stderr, "unexpected sparse-file size or block count\n");
    goto cleanup;
  }

  const uintmax_t allocated_bytes = (uintmax_t)status.st_blocks * 512U;
  if (allocated_bytes >= (uintmax_t)status.st_size) {
    fprintf(stderr, "file is not sparse: allocated=%ju logical=%jd\n",
            allocated_bytes, (intmax_t)status.st_size);
    goto cleanup;
  }

  const int extent_result = query_extents(fd, &found);
  if (extent_result < 0) {
    perror("SEEK_DATA/SEEK_HOLE");
    goto cleanup;
  }
  const int detail_result =
      extent_result == 1 ? query_second_extent(fd, expected_size, &found) : 0;
  if (detail_result < 0) {
    perror("detailed SEEK_DATA/SEEK_HOLE");
    goto cleanup;
  }
  if (extent_result == 0 || detail_result == 0) {
    printf("logical=%jd allocated=%ju; filesystem does not support "
           "detailed SEEK_DATA/SEEK_HOLE, extent check skipped\n",
           (intmax_t)status.st_size, allocated_bytes);
    result = EXIT_SUCCESS;
    goto cleanup;
  }

  if (found.first_hole <= (off_t)0 || found.first_hole >= tail_offset ||
      found.second_data < found.first_hole || found.second_data > tail_offset ||
      found.end_hole != expected_size) {
    fprintf(stderr, "unexpected extents: data=%jd hole=%jd data=%jd hole=%jd\n",
            (intmax_t)found.first_data, (intmax_t)found.first_hole,
            (intmax_t)found.second_data, (intmax_t)found.end_hole);
    goto cleanup;
  }

  printf("logical=%jd allocated=%ju; extents data[0,%jd) data[%jd,%jd)\n",
         (intmax_t)status.st_size, allocated_bytes, (intmax_t)found.first_hole,
         (intmax_t)found.second_data, (intmax_t)found.end_hole);
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
