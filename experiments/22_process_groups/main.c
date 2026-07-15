#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct session_report {
  pid_t pid;
  pid_t pgid;
  pid_t sid;
  int error_number;
  int relation_ok;
};

static int write_all(int fd, const void *buffer, size_t size) {
  const unsigned char *bytes = buffer;
  size_t total = 0;

  while (total < size) {
    const ssize_t count = write(fd, bytes + total, size - total);
    if (count > 0) {
      total += (size_t)count;
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

static int read_all(int fd, void *buffer, size_t size) {
  unsigned char *bytes = buffer;
  size_t total = 0;

  while (total < size) {
    const ssize_t count = read(fd, bytes + total, size - total);
    if (count > 0) {
      total += (size_t)count;
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count == 0) {
      errno = EPIPE;
    }
    return -1;
  }
  return 0;
}

static int wait_for(pid_t child, int *status) {
  pid_t waited;
  do {
    waited = waitpid(child, status, 0);
  } while (waited < 0 && errno == EINTR);
  return waited < 0 ? -1 : 0;
}

static void child_main(int report_fd) {
  struct session_report report = {
      .pid = getpid(),
      .pgid = (pid_t)-1,
      .sid = (pid_t)-1,
      .error_number = 0,
      .relation_ok = 0,
  };

  const pid_t created_sid = setsid();
  if (created_sid < 0) {
    report.error_number = errno;
  } else {
    report.pgid = getpgid(0);
    if (report.pgid < 0) {
      report.error_number = errno;
    } else {
      report.sid = getsid(0);
      if (report.sid < 0) {
        report.error_number = errno;
      } else {
        report.relation_ok = created_sid == report.pid &&
                             report.pgid == report.pid &&
                             report.sid == report.pid;
      }
    }
  }

  const int write_result = write_all(report_fd, &report, sizeof(report));
  (void)close(report_fd);
  if (write_result < 0) {
    _exit(3);
  }
  _exit(report.error_number == 0 && report.relation_ok != 0 ? 0 : 2);
}

int main(void) {
  int pipe_fds[2] = {-1, -1};
  struct session_report report = {0};
  pid_t child = (pid_t)-1;
  bool reaped = false;
  int child_status = 0;
  int result = EXIT_FAILURE;

  if (pipe(pipe_fds) < 0) {
    perror("pipe");
    goto cleanup;
  }
  child = fork();
  if (child < 0) {
    perror("fork");
    goto cleanup;
  }
  if (child == 0) {
    (void)close(pipe_fds[0]);
    child_main(pipe_fds[1]);
  }

  if (close(pipe_fds[1]) < 0) {
    pipe_fds[1] = -1;
    perror("close parent write end");
    goto cleanup;
  }
  pipe_fds[1] = -1;
  if (read_all(pipe_fds[0], &report, sizeof(report)) < 0) {
    perror("read child report");
    goto cleanup;
  }
  if (close(pipe_fds[0]) < 0) {
    pipe_fds[0] = -1;
    perror("close parent read end");
    goto cleanup;
  }
  pipe_fds[0] = -1;

  if (wait_for(child, &child_status) < 0) {
    perror("waitpid");
    goto cleanup;
  }
  reaped = true;
  if (report.error_number != 0) {
    fprintf(stderr, "child session operation failed: %s\n",
            strerror(report.error_number));
    goto cleanup;
  }
  if (report.pid != child || report.relation_ok == 0 || report.pgid != child ||
      report.sid != child) {
    fprintf(stderr, "child reported inconsistent PID/PGID/SID values\n");
    goto cleanup;
  }
  if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0) {
    fprintf(stderr, "child did not exit successfully\n");
    goto cleanup;
  }

  printf("child pid=%ld pgid=%ld sid=%ld; waitpid reaped it\n",
         (long)report.pid, (long)report.pgid, (long)report.sid);
  result = EXIT_SUCCESS;

cleanup:
  if (pipe_fds[0] >= 0 && close(pipe_fds[0]) < 0) {
    perror("close read end");
    result = EXIT_FAILURE;
  }
  if (pipe_fds[1] >= 0 && close(pipe_fds[1]) < 0) {
    perror("close write end");
    result = EXIT_FAILURE;
  }
  if (child > 0 && !reaped && wait_for(child, &child_status) < 0) {
    perror("cleanup waitpid");
    result = EXIT_FAILURE;
  }
  return result;
}
