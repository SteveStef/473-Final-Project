// You can ignore this file
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>

#define QUEUE_SIZE 100
#define NUMBER_OF_THREADS 5
#define TRUE 1

typedef struct {
  int row;
  int col;
  int length;
} TaskArgs;

typedef struct {
  void (*function)(void *args);
  void *args;
} Task;

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

int **matrixA; // input
int **matrixB; // input
int **matrixC; // output

int enqueue(Task t);
Task dequeue();
void compute_partial_product(void *arg);

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
}

// have the worker call this function to execute the work
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
      sem_post(&all_tasks_completed); // Signal the main thread!
    }
    pthread_mutex_unlock(&lock);
  }

  return NULL;
}

void pool_init(int num_threads) {
  thread_pool = malloc(num_threads * sizeof(pthread_t));
  task_list = calloc(QUEUE_SIZE, sizeof(Task));
  dequeue_index = 0; // dequeue from here
  enqueue_index = 0; // enqueue from here
  pthread_mutex_init(&lock, NULL);
  sem_init(&sem, 0, 0);
  sem_init(&all_tasks_completed, 0, 0);
  for (int i = 0; i < num_threads; i++) {
    pthread_create(&thread_pool[i], NULL, worker, NULL);
  }
}

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
    printf("enqueue failed");
  }
  pthread_mutex_unlock(&lock);

  return 1;
}

int** fillMatrix(int r, int c, int with0s) {
  int **mat = malloc(r * sizeof(int *));
  for (int i = 0; i < r; i++) {
    mat[i] = malloc(c * sizeof(int));
  }

  for (int i = 0; i < r; i++) {
    for (int j = 0; j < c; j++) {
      if(!with0s) mat[i][j] = rand() % 10;
      else mat[i][j] = 0;
    }
  }

  return mat;
}

void printMatrix(int **matrix, int rows, int cols) {
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      printf("%d ", matrix[i][j]);
    }
    printf("\n");
  }
}

void compute_partial_product(void *arg) {
  TaskArgs *args = (TaskArgs *)arg;

  int sum = 0;
  for (int k = 0; k < args->length; k++) {
    if (matrixA[args->row][k] != 0) {
      sum += matrixA[args->row][k] * matrixB[k][args->col];
    }
  }
  matrixC[args->row][args->col] = sum;
  free(args);
}

void save_result_to_file(const char *filename, int M, int N, int P, double time) {
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        return;
    }
    fprintf(f, "Time taken: %f seconds\n", time);
    fprintf(f, "Matrix A (%d x %d):\n", M, N);
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            fprintf(f, "%d ", matrixA[i][j]);
        }
        fprintf(f, "\n");
    }
    fprintf(f, "\n");

    fprintf(f, "Matrix B (%d x %d):\n", N, P);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < P; j++) {
            fprintf(f, "%d ", matrixB[i][j]);
        }
        fprintf(f, "\n");
    }
    fprintf(f, "\n");

    fprintf(f, "Result Matrix C (%d x %d):\n", M, P);
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < P; j++) {
            fprintf(f, "%d ", matrixC[i][j]);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    printf("Results saved to output.txt");
}


int main(int argc, char *argv[]) {

  if (argc != 4) {
    return 1;
  }

  int M = atoi(argv[1]);
  int N = atoi(argv[2]);
  int P = atoi(argv[3]);

  total_tasks = M * P;

  pool_init(NUMBER_OF_THREADS);

  // Make 2 matrix where 1 is M x N and other is N x P
  matrixA = fillMatrix(M, N, 0);
  matrixB = fillMatrix(N, P, 0);
  matrixC = fillMatrix(M, P, 1); // this will hold the output


  struct timeval start, end;
  gettimeofday(&start, NULL);

  for (int i = 0; i < M; i++) {
    for (int j = 0; j < P; j++) {
      TaskArgs *targs = malloc(sizeof(TaskArgs));
      targs->row = i;
      targs->col = j;
      targs->length = N;
      pool_submit(compute_partial_product, targs);
    }
  }

  sem_wait(&all_tasks_completed); 

  gettimeofday(&end, NULL);
  double time_taken = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
  printf("Wall time: %f seconds\n", time_taken);

  save_result_to_file("output.txt", M, N, P, time_taken);

  pool_shutdown();

  // printMatrix(matrixA, M, N);
  // printf("\n");
  // printMatrix(matrixB, N, P);
  // printf("\n");
  // printMatrix(matrixC, M, P);
  // printf("\n");

  for (int i = 0; i < M; i++) {
    free(matrixA[i]);
    free(matrixC[i]);
  }

  for (int i = 0; i < N; i++) {
    free(matrixB[i]);
  }

  free(matrixA);
  free(matrixB);
  free(matrixC);

  return 0;
}
