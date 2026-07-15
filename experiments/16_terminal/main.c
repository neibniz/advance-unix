#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int transfer_all(int fd, void *data, size_t size, bool writing) {
  unsigned char *cursor = data;
  size_t completed = 0;

  while (completed < size) {
    const ssize_t count = writing
                              ? write(fd, cursor + completed, size - completed)
                              : read(fd, cursor + completed, size - completed);
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
    completed += (size_t)count;
  }
  return 0;
}

static void make_raw(struct termios *attributes) {
  attributes->c_iflag &=
      (tcflag_t) ~(tcflag_t)(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                             ICRNL | IXON);
  attributes->c_oflag &= (tcflag_t) ~(tcflag_t)OPOST;
  attributes->c_lflag &=
      (tcflag_t) ~(tcflag_t)(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  attributes->c_cflag &= (tcflag_t) ~(tcflag_t)(CSIZE | PARENB);
  attributes->c_cflag |= (tcflag_t)CS8;
  attributes->c_cc[VMIN] = (cc_t)1;
  attributes->c_cc[VTIME] = (cc_t)0;
}

int main(void) {
  static const char to_slave[] = "master-to-slave\n";
  static const char to_master[] = "slave-to-master\n";
  char slave_buffer[sizeof(to_slave)] = {0};
  char master_buffer[sizeof(to_master)] = {0};
  struct termios saved = {0};
  struct termios raw = {0};
  int master_fd = -1;
  int slave_fd = -1;
  bool attributes_changed = false;
  int result = EXIT_FAILURE;

  master_fd = posix_openpt(O_RDWR | O_NOCTTY);
  if (master_fd < 0) {
    perror("posix_openpt");
    goto cleanup;
  }
  if (grantpt(master_fd) < 0 || unlockpt(master_fd) < 0) {
    perror("grantpt/unlockpt");
    goto cleanup;
  }
  const char *slave_name = ptsname(master_fd);
  if (slave_name == NULL) {
    perror("ptsname");
    goto cleanup;
  }
  slave_fd = open(slave_name, O_RDWR | O_NOCTTY);
  if (slave_fd < 0) {
    perror("open PTY slave");
    goto cleanup;
  }
  if (tcgetattr(slave_fd, &saved) < 0) {
    perror("tcgetattr");
    goto cleanup;
  }
  raw = saved;
  make_raw(&raw);
  if (tcsetattr(slave_fd, TCSANOW, &raw) < 0) {
    perror("tcsetattr raw");
    goto cleanup;
  }
  attributes_changed = true;

  if (transfer_all(master_fd, (void *)to_slave, sizeof(to_slave) - 1U, true) <
          0 ||
      transfer_all(slave_fd, slave_buffer, sizeof(to_slave) - 1U, false) < 0) {
    perror("master-to-slave transfer");
    goto cleanup;
  }
  if (transfer_all(slave_fd, (void *)to_master, sizeof(to_master) - 1U, true) <
          0 ||
      transfer_all(master_fd, master_buffer, sizeof(to_master) - 1U, false) <
          0) {
    perror("slave-to-master transfer");
    goto cleanup;
  }
  if (memcmp(slave_buffer, to_slave, sizeof(to_slave) - 1U) != 0 ||
      memcmp(master_buffer, to_master, sizeof(to_master) - 1U) != 0) {
    fprintf(stderr, "PTY data verification failed\n");
    goto cleanup;
  }

  printf("PTY %s transferred data in both directions\n", slave_name);
  result = EXIT_SUCCESS;

cleanup:
  if (attributes_changed && tcsetattr(slave_fd, TCSANOW, &saved) < 0) {
    perror("cleanup: restore termios");
    result = EXIT_FAILURE;
  }
  if (slave_fd >= 0) {
    (void)close(slave_fd);
  }
  if (master_fd >= 0) {
    (void)close(master_fd);
  }
  return result;
}
