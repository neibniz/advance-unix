#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

static void print_value(const char *label, rlim_t value) {
  if (value == RLIM_INFINITY) {
    printf("%s=infinity", label);
  } else {
    printf("%s=%" PRIuMAX, label, (uintmax_t)value);
  }
}

static bool same_limit(const struct rlimit *left, const struct rlimit *right) {
  return left->rlim_cur == right->rlim_cur && left->rlim_max == right->rlim_max;
}

int main(void) {
  struct rlimit original = {0};
  struct rlimit queried = {0};
  struct rlimit replaced = {0};
  struct rlimit temporary = {0};
  struct rlimit observed = {0};
  struct rusage usage = {0};
  bool changed = false;
  int result = EXIT_FAILURE;

  if (getrlimit(RLIMIT_NOFILE, &original) < 0) {
    perror("getrlimit");
    goto cleanup;
  }
  if (prlimit(0, RLIMIT_NOFILE, NULL, &queried) < 0) {
    perror("prlimit query");
    goto cleanup;
  }
  if (!same_limit(&original, &queried)) {
    fprintf(stderr, "getrlimit and prlimit returned different values\n");
    goto cleanup;
  }

  temporary = original;
  if (temporary.rlim_cur == RLIM_INFINITY || temporary.rlim_cur > (rlim_t)64) {
    temporary.rlim_cur = (rlim_t)64;
  }
  if (prlimit(0, RLIMIT_NOFILE, &temporary, &replaced) < 0) {
    perror("prlimit set");
    goto cleanup;
  }
  changed = true;
  if (!same_limit(&original, &replaced)) {
    fprintf(stderr, "prlimit did not report the previous limit\n");
    goto cleanup;
  }
  if (getrlimit(RLIMIT_NOFILE, &observed) < 0) {
    perror("getrlimit after set");
    goto cleanup;
  }
  if (!same_limit(&temporary, &observed)) {
    fprintf(stderr, "temporary limit was not applied\n");
    goto cleanup;
  }
  if (prlimit(0, RLIMIT_NOFILE, &original, NULL) < 0) {
    perror("restore prlimit");
    goto cleanup;
  }
  changed = false;

  if (getrusage(RUSAGE_SELF, &usage) < 0) {
    perror("getrusage");
    goto cleanup;
  }
  if (usage.ru_utime.tv_usec < 0 || usage.ru_utime.tv_usec >= 1000000 ||
      usage.ru_stime.tv_usec < 0 || usage.ru_stime.tv_usec >= 1000000) {
    fprintf(stderr, "getrusage returned an invalid timeval\n");
    goto cleanup;
  }

  print_value("soft", original.rlim_cur);
  printf(" ");
  print_value("hard", original.rlim_max);
  printf("; temporary-soft=%" PRIuMAX "; user=%jd.%06ld sys=%jd.%06ld\n",
         (uintmax_t)temporary.rlim_cur, (intmax_t)usage.ru_utime.tv_sec,
         (long)usage.ru_utime.tv_usec, (intmax_t)usage.ru_stime.tv_sec,
         (long)usage.ru_stime.tv_usec);
  result = EXIT_SUCCESS;

cleanup:
  if (changed && prlimit(0, RLIMIT_NOFILE, &original, NULL) < 0) {
    perror("cleanup: restore prlimit");
    result = EXIT_FAILURE;
  }
  return result;
}
