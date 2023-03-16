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

  WritingState w_disk_state;
  WritingState w_mem_state;

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
  printf("Computing buff segments\n");
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
  printf("Computed buff segments\n");
  return segments;
}

void *writeToDiskCircularBuffer(void *cb_ptr) {
  printf("starting write thread\n");
  CircularBuffer *const cb = (CircularBuffer *)cb_ptr;

  while (1) {
    printf("starting write thread loop\n");
    // synchronization code
    // lock this thread
    pthread_mutex_lock(&cb->write_disk_mtx);
    // lock until signal
    printf("waiting for cond disk_cond\n");
    pthread_cond_wait(&cb->write_disk_cond, &cb->write_disk_mtx);
    // unlock the thread
    printf("writing to disk...\n");
    pthread_mutex_unlock(&cb->write_disk_mtx);

    BuffSegmets bs = computeBufferSegments(cb);
    // set the state as writting
    pthread_mutex_lock(&cb->state_mtx);
    printf("setting w_state to WRIING\n");
    cb->w_disk_state = WRITING;
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
    cb->w_disk_state = NO_WRITING;

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
    if (cb->w_mem_state ==NO_WRITING){
      pthread_mutex_lock(&cb->write_mem_mtx);
      pthread_cond_signal(&cb->write_mem_cond);
      pthread_mutex_unlock(&cb->write_mem_mtx);
    }

    //TODO; 
    // finish
  }
}

void initializeTheads(CircularBuffer *const cb){
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

  pthread_mutex_init(&cb->write_mem_mtx, &mutex_attr);
  pthread_mutex_init(&cb->write_disk_mtx, &mutex_attr);
  pthread_mutex_init(&cb->state_mtx, &mutex_attr);

  pthread_cond_init(&cb->write_mem_cond, &cond_attr);
  pthread_cond_init(&cb->write_disk_cond, &cond_attr);

  pthread_create(&cb->writer_tid, &attr, &writeToDiskCircularBuffer, cb);


}
CircularBuffer createCircularBuffer(const size_t element_size,
                                    const size_t n_elements,
                                    const char *const buf_file_name) {
  void *mem = malloc(element_size * n_elements);
  char *state = malloc(n_elements);

  CircularBuffer cb = {mem, state, element_size, n_elements, 0, 0,
                       0,   0,     NO_WRITING, NO_WRITING};

  cb.buff_file = fopen(buf_file_name, "wb");

  fprintf(stderr, "CircularBuffer created\n");
  return cb;
}
double freeSpaceCircularBuffer(CircularBuffer *const cb) {
  pthread_mutex_lock(&cb->state_mtx);
  /** printf("\tMem elems: %ld\n\tDisk elems: %ld\n",cb->n_mem_elements,cb->n_disk_elements); */
  size_t elements_in_memory = cb->n_mem_elements - cb->n_disk_elements;
  pthread_mutex_unlock(&cb->state_mtx);
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

  printf("joining thread");
  pthread_join(cb->writer_tid, NULL);
  printf("CircularBuffer distroied");
}

const size_t sizeof_elem = 1000 * 1000 * 3;
const size_t buff_n_elems = 200;

void addCircularBuffer(CircularBuffer *const cb, void *elem_mem) {
  /** printf("called addCircularBuffer/n"); */

  /** // Esta seccion suspende el hilo capturar */
  /** printf("locking write_mem_mtx\n"); */
  /** pthread_mutex_lock(&cb->write_mem_mtx); */
  /** // si grabar no ha sido bleado seguir, caso contrario espera */
  /** printf("waiting on mem_cond\n"); */
  /** pthread_cond_wait(&cb->write_mem_cond, &cb->write_mem_mtx); */
  /** // Esta seccion reinicia el hilo capturar */
  /** printf("locking write_mem_mtx\n"); */
  /** pthread_mutex_unlock(&cb->write_mem_mtx); */
  //
  // check if space if available
  const double free_buff_space = freeSpaceCircularBuffer(cb);
  /** printf("free space %f \n",free_buff_space); */
  if (free_buff_space < 0.50 && !cb->w_disk_state) {
    // if less than 20% of memory available then write to disk before buffer is
    // empty
    printf("just sending disk_cond \n");

    pthread_mutex_lock(&cb->state_mtx);
    cb->w_disk_state = WRITING;
    pthread_mutex_unlock(&cb->state_mtx);

    pthread_mutex_lock(&cb->write_disk_mtx);
    pthread_cond_signal(&cb->write_disk_cond); // start writting to disk
    pthread_mutex_unlock(&cb->write_disk_mtx);
                                              
  } 
  if (free_buff_space ==0.0) {//todo calculate threshold
    // if no space  available stop writing
    pthread_mutex_lock(&cb->state_mtx);
    cb->w_mem_state = NO_WRITING;
    pthread_mutex_unlock(&cb->state_mtx);

    printf("lock writing no space available\n");
    pthread_mutex_lock(&cb->write_mem_mtx);
    pthread_cond_wait(&cb->write_mem_cond, &cb->write_mem_mtx);
    pthread_mutex_unlock(&cb->write_mem_mtx);
  }

  // copy memory in to buff
  void *mem = &cb->mem[cb->mem_idx * cb->sizeof_elem];
  memcpy(mem, elem_mem, cb->sizeof_elem);

  pthread_mutex_lock(&cb->state_mtx);
  cb->w_mem_state = WRITING;
  // set state as used
  cb->state_array[cb->mem_idx] = MEM_USED;
  // incremet mem_idx
  cb->mem_idx += 1;
  cb->mem_idx %= cb->n_elements;

  // add counter to mem elements
  cb->n_mem_elements++;
  /** printf("finished writting to mem, n_mem_elements: %ld\n",cb->n_mem_elements); */
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
  initializeTheads(&frame_buff);

  sleep(2);
  unsigned char buff[sizeof_elem];
  printf("generating frames\n");
  for (int i = 0; i < 10000; i++) {

    printf("%6d ", i);
    addCircularBuffer(&frame_buff, buff);

    usleep(1000000 / 500); // 500fps
  }

  deleteCircularBuffer(&frame_buff);
  return 0;
}
