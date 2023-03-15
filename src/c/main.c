/** #include <bits/pthreadtypes.h> */
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum {
  MEM_AVAILABLE,
  MEM_USED,
} BuffMemState;

typedef enum {
  NO_WRITING,
  WRITING,
} WritingState;

typedef struct {
  void *mem;
  char *state_array;
  const int sizeof_elem;
  const int n_elements;
  size_t mem_idx;
  size_t disk_idx;
  size_t n_mem_elements;
  size_t n_disk_elements;

  WritingState w_state;

  FILE *buff_file;

  pthread_t writer_tid;

  pthread_mutex_t write_mem_mtx;
  pthread_cond_t write_mem_cond;

  pthread_mutex_t write_disk_mtx;
  pthread_cond_t write_disk_cond;

  pthread_mutex_t state_mtx; // controls access to state_array

} CircularBuffer;

typedef struct {
  void *buffer_segment_1;
  size_t size_buff_1;

  void *buffer_segment_2;
  size_t size_buff_2;

} BuffSegmets;

BuffSegmets computeBufferSegments(CircularBuffer *const cb) {
  BuffSegmets segments;
  pthread_mutex_lock(&cb->state_mtx);

  if (cb->disk_idx > cb->mem_idx) {
    // segmemt 1 is all the elements from disk_idx to the last element in buff
    segments.buffer_segment_1 = &(cb->mem[cb->disk_idx * cb->sizeof_elem]);
    segments.size_buff_1 = (cb->n_elements - cb->disk_idx) * cb->sizeof_elem;
    // segmemt 2 is all the elements from 0 to mem_idx
    segments.buffer_segment_2 = cb->mem;
    segments.size_buff_2 = (cb->mem_idx - 1) * cb->sizeof_elem;
  } else {
    // segment one is all the elements from disk_idx to mem_idx
    segments.buffer_segment_1 = &(cb->mem[cb->disk_idx * cb->sizeof_elem]);
    segments.size_buff_1 = (cb->mem_idx - cb->disk_idx - 1) * cb->sizeof_elem;

    // segment 2 is empty
    segments.buffer_segment_2 = NULL;
    segments.size_buff_2 = 0;
  }
  pthread_mutex_unlock(&cb->state_mtx);
  return segments;
}

void *writeToDiskCircularBuffer(void *cb_ptr) {
  CircularBuffer *const cb = (CircularBuffer *)cb_ptr;

  while (1) {
    printf("computing buff segments\n");
    BuffSegmets bs = computeBufferSegments(cb);
    printf("computed buff segments\n");
    // set the state as writting
    pthread_mutex_lock(&cb->state_mtx);
    printf("setting w_state to WRIING\n");
    cb->w_state = WRITING;
    pthread_mutex_unlock(&cb->state_mtx);

    printf("writing to disk\n");
    if (bs.size_buff_1 != 0) {
      fwrite(bs.buffer_segment_1, bs.size_buff_1, 1, cb->buff_file);
    }
    if (bs.size_buff_2 != 0) {
      fwrite(bs.buffer_segment_2, bs.size_buff_2, 1, cb->buff_file);
    }

    // locks until all variables have been set to their correct values
    pthread_mutex_lock(&cb->state_mtx);

    printf("setting w_state to NO_WRIING\n");
    cb->w_state = NO_WRITING;

    // set state_array to available where needed
    printf("setting state_array\n");
    const size_t freed_elemts =
        (bs.size_buff_1 + bs.size_buff_2) / cb->sizeof_elem;
    for (size_t i = 0; i < freed_elemts; i++) {
      cb->state_array[(cb->disk_idx + i) % cb->n_elements] = MEM_AVAILABLE;
      cb->disk_idx += 1;
      cb->disk_idx %= cb->n_elements;
      cb->n_disk_elements++;
    }
    pthread_mutex_unlock(&cb->state_mtx);

    // synchronization code
    // lock this thread
    pthread_mutex_lock(&cb->write_disk_mtx);
    // lock until signal
    printf("waiting for cond disk_cond\n");
    pthread_cond_wait(&cb->write_disk_cond, &cb->write_disk_mtx);
    // unlock the thread
    pthread_mutex_unlock(&cb->write_disk_mtx);
  }
}

