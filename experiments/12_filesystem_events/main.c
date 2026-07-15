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

static int write_all(int descriptor, const void *buffer, size_t size) {
  const unsigned char *bytes = buffer;
  size_t written = 0;

  while (written < size) {
    const ssize_t count = write(descriptor, bytes + written, size - written);
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

int main(void) {
  static const char file_name[] = "sample.txt";
  static const char content[] = "inotify\n";
  char directory[] = "./lab-inotify-XXXXXX";
  char file_path[sizeof(directory) + sizeof(file_name)] = {0};

  int inotify_fd = -1;
  int watch = -1;
  int file_fd = -1;
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

  file_fd = open(file_path, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
  if (file_fd == -1) {
    perror("open");
    goto cleanup;
  }
  file_exists = 1;
  if (write_all(file_fd, content, sizeof(content) - 1U) < 0) {
    perror("write");
    goto cleanup;
  }
  if (close(file_fd) < 0) {
    perror("close file");
    file_fd = -1;
    goto cleanup;
  }
  file_fd = -1;
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
    if ((ready.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
      fprintf(stderr, "unexpected inotify poll events: %#x\n",
              (unsigned int)ready.revents);
      goto cleanup;
    }

    _Alignas(struct inotify_event) char buffer[4096];
    ssize_t length;
    do {
      length = read(inotify_fd, buffer, sizeof(buffer));
    } while (length < 0 && errno == EINTR);
    if (length == -1 && errno == EAGAIN) {
      continue;
    }
    if (length == -1) {
      perror("read(inotify)");
      goto cleanup;
    }
    if (length == 0) {
      fputs("read(inotify) returned an unexpected EOF\n", stderr);
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
      if ((event->mask & IN_Q_OVERFLOW) != 0U) {
        fputs("inotify event queue overflowed\n", stderr);
        goto cleanup;
      }
      if (event->wd == watch && event->len > 0U &&
          strcmp(event->name, file_name) == 0) {
        observed |= event->mask & required_events;
      }
      offset += event_size;
    }
    if (offset != (size_t)length) {
      fputs("trailing partial inotify event\n", stderr);
      goto cleanup;
    }
  }

  printf("file=%s events=create,close_write,delete verification=ok\n",
         file_name);
  result = EXIT_SUCCESS;

cleanup:
  if (file_fd >= 0 && close(file_fd) < 0) {
    perror("cleanup close file");
    result = EXIT_FAILURE;
  }
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
