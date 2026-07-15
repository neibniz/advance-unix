#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef double (*unary_math_function)(double);

int main(void) {
  void *handle = NULL;
  int result = EXIT_FAILURE;

  handle = dlopen("libm.so.6", RTLD_NOW | RTLD_LOCAL);
  if (handle == NULL) {
    const char *const open_error = dlerror();
    fprintf(stderr, "dlopen libm.so.6: %s\n",
            open_error != NULL ? open_error : "unknown error");
    goto cleanup;
  }

  (void)dlerror();
  void *const symbol = dlsym(handle, "cos");
  const char *const symbol_error = dlerror();
  if (symbol_error != NULL) {
    fprintf(stderr, "dlsym cos: %s\n", symbol_error);
    goto cleanup;
  }

  unary_math_function cosine = NULL;
  _Static_assert(sizeof(cosine) == sizeof(symbol),
                 "function and object pointers must have equal size");
  memcpy(&cosine, &symbol, sizeof(cosine));
  if (cosine == NULL) {
    fprintf(stderr, "dlsym returned a null cos symbol\n");
    goto cleanup;
  }

  const double value = cosine(0.0);
  if (value != 1.0) {
    fprintf(stderr, "cos(0.0) returned %.17g instead of 1\n", value);
    goto cleanup;
  }

  printf("dlopen(libm.so.6) resolved cos(0.0) = %.1f\n", value);
  result = EXIT_SUCCESS;

cleanup:
  if (handle != NULL && dlclose(handle) != 0) {
    const char *const close_error = dlerror();
    fprintf(stderr, "dlclose: %s\n",
            close_error != NULL ? close_error : "unknown error");
    result = EXIT_FAILURE;
  }
  return result;
}
