#include <errno.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  cpu_set_t original;
  cpu_set_t single;
  cpu_set_t observed;
  cpu_set_t restored;
  int selected = -1;
  bool affinity_changed = false;
  int result = EXIT_FAILURE;

  CPU_ZERO(&original);
  if (sched_getaffinity(0, sizeof(original), &original) < 0) {
    perror("sched_getaffinity");
    goto cleanup;
  }

  const int original_count = CPU_COUNT(&original);
  if (original_count <= 0) {
    fprintf(stderr, "the allowed CPU set is empty\n");
    goto cleanup;
  }
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
    if (CPU_ISSET(cpu, &original)) {
      selected = cpu;
      break;
    }
  }
  if (selected < 0) {
    fprintf(stderr, "no representable CPU found in affinity mask\n");
    goto cleanup;
  }

  CPU_ZERO(&single);
  CPU_SET(selected, &single);
  if (sched_setaffinity(0, sizeof(single), &single) < 0) {
    perror("sched_setaffinity single CPU");
    goto cleanup;
  }
  affinity_changed = true;

  CPU_ZERO(&observed);
  if (sched_getaffinity(0, sizeof(observed), &observed) < 0) {
    perror("sched_getaffinity after set");
    goto cleanup;
  }
  if (CPU_COUNT(&observed) != 1 || !CPU_ISSET(selected, &observed)) {
    fprintf(stderr, "single-CPU affinity verification failed\n");
    goto cleanup;
  }

  if (sched_setaffinity(0, sizeof(original), &original) < 0) {
    perror("restore sched_setaffinity");
    goto cleanup;
  }
  affinity_changed = false;

  CPU_ZERO(&restored);
  if (sched_getaffinity(0, sizeof(restored), &restored) < 0) {
    perror("sched_getaffinity after restore");
    goto cleanup;
  }
  if (!CPU_EQUAL(&restored, &original)) {
    fprintf(stderr, "original CPU affinity was not restored\n");
    goto cleanup;
  }

  printf("allowed CPUs=%d; temporarily selected CPU=%d; restored=yes\n",
         original_count, selected);
  result = EXIT_SUCCESS;

cleanup:
  if (affinity_changed &&
      sched_setaffinity(0, sizeof(original), &original) < 0) {
    perror("cleanup: restore sched_setaffinity");
    result = EXIT_FAILURE;
  }
  return result;
}
