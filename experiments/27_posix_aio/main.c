#include <aio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_all(int fd, const void *data, size_t size) {
  const unsigned char *cursor = data;
  size_t written = 0;

  while (written < size) {
    const ssize_t count = write(fd, cursor + written, size - written);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (count == 0) {
      errno = EIO;
      return -1;
    }
    written += (size_t)count;
  }
  return 0;
}

static int wait_until_complete(const struct aiocb *request, int *status) {
  const struct aiocb *const requests[] = {request};

  for (;;) {
    const int current = aio_error(request);
    if (current < 0) {
      return -1;
    }
    if (current != EINPROGRESS) {
      *status = current;
      return 0;
    }

    int suspend_result;
    do {
      suspend_result = aio_suspend(requests, 1, NULL);
    } while (suspend_result < 0 && errno == EINTR);
    if (suspend_result < 0) {
      return -1;
    }
  }
}

static int cancel_and_reap(struct aiocb *request) {
  int first_error = 0;
  int status = aio_error(request);

  if (status < 0) {
    first_error = errno;
  } else if (status == EINPROGRESS &&
             aio_cancel(request->aio_fildes, request) < 0) {
    first_error = errno;
  }

  if (wait_until_complete(request, &status) < 0) {
    if (first_error == 0) {
      first_error = errno;
    }
  } else {
    errno = 0;
    const ssize_t ignored = aio_return(request);
    if (ignored < 0 && status == 0 && first_error == 0) {
      first_error = errno != 0 ? errno : EIO;
    }
  }

  if (first_error != 0) {
    errno = first_error;
    return -1;
  }
  return 0;
}

int main(void) {
  static const char payload[] = "POSIX asynchronous file read";
  const size_t payload_size = sizeof(payload) - 1U;
  char path[] = "./lab_posix_aio.XXXXXX";
  char buffer[sizeof(payload)] = {0};
  struct aiocb request = {0};
  bool path_exists = false;
  bool request_pending = false;
  int fd = -1;
  int result = EXIT_FAILURE;

  fd = mkstemp(path);
  if (fd < 0) {
    perror("mkstemp");
    goto cleanup;
  }
  path_exists = true;
  if (unlink(path) < 0) {
    perror("unlink temporary file");
    goto cleanup;
  }
  path_exists = false;

  if (write_all(fd, payload, payload_size) < 0) {
    perror("write temporary file");
    goto cleanup;
  }

  request.aio_fildes = fd;
  request.aio_buf = buffer;
  request.aio_nbytes = payload_size;
  request.aio_offset = 0;
  request.aio_sigevent.sigev_notify = SIGEV_NONE;
  if (aio_read(&request) < 0) {
    perror("aio_read");
    goto cleanup;
  }
  request_pending = true;

  int aio_status = 0;
  if (wait_until_complete(&request, &aio_status) < 0) {
    perror("aio_suspend/aio_error");
    goto cleanup;
  }
  if (aio_status != 0) {
    errno = aio_status;
    perror("asynchronous read");
    goto cleanup;
  }

  const ssize_t received = aio_return(&request);
  request_pending = false;
  if (received != (ssize_t)payload_size ||
      memcmp(buffer, payload, payload_size) != 0) {
    fprintf(stderr, "asynchronous read verification failed\n");
    goto cleanup;
  }

  printf("POSIX AIO read %zd bytes; content matched\n", received);
  result = EXIT_SUCCESS;

cleanup:
  if (request_pending && cancel_and_reap(&request) < 0) {
    perror("cancel/reap asynchronous request");
    result = EXIT_FAILURE;
  }
  if (fd >= 0 && close(fd) < 0) {
    perror("close");
    result = EXIT_FAILURE;
  }
  if (path_exists && unlink(path) < 0) {
    perror("unlink");
    result = EXIT_FAILURE;
  }
  return result;
}
