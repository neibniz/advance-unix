#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int set_ofd_lock(int fd, short type) {
  struct flock lock = {
      .l_type = type,
      .l_whence = SEEK_SET,
      .l_start = 0,
      .l_len = 1,
      .l_pid = 0,
  };
  return fcntl(fd, F_OFD_SETLK, &lock);
}

static int expect_conflict(int fd, const char *stage) {
  errno = 0;
  if (set_ofd_lock(fd, F_WRLCK) == 0) {
    (void)set_ofd_lock(fd, F_UNLCK);
    fprintf(stderr, "%s: a separate open file description acquired the lock\n",
            stage);
    return -1;
  }
  if (errno != EAGAIN && errno != EACCES) {
    perror(stage);
    return -1;
  }
  return 0;
}

int main(void) {
  char path[] = "./lab_file_locking.XXXXXX";
  int owner = -1;
  int duplicate = -1;
  int contender = -1;
  int file_created = 0;
  int result = EXIT_FAILURE;

  owner = mkstemp(path);
  if (owner < 0) {
    perror("mkstemp");
    goto cleanup;
  }
  file_created = 1;
  ssize_t write_count;
  do {
    write_count = write(owner, "x", 1);
  } while (write_count < 0 && errno == EINTR);
  if (write_count != 1) {
    if (write_count >= 0) {
      errno = EIO;
    }
    perror("write");
    goto cleanup;
  }

  duplicate = dup(owner);
  if (duplicate < 0) {
    perror("dup");
    goto cleanup;
  }
  contender = open(path, O_RDWR | O_CLOEXEC);
  if (contender < 0) {
    perror("open contender");
    goto cleanup;
  }

  if (set_ofd_lock(owner, F_WRLCK) < 0) {
    perror("F_OFD_SETLK");
    goto cleanup;
  }
  if (expect_conflict(contender, "lock conflict") < 0) {
    goto cleanup;
  }

  if (close(owner) < 0) {
    perror("close owner");
    owner = -1;
    goto cleanup;
  }
  owner = -1;
  if (expect_conflict(contender, "lock after closing one duplicate") < 0) {
    goto cleanup;
  }

  if (set_ofd_lock(duplicate, F_UNLCK) < 0) {
    perror("unlock through duplicate");
    goto cleanup;
  }
  if (set_ofd_lock(contender, F_WRLCK) < 0) {
    perror("contender acquire after unlock");
    goto cleanup;
  }

  printf("OFD lock verified: duplicate kept lock, contender acquired after "
         "unlock\n");
  result = EXIT_SUCCESS;

cleanup:
  if (owner >= 0 && close(owner) < 0) {
    perror("close owner");
    result = EXIT_FAILURE;
  }
  if (duplicate >= 0 && close(duplicate) < 0) {
    perror("close duplicate");
    result = EXIT_FAILURE;
  }
  if (contender >= 0 && close(contender) < 0) {
    perror("close contender");
    result = EXIT_FAILURE;
  }
  if (file_created && unlink(path) < 0) {
    perror("unlink");
    result = EXIT_FAILURE;
  }
  return result;
}
