#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  QUEUE_CAPACITY = 4,
  ITEM_COUNT = 20,
};

struct queue {
  int items[QUEUE_CAPACITY];
  size_t head;
  size_t tail;
  size_t count;
  int produced;
  int consumed;
  long consumed_sum;
  pthread_mutex_t mutex;
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
};

static void check_pthread(int error, const char *operation) {
  if (error != 0) {
    fprintf(stderr, "%s: %s\n", operation, strerror(error));
    exit(EXIT_FAILURE);
  }
}

static void *producer(void *argument) {
  struct queue *queue = argument;

  for (int value = 1; value <= ITEM_COUNT; ++value) {
    check_pthread(pthread_mutex_lock(&queue->mutex), "pthread_mutex_lock");
    while (queue->count == QUEUE_CAPACITY) {
      check_pthread(pthread_cond_wait(&queue->not_full, &queue->mutex),
                    "pthread_cond_wait(not_full)");
    }

    queue->items[queue->tail] = value;
    queue->tail = (queue->tail + 1U) % QUEUE_CAPACITY;
    ++queue->count;
    ++queue->produced;

    check_pthread(pthread_cond_signal(&queue->not_empty),
                  "pthread_cond_signal(not_empty)");
    check_pthread(pthread_mutex_unlock(&queue->mutex), "pthread_mutex_unlock");
  }

  return NULL;
}

static void *consumer(void *argument) {
  struct queue *queue = argument;

  for (int index = 0; index < ITEM_COUNT; ++index) {
    check_pthread(pthread_mutex_lock(&queue->mutex), "pthread_mutex_lock");
    while (queue->count == 0U) {
      check_pthread(pthread_cond_wait(&queue->not_empty, &queue->mutex),
                    "pthread_cond_wait(not_empty)");
    }

    const int value = queue->items[queue->head];
    queue->head = (queue->head + 1U) % QUEUE_CAPACITY;
    --queue->count;
    ++queue->consumed;
    queue->consumed_sum += value;

    check_pthread(pthread_cond_signal(&queue->not_full),
                  "pthread_cond_signal(not_full)");
    check_pthread(pthread_mutex_unlock(&queue->mutex), "pthread_mutex_unlock");
  }

  return NULL;
}

int main(void) {
  static struct queue queue = {
      .mutex = PTHREAD_MUTEX_INITIALIZER,
      .not_empty = PTHREAD_COND_INITIALIZER,
      .not_full = PTHREAD_COND_INITIALIZER,
  };
  pthread_t producer_thread;
  pthread_t consumer_thread;

  check_pthread(pthread_create(&producer_thread, NULL, producer, &queue),
                "pthread_create(producer)");
  check_pthread(pthread_create(&consumer_thread, NULL, consumer, &queue),
                "pthread_create(consumer)");
  check_pthread(pthread_join(producer_thread, NULL), "pthread_join(producer)");
  check_pthread(pthread_join(consumer_thread, NULL), "pthread_join(consumer)");

  const long expected_sum = (long)ITEM_COUNT * (ITEM_COUNT + 1) / 2;
  const int verified = queue.produced == ITEM_COUNT &&
                       queue.consumed == ITEM_COUNT && queue.count == 0U &&
                       queue.consumed_sum == expected_sum;

  printf("produced=%d consumed=%d sum=%ld verification=%s\n", queue.produced,
         queue.consumed, queue.consumed_sum, verified ? "ok" : "failed");

  check_pthread(pthread_cond_destroy(&queue.not_full), "pthread_cond_destroy");
  check_pthread(pthread_cond_destroy(&queue.not_empty), "pthread_cond_destroy");
  check_pthread(pthread_mutex_destroy(&queue.mutex), "pthread_mutex_destroy");
  return verified ? EXIT_SUCCESS : EXIT_FAILURE;
}
