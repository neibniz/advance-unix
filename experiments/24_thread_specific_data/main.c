#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { THREAD_COUNT = 3 };

struct thread_value {
  int id;
};

struct worker_context {
  int id;
  int verified;
};

static pthread_key_t value_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;
static int key_error = 0;
static int key_initialized = 0;
static atomic_int destructor_count;
static atomic_int destructor_id_sum;

static void destroy_thread_value(void *value) {
  struct thread_value *thread_value = value;
  if (thread_value != NULL) {
    atomic_fetch_add_explicit(&destructor_count, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&destructor_id_sum, thread_value->id,
                              memory_order_relaxed);
    free(thread_value);
  }
}

static void initialize_key(void) {
  key_error = pthread_key_create(&value_key, destroy_thread_value);
  key_initialized = key_error == 0;
}

static void *worker(void *argument) {
  struct worker_context *context = argument;
  const int once_error = pthread_once(&key_once, initialize_key);
  if (once_error != 0 || key_error != 0) {
    return NULL;
  }

  struct thread_value *value = malloc(sizeof(*value));
  if (value == NULL) {
    return NULL;
  }
  value->id = context->id;

  const int set_error = pthread_setspecific(value_key, value);
  if (set_error != 0) {
    free(value);
    return NULL;
  }

  const struct thread_value *observed = pthread_getspecific(value_key);
  context->verified = observed == value && observed->id == context->id;
  return NULL;
}

int main(void) {
  pthread_t threads[THREAD_COUNT];
  struct worker_context contexts[THREAD_COUNT] = {0};
  int created = 0;
  int result = EXIT_FAILURE;

  for (int index = 0; index < THREAD_COUNT; ++index) {
    contexts[index].id = index + 1;
    const int error =
        pthread_create(&threads[index], NULL, worker, &contexts[index]);
    if (error != 0) {
      fprintf(stderr, "pthread_create: %s\n", strerror(error));
      break;
    }
    ++created;
  }

  int join_failed = 0;
  for (int index = 0; index < created; ++index) {
    const int error = pthread_join(threads[index], NULL);
    if (error != 0) {
      fprintf(stderr, "pthread_join: %s\n", strerror(error));
      join_failed = 1;
    }
  }

  if (join_failed) {
    const int destroyed =
        atomic_load_explicit(&destructor_count, memory_order_relaxed);
    const int id_sum =
        atomic_load_explicit(&destructor_id_sum, memory_order_relaxed);
    fprintf(stderr, "key retained because a thread may still be running\n");
    printf("threads=%d destructors=%d id_sum=%d verification=failed\n", created,
           destroyed, id_sum);
    return EXIT_FAILURE;
  }

  int workers_verified = created == THREAD_COUNT;
  for (int index = 0; index < created; ++index) {
    workers_verified = workers_verified && contexts[index].verified;
  }

  const int destroyed =
      atomic_load_explicit(&destructor_count, memory_order_relaxed);
  const int id_sum =
      atomic_load_explicit(&destructor_id_sum, memory_order_relaxed);
  const int expected_sum = THREAD_COUNT * (THREAD_COUNT + 1) / 2;
  const int verified = workers_verified && key_initialized && key_error == 0 &&
                       destroyed == THREAD_COUNT && id_sum == expected_sum;

  if (key_error != 0) {
    fprintf(stderr, "pthread_key_create: %s\n", strerror(key_error));
  } else if (!key_initialized) {
    fprintf(stderr, "thread-specific key was never initialized\n");
  } else {
    const int error = pthread_key_delete(value_key);
    if (error != 0) {
      fprintf(stderr, "pthread_key_delete: %s\n", strerror(error));
    } else if (verified) {
      result = EXIT_SUCCESS;
    }
  }

  printf("threads=%d destructors=%d id_sum=%d verification=%s\n", created,
         destroyed, id_sum, result == EXIT_SUCCESS ? "ok" : "failed");
  return result;
}
