#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(void) {
  static const char file_name[] = "sample.txt";
  static const char content[] = "inotify\n";
  char directory[] = "./lab-inotify-XXXXXX";
  char file_path[sizeof(directory) + sizeof(file_name)] = {0};

  int inotify_fd = -1;
  int watch = -1;
  int directory_created = 0;
  int file_exists = 0;
  int result = EXIT_FAILURE;

  if (mkdtemp(directory) == NULL) {
    perror("mkdtemp");
    goto cleanup;
  }
  directory_created = 1;
  const int path_length =
      snprintf(file_path, sizeof(file_path), "%s/%s", directory, file_name);
  if (path_length < 0 || path_length >= (int)sizeof(file_path)) {
    fputs("temporary file path is too long\n", stderr);
    goto cleanup;
  }

  inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
  if (inotify_fd == -1) {
    perror("inotify_init1");
    goto cleanup;
  }
  const uint32_t required_events = IN_CREATE | IN_CLOSE_WRITE | IN_DELETE;
  watch = inotify_add_watch(inotify_fd, directory, required_events | IN_MODIFY);
  if (watch == -1) {
    perror("inotify_add_watch");
    goto cleanup;
  }

  int file_fd = open(file_path, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
  if (file_fd == -1) {
    perror("open");
    goto cleanup;
  }
  file_exists = 1;
  const ssize_t written = write(file_fd, content, sizeof(content) - 1U);
  if (close(file_fd) == -1 || written != (ssize_t)(sizeof(content) - 1U)) {
    perror("write/close");
    goto cleanup;
  }
  if (unlink(file_path) == -1) {
    perror("unlink");
    goto cleanup;
  }
  file_exists = 0;

  uint32_t observed = 0;
  while ((observed & required_events) != required_events) {
    struct pollfd ready = {.fd = inotify_fd, .events = POLLIN};
    int poll_result;
    do {
      poll_result = poll(&ready, 1, 1000);
    } while (poll_result == -1 && errno == EINTR);
    if (poll_result == 0) {
      fputs("poll(inotify) timed out\n", stderr);
      goto cleanup;
    }
    if (poll_result == -1) {
      perror("poll(inotify)");
      goto cleanup;
    }

    _Alignas(struct inotify_event) char buffer[4096];
    const ssize_t length = read(inotify_fd, buffer, sizeof(buffer));
    if (length == -1 && errno == EAGAIN) {
      continue;
    }
    if (length == -1) {
      perror("read(inotify)");
      goto cleanup;
    }

    size_t offset = 0;
    while (offset + sizeof(struct inotify_event) <= (size_t)length) {
      const struct inotify_event *event =
          (const struct inotify_event *)(buffer + offset);
      const size_t event_size = sizeof(*event) + event->len;
      if (event_size > (size_t)length - offset) {
        fputs("truncated inotify event\n", stderr);
        goto cleanup;
      }
      if (event->wd == watch && event->len > 0U &&
          strcmp(event->name, file_name) == 0) {
        observed |= event->mask & required_events;
      }
      offset += event_size;
    }
  }

  printf("file=%s events=create,close_write,delete verification=ok\n",
         file_name);
  result = EXIT_SUCCESS;

cleanup:
  if (file_exists && unlink(file_path) == -1 && errno != ENOENT) {
    perror("cleanup unlink");
    result = EXIT_FAILURE;
  }
  if (watch != -1 && inotify_rm_watch(inotify_fd, watch) == -1 &&
      errno != EINVAL) {
    perror("inotify_rm_watch");
    result = EXIT_FAILURE;
  }
  if (inotify_fd != -1 && close(inotify_fd) == -1) {
    perror("close(inotify)");
    result = EXIT_FAILURE;
  }
  if (directory_created && rmdir(directory) == -1) {
    perror("rmdir");
    result = EXIT_FAILURE;
  }
  return result;
}
