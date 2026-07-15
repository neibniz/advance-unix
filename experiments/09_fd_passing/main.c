#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int write_all(int fd, const void *buffer, size_t length) {
  const unsigned char *bytes = buffer;
  size_t written = 0;

  while (written < length) {
    const ssize_t count = write(fd, bytes + written, length - written);
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

static int read_all(int fd, void *buffer, size_t length) {
  unsigned char *bytes = buffer;
  size_t received = 0;

  while (received < length) {
    const ssize_t count = read(fd, bytes + received, length - received);
    if (count > 0) {
      received += (size_t)count;
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

static pid_t wait_for_child(pid_t child, int *status) {
  pid_t waited;
  do {
    waited = waitpid(child, status, 0);
  } while (waited < 0 && errno == EINTR);
  return waited;
}

static void terminate_and_reap(pid_t child) {
  const int saved_errno = errno;
  int kill_result;
  do {
    kill_result = kill(child, SIGKILL);
  } while (kill_result < 0 && errno == EINTR);

  int status;
  (void)wait_for_child(child, &status);
  errno = saved_errno;
}

static int send_descriptor(int socket_fd, int descriptor) {
  char payload = 'F';
  struct iovec iov = {.iov_base = &payload, .iov_len = sizeof(payload)};
  union {
    struct cmsghdr alignment;
    unsigned char bytes[CMSG_SPACE(sizeof(int))];
  } control = {.bytes = {0}};
  struct msghdr message = {
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = control.bytes,
      .msg_controllen = sizeof(control.bytes),
  };

  struct cmsghdr *header = CMSG_FIRSTHDR(&message);
  header->cmsg_level = SOL_SOCKET;
  header->cmsg_type = SCM_RIGHTS;
  header->cmsg_len = CMSG_LEN(sizeof(descriptor));
  memcpy(CMSG_DATA(header), &descriptor, sizeof(descriptor));

  ssize_t sent;
  do {
    sent = sendmsg(socket_fd, &message, 0);
  } while (sent == -1 && errno == EINTR);
  if (sent == (ssize_t)sizeof(payload)) {
    return 0;
  }
  if (sent >= 0) {
    errno = EIO;
  }
  return -1;
}

static int receive_descriptor(int socket_fd) {
  char payload = 0;
  struct iovec iov = {.iov_base = &payload, .iov_len = sizeof(payload)};
  union {
    struct cmsghdr alignment;
    unsigned char bytes[CMSG_SPACE(4U * sizeof(int))];
  } control = {.bytes = {0}};
  struct msghdr message = {
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = control.bytes,
      .msg_controllen = sizeof(control.bytes),
  };

  int receive_flags = 0;
#ifdef MSG_CMSG_CLOEXEC
  receive_flags = MSG_CMSG_CLOEXEC;
#endif
  ssize_t received;
  do {
    received = recvmsg(socket_fd, &message, receive_flags);
  } while (received == -1 && errno == EINTR);
  if (received == -1) {
    return -1;
  }

  int descriptor = -1;
  size_t descriptor_count = 0;
  int malformed = 0;
  for (struct cmsghdr *header = CMSG_FIRSTHDR(&message); header != NULL;
       header = CMSG_NXTHDR(&message, header)) {
    if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS) {
      continue;
    }
    if (header->cmsg_len < CMSG_LEN(0)) {
      malformed = 1;
      continue;
    }

    const size_t data_size = (size_t)header->cmsg_len - CMSG_LEN(0);
    if (data_size == 0U || data_size % sizeof(int) != 0U) {
      malformed = 1;
      continue;
    }
    for (size_t offset = 0; offset < data_size; offset += sizeof(int)) {
      int candidate = -1;
      memcpy(&candidate, CMSG_DATA(header) + offset, sizeof(candidate));
      ++descriptor_count;
      if (descriptor < 0) {
        descriptor = candidate;
      } else {
        (void)close(candidate);
      }
    }
  }

  if (received != (ssize_t)sizeof(payload) || payload != 'F' || malformed ||
      descriptor_count != 1U ||
      (message.msg_flags & (MSG_CTRUNC | MSG_TRUNC)) != 0) {
    if (descriptor >= 0) {
      (void)close(descriptor);
    }
    errno = EBADMSG;
    return -1;
  }

  const int descriptor_flags = fcntl(descriptor, F_GETFD);
  if (descriptor_flags == -1 ||
      fcntl(descriptor, F_SETFD, descriptor_flags | FD_CLOEXEC) == -1) {
    const int saved_errno = errno;
    (void)close(descriptor);
    errno = saved_errno;
    return -1;
  }
  return descriptor;
}

int main(void) {
  static const char expected[] = "descriptor-passing-ok";
  int channels[2];
  if (socketpair(AF_UNIX, SOCK_DGRAM, 0, channels) == -1) {
    perror("socketpair");
    return EXIT_FAILURE;
  }

  const pid_t child = fork();
  if (child == -1) {
    perror("fork");
    close(channels[0]);
    close(channels[1]);
    return EXIT_FAILURE;
  }
  if (child == 0) {
    close(channels[0]);
    const int received_fd = receive_descriptor(channels[1]);
    if (received_fd == -1) {
      perror("recvmsg");
      _exit(2);
    }

    char buffer[sizeof(expected) - 1U] = {0};
    const int matches = read_all(received_fd, buffer, sizeof(buffer)) == 0 &&
                        memcmp(buffer, expected, sizeof(buffer)) == 0;
    close(received_fd);
    close(channels[1]);
    _exit(matches ? 0 : 3);
  }

  close(channels[1]);
  int pipe_fds[2];
  if (pipe(pipe_fds) == -1) {
    const int saved_errno = errno;
    close(channels[0]);
    errno = saved_errno;
    terminate_and_reap(child);
    perror("pipe");
    return EXIT_FAILURE;
  }
  if (write_all(pipe_fds[1], expected, sizeof(expected) - 1U) < 0) {
    const int saved_errno = errno;
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    close(channels[0]);
    errno = saved_errno;
    terminate_and_reap(child);
    perror("write pipe payload");
    return EXIT_FAILURE;
  }
  close(pipe_fds[1]);
  if (send_descriptor(channels[0], pipe_fds[0]) == -1) {
    const int saved_errno = errno;
    close(pipe_fds[0]);
    close(channels[0]);
    errno = saved_errno;
    terminate_and_reap(child);
    perror("sendmsg");
    return EXIT_FAILURE;
  }
  close(pipe_fds[0]);
  close(channels[0]);

  int status = 0;
  const pid_t waited = wait_for_child(child, &status);
  const int verified =
      waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  printf("SCM_RIGHTS payload=%s verification=%s\n", expected,
         verified ? "ok" : "failed");
  return verified ? EXIT_SUCCESS : EXIT_FAILURE;
}
