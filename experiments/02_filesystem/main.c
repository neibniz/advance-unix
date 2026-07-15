#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int write_all(int fd, const void *buffer, size_t length) {
  size_t done = 0;

  while (done < length) {
    ssize_t count = write(fd, (const char *)buffer + done, length - done);
    if (count > 0) {
      done += (size_t)count;
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

int main(void) {
  static const char content[] = "directory-relative I/O\n";
  char directory[] = "./lab_filesystem.XXXXXX";
  int directory_created = 0;
  int dirfd = -1;
  int filefd = -1;
  int result = EXIT_FAILURE;

  if (mkdtemp(directory) == NULL) {
    perror("mkdtemp");
    goto cleanup;
  }
  directory_created = 1;

  dirfd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (dirfd < 0) {
    perror("open directory");
    goto cleanup;
  }

  filefd = openat(dirfd, "source.txt",
                  O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (filefd < 0) {
    perror("openat source.txt");
    goto cleanup;
  }
  if (write_all(filefd, content, sizeof(content) - 1) < 0) {
    perror("write");
    goto cleanup;
  }
  if (close(filefd) < 0) {
    perror("close source.txt");
    filefd = -1;
    goto cleanup;
  }
  filefd = -1;

  struct stat status;
  if (fstatat(dirfd, "source.txt", &status, AT_SYMLINK_NOFOLLOW) < 0) {
    perror("fstatat source.txt");
    goto cleanup;
  }
  if (!S_ISREG(status.st_mode) ||
      status.st_size != (off_t)(sizeof(content) - 1)) {
    fprintf(stderr, "fstatat returned unexpected metadata\n");
    goto cleanup;
  }

  if (symlinkat("source.txt", dirfd, "source.link") < 0) {
    perror("symlinkat");
    goto cleanup;
  }
  errno = 0;
  int probe = openat(dirfd, "source.link", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (probe >= 0) {
    close(probe);
    fprintf(stderr, "O_NOFOLLOW unexpectedly followed the symbolic link\n");
    goto cleanup;
  }
  if (errno != ELOOP) {
    perror("openat source.link with O_NOFOLLOW");
    goto cleanup;
  }

  if (renameat(dirfd, "source.txt", dirfd, "renamed.txt") < 0) {
    perror("renameat");
    goto cleanup;
  }
  if (unlinkat(dirfd, "source.link", 0) < 0) {
    perror("unlinkat source.link");
    goto cleanup;
  }
  if (unlinkat(dirfd, "renamed.txt", 0) < 0) {
    perror("unlinkat renamed.txt");
    goto cleanup;
  }

  printf("filesystem APIs verified: regular file renamed and cleaned\n");
  result = EXIT_SUCCESS;

cleanup:
  if (filefd >= 0 && close(filefd) < 0) {
    perror("close");
    result = EXIT_FAILURE;
  }
  if (dirfd >= 0) {
    (void)unlinkat(dirfd, "source.link", 0);
    (void)unlinkat(dirfd, "source.txt", 0);
    (void)unlinkat(dirfd, "renamed.txt", 0);
    if (close(dirfd) < 0) {
      perror("close directory");
      result = EXIT_FAILURE;
    }
  }
  if (directory_created && rmdir(directory) < 0) {
    perror("rmdir");
    result = EXIT_FAILURE;
  }
  return result;
}
