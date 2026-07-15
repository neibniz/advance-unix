#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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
  return sent == (ssize_t)sizeof(payload) ? 0 : -1;
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

    char buffer[sizeof(expected)] = {0};
    ssize_t bytes_read;
    do {
      bytes_read = read(received_fd, buffer, sizeof(buffer));
    } while (bytes_read == -1 && errno == EINTR);
    const int matches = bytes_read == (ssize_t)(sizeof(expected) - 1U) &&
                        memcmp(buffer, expected, sizeof(expected) - 1U) == 0;
    close(received_fd);
    close(channels[1]);
    _exit(matches ? 0 : 3);
  }

  close(channels[1]);
  int pipe_fds[2];
  if (pipe(pipe_fds) == -1) {
    perror("pipe");
    close(channels[0]);
    (void)waitpid(child, NULL, 0);
    return EXIT_FAILURE;
  }
  const ssize_t written = write(pipe_fds[1], expected, sizeof(expected) - 1U);
  close(pipe_fds[1]);
  if (written != (ssize_t)(sizeof(expected) - 1U) ||
      send_descriptor(channels[0], pipe_fds[0]) == -1) {
    perror("sendmsg/write");
    close(pipe_fds[0]);
    close(channels[0]);
    (void)waitpid(child, NULL, 0);
    return EXIT_FAILURE;
  }
  close(pipe_fds[0]);
  close(channels[0]);

  int status = 0;
  pid_t waited;
  do {
    waited = waitpid(child, &status, 0);
  } while (waited == -1 && errno == EINTR);
  const int verified =
      waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  printf("SCM_RIGHTS payload=%s verification=%s\n", expected,
         verified ? "ok" : "failed");
  return verified ? EXIT_SUCCESS : EXIT_FAILURE;
}
