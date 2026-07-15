#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int set_nonblocking(int descriptor, int enabled) {
  int flags;
  do {
    flags = fcntl(descriptor, F_GETFL);
  } while (flags == -1 && errno == EINTR);
  if (flags == -1) {
    return -1;
  }
  const int updated = enabled ? flags | O_NONBLOCK : flags & ~O_NONBLOCK;
  int result;
  do {
    result = fcntl(descriptor, F_SETFL, updated);
  } while (result == -1 && errno == EINTR);
  return result;
}

static int finish_nonblocking_connect(int descriptor) {
  struct pollfd ready = {.fd = descriptor, .events = POLLOUT};
  int poll_result;
  do {
    poll_result = poll(&ready, 1, 2000);
  } while (poll_result == -1 && errno == EINTR);
  if (poll_result == 0) {
    errno = ETIMEDOUT;
    return -1;
  }
  if (poll_result == -1 || (ready.revents & POLLNVAL) != 0) {
    if ((ready.revents & POLLNVAL) != 0) {
      errno = EBADF;
    }
    return -1;
  }

  int socket_error = 0;
  socklen_t error_size = sizeof(socket_error);
  int option_result;
  do {
    option_result = getsockopt(descriptor, SOL_SOCKET, SO_ERROR, &socket_error,
                               &error_size);
  } while (option_result == -1 && errno == EINTR);
  if (option_result == -1) {
    return -1;
  }
  if (socket_error != 0) {
    errno = socket_error;
    return -1;
  }
  return 0;
}

static int send_all(int descriptor, const void *buffer, size_t length) {
  size_t offset = 0;
  while (offset < length) {
    const ssize_t sent = send(descriptor, (const char *)buffer + offset,
                              length - offset, MSG_NOSIGNAL);
    if (sent > 0) {
      offset += (size_t)sent;
    } else if (sent == -1 && errno == EINTR) {
      continue;
    } else {
      if (sent == 0) {
        errno = EIO;
      }
      return -1;
    }
  }
  return 0;
}

static int receive_all(int descriptor, void *buffer, size_t length) {
  size_t offset = 0;
  while (offset < length) {
    const ssize_t received =
        recv(descriptor, (char *)buffer + offset, length - offset, 0);
    if (received > 0) {
      offset += (size_t)received;
    } else if (received == -1 && errno == EINTR) {
      continue;
    } else {
      if (received == 0) {
        errno = ECONNRESET;
      }
      return -1;
    }
  }
  return 0;
}

int main(void) {
  static const char request[] = "ping";
  static const char response[] = "pong";
  struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_STREAM,
      .ai_flags = AI_NUMERICHOST | AI_NUMERICSERV,
  };
  struct addrinfo *server_addresses = NULL;
  struct addrinfo *client_addresses = NULL;
  int listener = -1;
  int client = -1;
  int peer = -1;
  int result = EXIT_FAILURE;

  int address_result = getaddrinfo("127.0.0.1", "0", &hints, &server_addresses);
  if (address_result != 0) {
    fprintf(stderr, "getaddrinfo(server): %s\n", gai_strerror(address_result));
    goto cleanup;
  }

  listener = socket(server_addresses->ai_family,
                    server_addresses->ai_socktype | SOCK_CLOEXEC,
                    server_addresses->ai_protocol);
  if (listener == -1) {
    perror("socket(listener)");
    goto cleanup;
  }
  const int reuse_address = 1;
  if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse_address,
                 sizeof(reuse_address)) == -1 ||
      bind(listener, server_addresses->ai_addr, server_addresses->ai_addrlen) ==
          -1 ||
      listen(listener, 4) == -1) {
    perror("setsockopt/bind/listen");
    goto cleanup;
  }
  freeaddrinfo(server_addresses);
  server_addresses = NULL;

  struct sockaddr_in bound_address = {0};
  socklen_t bound_size = sizeof(bound_address);
  if (getsockname(listener, (struct sockaddr *)&bound_address, &bound_size) ==
      -1) {
    perror("getsockname");
    goto cleanup;
  }
  if (bound_size != (socklen_t)sizeof(bound_address) ||
      bound_address.sin_family != AF_INET) {
    fputs("getsockname returned an unexpected address shape\n", stderr);
    goto cleanup;
  }
  char service[6];
  const unsigned int port = ntohs(bound_address.sin_port);
  const int service_length = snprintf(service, sizeof(service), "%u", port);
  if (service_length < 0 || service_length >= (int)sizeof(service)) {
    fputs("ephemeral port did not fit in service buffer\n", stderr);
    goto cleanup;
  }

  address_result = getaddrinfo("127.0.0.1", service, &hints, &client_addresses);
  if (address_result != 0) {
    fprintf(stderr, "getaddrinfo(client): %s\n", gai_strerror(address_result));
    goto cleanup;
  }
  client = socket(client_addresses->ai_family,
                  client_addresses->ai_socktype | SOCK_CLOEXEC,
                  client_addresses->ai_protocol);
  if (client == -1 || set_nonblocking(client, 1) == -1) {
    perror("socket/fcntl(client)");
    goto cleanup;
  }

  if (connect(client, client_addresses->ai_addr,
              client_addresses->ai_addrlen) == -1) {
    const int connect_error = errno;
    if ((connect_error != EINPROGRESS && connect_error != EALREADY &&
         connect_error != EINTR) ||
        finish_nonblocking_connect(client) == -1) {
      perror("connect/poll");
      goto cleanup;
    }
  }
  if (set_nonblocking(client, 0) == -1) {
    perror("fcntl(client, blocking)");
    goto cleanup;
  }
  freeaddrinfo(client_addresses);
  client_addresses = NULL;

  do {
    peer = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
  } while (peer == -1 && errno == EINTR);
  if (peer == -1) {
    perror("accept4");
    goto cleanup;
  }

  char received_request[sizeof(request) - 1U];
  char received_response[sizeof(response) - 1U];
  if (send_all(client, request, sizeof(request) - 1U) == -1 ||
      receive_all(peer, received_request, sizeof(received_request)) == -1 ||
      send_all(peer, response, sizeof(response) - 1U) == -1 ||
      receive_all(client, received_response, sizeof(received_response)) == -1) {
    perror("loopback exchange");
    goto cleanup;
  }

  const int verified =
      memcmp(received_request, request, sizeof(received_request)) == 0 &&
      memcmp(received_response, response, sizeof(received_response)) == 0;
  printf("loopback=127.0.0.1:%u request=%s response=%s verification=%s\n", port,
         request, response, verified ? "ok" : "failed");
  result = verified ? EXIT_SUCCESS : EXIT_FAILURE;

cleanup:
  if (server_addresses != NULL) {
    freeaddrinfo(server_addresses);
  }
  if (client_addresses != NULL) {
    freeaddrinfo(client_addresses);
  }
  if (peer != -1) {
    close(peer);
  }
  if (client != -1) {
    close(client);
  }
  if (listener != -1) {
    close(listener);
  }
  return result;
}
