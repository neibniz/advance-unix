#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum { MESSAGE_SIZE = 64 };

static int receive_and_check(mqd_t queue, const char *expected,
                             unsigned int expected_priority) {
  char buffer[MESSAGE_SIZE] = {0};
  unsigned int priority = 0;
  ssize_t count;

  do {
    count = mq_receive(queue, buffer, sizeof(buffer), &priority);
  } while (count < 0 && errno == EINTR);
  if (count < 0) {
    return -1;
  }

  const size_t expected_size = strlen(expected) + 1U;
  if ((size_t)count != expected_size || priority != expected_priority ||
      memcmp(buffer, expected, expected_size) != 0) {
    errno = EBADMSG;
    return -1;
  }
  return 0;
}

int main(void) {
  static const char low_message[] = "low priority";
  static const char high_message[] = "high priority";
  struct timespec now = {0};
  char queue_name[96] = {0};
  mqd_t queue = (mqd_t)-1;
  bool queue_created = false;
  int result = EXIT_FAILURE;

  if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
    perror("clock_gettime");
    goto cleanup;
  }
  const int name_length =
      snprintf(queue_name, sizeof(queue_name), "/advance_unix_%ld_%ld",
               (long)getpid(), now.tv_nsec);
  if (name_length < 0 || (size_t)name_length >= sizeof(queue_name)) {
    fprintf(stderr, "message queue name is too long\n");
    goto cleanup;
  }

  const struct mq_attr attributes = {
      .mq_flags = 0,
      .mq_maxmsg = 4,
      .mq_msgsize = MESSAGE_SIZE,
      .mq_curmsgs = 0,
  };
  queue = mq_open(queue_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR,
                  &attributes);
  if (queue == (mqd_t)-1) {
    perror("mq_open");
    goto cleanup;
  }
  queue_created = true;

  if (mq_send(queue, low_message, sizeof(low_message), 1U) < 0) {
    perror("mq_send low priority");
    goto cleanup;
  }
  if (mq_send(queue, high_message, sizeof(high_message), 7U) < 0) {
    perror("mq_send high priority");
    goto cleanup;
  }

  struct mq_attr observed = {0};
  if (mq_getattr(queue, &observed) < 0) {
    perror("mq_getattr");
    goto cleanup;
  }
  if (observed.mq_curmsgs != 2 || observed.mq_msgsize != MESSAGE_SIZE) {
    fprintf(stderr, "unexpected message queue attributes\n");
    goto cleanup;
  }

  if (receive_and_check(queue, high_message, 7U) < 0) {
    perror("receive high priority first");
    goto cleanup;
  }
  if (receive_and_check(queue, low_message, 1U) < 0) {
    perror("receive low priority second");
    goto cleanup;
  }

  printf("queue %s delivered priorities 7 then 1 and was cleaned up\n",
         queue_name);
  result = EXIT_SUCCESS;

cleanup:
  if (queue != (mqd_t)-1 && mq_close(queue) < 0) {
    perror("mq_close");
    result = EXIT_FAILURE;
  }
  if (queue_created && mq_unlink(queue_name) < 0) {
    perror("mq_unlink");
    result = EXIT_FAILURE;
  }
  return result;
}
