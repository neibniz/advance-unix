#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static struct timespec stat_access_time(const struct stat *status) {
#if defined(__APPLE__)
  return status->st_atimespec;
#else
  return status->st_atim;
#endif
}

static struct timespec stat_modify_time(const struct stat *status) {
#if defined(__APPLE__)
  return status->st_mtimespec;
#else
  return status->st_mtim;
#endif
}

static bool same_time(struct timespec left, struct timespec right) {
  return left.tv_sec == right.tv_sec && left.tv_nsec == right.tv_nsec;
}

int main(void) {
  static const char file_name[] = "sample";
  char directory[] = "./file-metadata-XXXXXX";
  const mode_t experiment_mask = (mode_t)0027;
  const mode_t final_mode = (mode_t)0600;
  const struct timespec requested_times[2] = {
      {.tv_sec = (time_t)1700000000, .tv_nsec = 123456789L},
      {.tv_sec = (time_t)1700000123, .tv_nsec = 987654321L},
  };
  struct stat status = {0};
  mode_t original_mask = 0;
  bool directory_created = false;
  bool file_created = false;
  bool mask_needs_restore = false;
  int directory_fd = -1;
  int fd = -1;
  int result = EXIT_FAILURE;

  if (mkdtemp(directory) == NULL) {
    perror("mkdtemp");
    goto cleanup;
  }
  directory_created = true;
  directory_fd = open(directory, O_RDONLY);
  if (directory_fd < 0) {
    perror("open temporary directory");
    goto cleanup;
  }

  original_mask = umask(experiment_mask);
  mask_needs_restore = true;
  fd = openat(directory_fd, file_name, O_CREAT | O_EXCL | O_RDWR, (mode_t)0666);
  if (fd < 0) {
    perror("openat");
    goto cleanup;
  }
  file_created = true;
  if (fstat(fd, &status) < 0) {
    perror("fstat after openat");
    goto cleanup;
  }
  if (!S_ISREG(status.st_mode)) {
    fprintf(stderr, "openat did not create a regular file\n");
    goto cleanup;
  }
  const mode_t created_mode = status.st_mode & (mode_t)0777;
  if ((created_mode & ~(mode_t)0666) != 0) {
    fprintf(stderr, "created mode contains permissions not requested: %03o\n",
            (unsigned int)created_mode);
    goto cleanup;
  }

  const mode_t active_mask = umask(original_mask);
  mask_needs_restore = false;
  if (active_mask != experiment_mask) {
    fprintf(stderr, "unexpected active umask: %03o\n",
            (unsigned int)active_mask);
    goto cleanup;
  }
  const mode_t restored_mask = umask(original_mask);
  if (restored_mask != original_mask) {
    fprintf(stderr, "original umask was not restored\n");
    goto cleanup;
  }

  if (fchmod(fd, final_mode) < 0) {
    perror("fchmod");
    goto cleanup;
  }
  if (futimens(fd, requested_times) < 0) {
    perror("futimens");
    goto cleanup;
  }
  if (fstat(fd, &status) < 0) {
    perror("fstat after metadata changes");
    goto cleanup;
  }

  const mode_t observed_mode = status.st_mode & (mode_t)0777;
  const struct timespec observed_access = stat_access_time(&status);
  const struct timespec observed_modify = stat_modify_time(&status);
  if (observed_mode != final_mode ||
      !same_time(observed_access, requested_times[0]) ||
      !same_time(observed_modify, requested_times[1])) {
    fprintf(stderr, "final permissions or timestamps did not match\n");
    goto cleanup;
  }

  printf("umask created mode=%03o; final mode=%03o atime=%jd.%09ld "
         "mtime=%jd.%09ld; umask restored\n",
         (unsigned int)created_mode, (unsigned int)observed_mode,
         (intmax_t)observed_access.tv_sec, observed_access.tv_nsec,
         (intmax_t)observed_modify.tv_sec, observed_modify.tv_nsec);
  result = EXIT_SUCCESS;

cleanup:
  if (mask_needs_restore) {
    (void)umask(original_mask);
  }
  if (fd >= 0 && close(fd) < 0) {
    perror("close file");
    result = EXIT_FAILURE;
  }
  if (file_created && unlinkat(directory_fd, file_name, 0) < 0) {
    perror("unlinkat");
    result = EXIT_FAILURE;
  }
  if (directory_fd >= 0 && close(directory_fd) < 0) {
    perror("close directory");
    result = EXIT_FAILURE;
  }
  if (directory_created && rmdir(directory) < 0) {
    perror("rmdir");
    result = EXIT_FAILURE;
  }
  return result;
}
