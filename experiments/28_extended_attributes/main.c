#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/xattr.h>
#include <unistd.h>

static bool list_contains(const char *list, size_t list_size,
                          const char *expected) {
  size_t offset = 0;

  while (offset < list_size) {
    const size_t remaining = list_size - offset;
    const size_t name_size = strnlen(list + offset, remaining);
    if (name_size == remaining) {
      return false;
    }
    if (strcmp(list + offset, expected) == 0) {
      return true;
    }
    offset += name_size + 1U;
  }
  return false;
}

int main(void) {
  static const char attribute_name[] = "user.advance_unix";
  static const char attribute_value[] = "descriptor metadata";
  const size_t value_size = sizeof(attribute_value) - 1U;
  char path[] = "./lab_extended_attributes.XXXXXX";
  char value_buffer[sizeof(attribute_value)] = {0};
  char *attribute_list = NULL;
  bool path_exists = false;
  int fd = -1;
  int result = EXIT_FAILURE;

  fd = mkstemp(path);
  if (fd < 0) {
    perror("mkstemp");
    goto cleanup;
  }
  path_exists = true;

  if (fsetxattr(fd, attribute_name, attribute_value, value_size, XATTR_CREATE) <
      0) {
    perror("fsetxattr");
    goto cleanup;
  }

  const ssize_t reported_size = fgetxattr(fd, attribute_name, NULL, 0);
  if (reported_size != (ssize_t)value_size) {
    if (reported_size < 0) {
      perror("fgetxattr size");
    } else {
      fprintf(stderr, "unexpected extended attribute size: %zd\n",
              reported_size);
    }
    goto cleanup;
  }

  const ssize_t read_size =
      fgetxattr(fd, attribute_name, value_buffer, sizeof(value_buffer));
  if (read_size != (ssize_t)value_size ||
      memcmp(value_buffer, attribute_value, value_size) != 0) {
    if (read_size < 0) {
      perror("fgetxattr value");
    } else {
      fprintf(stderr, "extended attribute value did not match\n");
    }
    goto cleanup;
  }

  const ssize_t list_size = flistxattr(fd, NULL, 0);
  if (list_size <= 0) {
    if (list_size < 0) {
      perror("flistxattr size");
    } else {
      fprintf(stderr, "extended attribute list is empty\n");
    }
    goto cleanup;
  }
  attribute_list = malloc((size_t)list_size);
  if (attribute_list == NULL) {
    perror("malloc attribute list");
    goto cleanup;
  }
  const ssize_t listed = flistxattr(fd, attribute_list, (size_t)list_size);
  if (listed != list_size ||
      !list_contains(attribute_list, (size_t)listed, attribute_name)) {
    if (listed < 0) {
      perror("flistxattr");
    } else {
      fprintf(stderr, "attribute name was absent from flistxattr result\n");
    }
    goto cleanup;
  }

  if (fremovexattr(fd, attribute_name) < 0) {
    perror("fremovexattr");
    goto cleanup;
  }
  errno = 0;
  if (fgetxattr(fd, attribute_name, value_buffer, sizeof(value_buffer)) >= 0 ||
      errno != ENODATA) {
    fprintf(stderr, "removed attribute did not report ENODATA\n");
    goto cleanup;
  }

  printf("xattr %s set, listed, read, and removed; ENODATA verified\n",
         attribute_name);
  result = EXIT_SUCCESS;

cleanup:
  free(attribute_list);
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
