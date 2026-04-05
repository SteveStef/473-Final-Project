#include "threadpool.h"

pthread_t *thread_pool;
Task *task_list;
int enqueue_index = 0;
int dequeue_index = 0;
int queue_count = 0;

int total_tasks = 0;
int completed_tasks = 0;

pthread_mutex_t lock;
sem_t sem;
sem_t all_tasks_completed;

int enqueue(Task t) {
  if (queue_count == QUEUE_SIZE) return 1;

  task_list[enqueue_index] = t;
  enqueue_index += 1;
  if (enqueue_index >= QUEUE_SIZE) enqueue_index = 0;
  queue_count++;
  return 0;
}

Task dequeue() {
  Task t = task_list[dequeue_index];
  dequeue_index += 1;
  if (dequeue_index >= QUEUE_SIZE) dequeue_index = 0;
  queue_count--;
  return t;
}

void execute(void (*somefunction)(void *p), void *p) {
  (*somefunction)(p);
}

void* worker(void *args) {
  while (1) {
    sem_wait(&sem); // Wait for work

    pthread_mutex_lock(&lock);
    if (completed_tasks >= total_tasks) {
      pthread_mutex_unlock(&lock);
      break; 
    }

    Task t = dequeue();

    pthread_mutex_unlock(&lock);

    execute(t.function, t.args);

    pthread_mutex_lock(&lock);
    completed_tasks++;

    // Check if this was the final task
    if (completed_tasks == total_tasks) {
      sem_post(&all_tasks_completed); // we are done here
    }
    pthread_mutex_unlock(&lock);
  }

  return NULL;
}

void pool_init(int num_threads) {
  thread_pool = malloc(num_threads * sizeof(pthread_t));
  task_list = calloc(QUEUE_SIZE, sizeof(Task));
  pthread_mutex_init(&lock, NULL);
  sem_init(&sem, 0, 0);
  sem_init(&all_tasks_completed, 0, 0);
  for (int i = 0; i < num_threads; i++) {
    pthread_create(&thread_pool[i], NULL, worker, NULL);
  }
}

int pool_submit(void (*somefunction)(void *p), void *p) {
  Task t;

  t.function = somefunction;
  t.args = p;

  pthread_mutex_lock(&lock);
  if (enqueue(t) == 0) {
    sem_post(&sem);   // wakes one worker thread
    pthread_mutex_unlock(&lock);
    return 0;
  } else {
    printf("\nenqueue failed\n");
  }
  pthread_mutex_unlock(&lock);

  return 1;
}

void pool_shutdown(void) {
  for (int i = 0; i < NUMBER_OF_THREADS; i++) {
    sem_post(&sem);
  }

  for (int i = 0; i < NUMBER_OF_THREADS; i++) {
    pthread_join(thread_pool[i], NULL);
  }

  free(task_list);
  free(thread_pool);
  pthread_mutex_destroy(&lock);
  sem_destroy(&sem);
  sem_destroy(&all_tasks_completed);
}
