#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

enum { MESSAGE_TYPE = 7, TEXT_CAPACITY = 64 };

struct message {
  long type;
  char text[TEXT_CAPACITY];
};

static int wait_for_child(pid_t child, int *status) {
  pid_t waited;
  do {
    waited = waitpid(child, status, 0);
  } while (waited < 0 && errno == EINTR);
  return waited == child ? 0 : -1;
}

int main(void) {
  static const char payload[] = "hello from a System V message queue";
  int queue_id = -1;
  pid_t child = -1;
  int child_reaped = 0;
  int result = EXIT_FAILURE;

  queue_id = msgget(IPC_PRIVATE, IPC_CREAT | S_IRUSR | S_IWUSR);
  if (queue_id < 0) {
    perror("msgget");
    goto cleanup;
  }

  child = fork();
  if (child < 0) {
    perror("fork");
    goto cleanup;
  }
  if (child == 0) {
    struct message outgoing = {.type = MESSAGE_TYPE};
    memcpy(outgoing.text, payload, sizeof(payload));
    int send_result;
    do {
      send_result = msgsnd(queue_id, &outgoing, sizeof(payload), IPC_NOWAIT);
    } while (send_result < 0 && errno == EINTR);
    _exit(send_result == 0 ? EXIT_SUCCESS : 120);
  }

  int child_status = 0;
  if (wait_for_child(child, &child_status) < 0) {
    const int wait_error = errno;
    if (wait_error == ECHILD) {
      child_reaped = 1;
    }
    errno = wait_error;
    perror("waitpid");
    goto cleanup;
  }
  child_reaped = 1;
  if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != EXIT_SUCCESS) {
    fprintf(stderr, "message queue sender failed\n");
    goto cleanup;
  }

  struct msqid_ds attributes;
  if (msgctl(queue_id, IPC_STAT, &attributes) < 0) {
    perror("msgctl IPC_STAT");
    goto cleanup;
  }
  if (attributes.msg_qnum != 1U) {
    fprintf(stderr, "unexpected queued message count\n");
    goto cleanup;
  }

  struct message incoming = {0};
  ssize_t received;
  do {
    received = msgrcv(queue_id, &incoming, sizeof(incoming.text), MESSAGE_TYPE,
                      IPC_NOWAIT);
  } while (received < 0 && errno == EINTR);
  if (received < 0) {
    perror("msgrcv");
    goto cleanup;
  }
  if (received != (ssize_t)sizeof(payload) ||
      memcmp(incoming.text, payload, sizeof(payload)) != 0) {
    fprintf(stderr, "message queue payload mismatch\n");
    goto cleanup;
  }

  if (msgctl(queue_id, IPC_STAT, &attributes) < 0) {
    perror("msgctl IPC_STAT after receive");
    goto cleanup;
  }
  if (attributes.msg_qnum != 0U) {
    fprintf(stderr, "message queue is not empty after receive\n");
    goto cleanup;
  }
  result = EXIT_SUCCESS;

cleanup:
  if (queue_id >= 0 && msgctl(queue_id, IPC_RMID, NULL) < 0) {
    perror("msgctl IPC_RMID");
    result = EXIT_FAILURE;
  }
  if (child > 0 && !child_reaped) {
    if (kill(child, SIGTERM) < 0 && errno != ESRCH) {
      perror("kill child");
      result = EXIT_FAILURE;
    }
    int cleanup_status = 0;
    if (wait_for_child(child, &cleanup_status) < 0 && errno != ECHILD) {
      perror("waitpid cleanup");
      result = EXIT_FAILURE;
    }
  }
  if (result == EXIT_SUCCESS) {
    printf("queue id=%d delivered type=%d and was removed\n", queue_id,
           MESSAGE_TYPE);
  }
  return result;
}