CircularBuffer createCircularBuffer(const size_t element_size,
                                    const size_t n_elements,
                                    const char *const buf_file_name) {
  void *mem = malloc(element_size * n_elements);
  char *state = malloc(n_elements);

  CircularBuffer cb = {mem, state, element_size, n_elements, 0, 0,
                       0,   0,     NO_WRITING};

  cb.buff_file = fopen(buf_file_name, "wb");

  pthread_attr_t attr;
  pthread_mutexattr_t mutex_attr;
  pthread_condattr_t cond_attr;

  pthread_mutexattr_init(&mutex_attr);
  pthread_condattr_init(&cond_attr);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

#ifdef _POSIX_THREAD_PROCESS_SHARED
  pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
#endif

  pthread_mutex_init(&cb.write_mem_mtx, &mutex_attr);
  pthread_mutex_init(&cb.write_disk_mtx, &mutex_attr);
  pthread_mutex_init(&cb.state_mtx, &mutex_attr);

  pthread_cond_init(&cb.write_mem_cond, &cond_attr);
  pthread_cond_init(&cb.write_disk_cond, &cond_attr);

  pthread_create(&cb.writer_tid, &attr, &writeToDiskCircularBuffer, &cb);

  fprintf(stderr, "CircularBuffer created\n");
  return cb;
}
double freeSpaceCircularBuffer(const CircularBuffer *const cb) {
  size_t elements_in_memory = cb->n_mem_elements - cb->n_disk_elements;
  return 1 - ((double)elements_in_memory) / cb->n_elements;
}

void deleteCircularBuffer(CircularBuffer *const cb) {

  printf("destroying CircularBuffer");
  if (freeSpaceCircularBuffer(cb) < cb->sizeof_elem * cb->n_elements) {
    printf("deleting CircularBuffer with data");
  }

  free(cb->mem);
  free(cb->state_array);
  // do not hold pointer un free memory
  cb->mem = NULL;
  cb->state_array = NULL;

  fclose(cb->buff_file);

  pthread_mutex_destroy(&cb->state_mtx);

  pthread_mutex_destroy(&cb->write_mem_mtx);
  pthread_mutex_destroy(&cb->write_disk_mtx);

  pthread_cond_destroy(&cb->write_mem_cond);
  pthread_cond_destroy(&cb->write_disk_cond);

  pthread_join(cb->writer_tid, NULL);
  printf("CircularBuffer distroied");
}

const size_t sizeof_elem = 1000 * 1000 * 3;
const size_t buff_n_elems = 200;

void addCircularBuffer(CircularBuffer *const cb, void *elem_mem) {

  // Esta seccion suspende el hilo capturar
  pthread_mutex_lock(&cb->write_mem_mtx);
  // si grabar no ha sido bleado seguir, caso contrario espera
  pthread_cond_wait(&cb->write_mem_cond, &cb->write_mem_mtx);
  // Esta seccion reinicia el hilo capturar
  pthread_mutex_unlock(&cb->write_mem_mtx);
  //
  // check if space if available
  const double free_buff_space = freeSpaceCircularBuffer(cb);
  if (free_buff_space < 0.2 && !cb->w_state) {
    // if less than 20% of memory available then write to disk before buffer is
    // empty
    pthread_cond_signal(&cb->write_disk_cond); // start writting to disk
  } else if (free_buff_space == 0.) {
    // if no space  available stop writing
    pthread_mutex_lock(&cb->write_mem_mtx);
    pthread_cond_wait(&cb->write_mem_cond, &cb->write_mem_mtx);
    pthread_mutex_unlock(&cb->write_mem_mtx);
  }

  // copy memory in to buff
  void *mem = &cb->mem[cb->mem_idx * cb->sizeof_elem];
  memcpy(mem, elem_mem, cb->sizeof_elem);

  pthread_mutex_lock(&cb->state_mtx);
  // set state as used
  cb->state_array[cb->mem_idx] = MEM_USED;
  // incremet mem_idx
  cb->mem_idx += 1;
  cb->mem_idx %= cb->n_elements;

  // add counter to disk elements
  cb->n_mem_elements++;
  pthread_mutex_unlock(&cb->state_mtx);
}

int main(int argc, char *argv[]) {
  // file to write buffer content
  printf("Initializing program...");
  const char *const buf_file_name = "./file.buf";
  // create the circular buffer
  printf("creating circular buffer");
  CircularBuffer frame_buff =
      createCircularBuffer(sizeof_elem, buff_n_elems, buf_file_name);


  printf("generating frames\n");
  for (int i = 0; i < 10000; i++) {

    printf("frame %d\n", i);

    usleep(1000000 / 1000); // 1000fps
  }

  deleteCircularBuffer(&frame_buff);
  return 0;
}
