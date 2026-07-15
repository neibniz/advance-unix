#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

enum {
  EVENTFD_ID = 1,
  TIMERFD_ID = 2,
};

int main(void) {
  int epoll_fd = -1;
  int event_fd = -1;
  int timer_fd = -1;
  int result = EXIT_FAILURE;
  uint64_t event_value = 0;
  uint64_t timer_expirations = 0;
  int saw_event = 0;
  int saw_timer = 0;

  epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
  if (epoll_fd == -1 || event_fd == -1 || timer_fd == -1) {
    perror("epoll_create1/eventfd/timerfd_create");
    goto cleanup;
  }

  struct epoll_event registration = {
      .events = EPOLLIN,
      .data.u32 = EVENTFD_ID,
  };
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_fd, &registration) == -1) {
    perror("epoll_ctl(eventfd)");
    goto cleanup;
  }
  registration.data.u32 = TIMERFD_ID;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &registration) == -1) {
    perror("epoll_ctl(timerfd)");
    goto cleanup;
  }

  const struct itimerspec timer = {
      .it_value = {.tv_sec = 0, .tv_nsec = 20 * 1000 * 1000},
  };
  if (timerfd_settime(timer_fd, 0, &timer, NULL) == -1) {
    perror("timerfd_settime");
    goto cleanup;
  }
  const uint64_t wake_value = 7;
  if (write(event_fd, &wake_value, sizeof(wake_value)) !=
      (ssize_t)sizeof(wake_value)) {
    perror("write(eventfd)");
    goto cleanup;
  }

  while (!saw_event || !saw_timer) {
    struct epoll_event events[2];
    int ready;
    do {
      ready = epoll_wait(epoll_fd, events, 2, 1000);
    } while (ready == -1 && errno == EINTR);
    if (ready == 0) {
      fputs("epoll_wait timed out\n", stderr);
      goto cleanup;
    }
    if (ready == -1) {
      perror("epoll_wait");
      goto cleanup;
    }

    for (int index = 0; index < ready; ++index) {
      if (events[index].data.u32 == EVENTFD_ID && !saw_event) {
        if (read(event_fd, &event_value, sizeof(event_value)) !=
            (ssize_t)sizeof(event_value)) {
          perror("read(eventfd)");
          goto cleanup;
        }
        saw_event = 1;
      } else if (events[index].data.u32 == TIMERFD_ID && !saw_timer) {
        if (read(timer_fd, &timer_expirations, sizeof(timer_expirations)) !=
            (ssize_t)sizeof(timer_expirations)) {
          perror("read(timerfd)");
          goto cleanup;
        }
        saw_timer = 1;
      }
    }
  }

  const int verified = event_value == wake_value && timer_expirations == 1;
  printf("eventfd=%llu timer_expirations=%llu verification=%s\n",
         (unsigned long long)event_value, (unsigned long long)timer_expirations,
         verified ? "ok" : "failed");
  result = verified ? EXIT_SUCCESS : EXIT_FAILURE;

cleanup:
  if (timer_fd != -1) {
    close(timer_fd);
  }
  if (event_fd != -1) {
    close(event_fd);
  }
  if (epoll_fd != -1) {
    close(epoll_fd);
  }
  return result;
}
