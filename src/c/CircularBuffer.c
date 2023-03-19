#include "CircularBuffer.h"
#include <stdlib.h>
#include <string.h>

CircularBuffer createCircularBuffer(const size_t element_size,
                                    const size_t n_elements,
                                    const size_t write_buff_length,
                                    const double save_threshold,
                                    const char *const buf_file_name) {
  void *mem = malloc(element_size * n_elements);

  CircularBuffer cb = {mem,
                       element_size,
                       n_elements,
                       write_buff_length,
                       save_threshold,
                       0,
                       0,
                       0,
                       0,
                       FALSE,
                       FALSE,
                       FALSE,
                       fopen(buf_file_name, "wb")};

  fprintf(stderr, "CircularBuffer created\n");
  return cb;
}

void initializeTheadsCircularBuffer(CircularBuffer *const cb) {
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

  struct sched_param sched;
  const int sched_type = SCHED_RR;
  sched.sched_priority = sched_get_priority_max(sched_type);
  pthread_setschedparam(cb->writer_tid, sched_type, &sched);
}

void *writeToDiskCircularBuffer(void *cb_ptr) {
  nice(-20);
  CircularBuffer *const cb = (CircularBuffer *)cb_ptr;

  while (1) {
    // synchronization code
    lockWriteToDisk(cb);

    printf("writing to disk...\n");
    BuffSegmets bs = computeBufferSegments(cb);
    // set the state as writting
    /** pthread_mutex_lock(&cb->state_mtx); */
    printf("setting w_state to WRIING\n");
    cb->writing_to_disk = TRUE;
    /** pthread_mutex_unlock(&cb->state_mtx); */

    printf("writing to disk\n");
    /** printf("buffers: %ld %ld \n",bs.size_buff_1,bs.size_buff_2); */

    writeBuffToDisk(cb, bs.buffer_segment_1, bs.size_buff_1);
    writeBuffToDisk(cb, bs.buffer_segment_2, bs.size_buff_2);

    if (!cb->writing_to_mem) {
      enableWriteToMem(cb);
    }

    if (cb->exit)
      break;
    // locks until all variables have been set to their correct values
    /** pthread_mutex_lock(&cb->state_mtx); */
    cb->writing_to_disk = FALSE;
    /** pthread_mutex_unlock(&cb->state_mtx); */
  }
}

void deleteCircularBuffer(CircularBuffer *const cb) {

  printf("destroying CircularBuffer");
  cb->exit = TRUE;
  pthread_mutex_lock(&cb->write_disk_mtx);
  pthread_cond_signal(&cb->write_disk_cond); // start writting to disk
  pthread_mutex_unlock(&cb->write_disk_mtx);
  printf("joining thread");
  pthread_join(cb->writer_tid, NULL);
  printf("joined write thread");

  free(cb->mem);
  // do not hold pointer un free memory
  cb->mem = NULL;

  fclose(cb->buff_file);

  pthread_mutex_destroy(&cb->state_mtx);

  pthread_mutex_destroy(&cb->write_mem_mtx);
  pthread_mutex_destroy(&cb->write_disk_mtx);

  pthread_cond_destroy(&cb->write_mem_cond);
  pthread_cond_destroy(&cb->write_disk_cond);

  printf("CircularBuffer distroied");
}

void addCircularBuffer(CircularBuffer *const cb, const void *const elem_mem) {
  // check if space if available
  const double free_buff_space = freeSpaceCircularBuffer(cb);
  /** printf("free space %f \n",free_buff_space); */
  printf(" %lf ", free_buff_space);
  if (free_buff_space < cb->save_threshold && !cb->writing_to_disk) {
    // if less than 20% of memory available then write to disk before buffer is
    // empty

    struct sched_param sched;
    const int sched_type = SCHED_RR;
    sched.sched_priority = sched_get_priority_max(sched_type);
    pthread_setschedparam(cb->writer_tid, sched_type, &sched);
    nice(-20);
    /** pthread_mutex_lock(&cb->state_mtx); */
    /** cb->w_disk_state = WRITING; */
    /** pthread_mutex_unlock(&cb->state_mtx); */
    enableWriteToDisk(cb);
  }
  if ((cb->mem_idx + 1) % cb->n_elements ==
      cb->disk_idx) { // todo calculate threshold
    // if no space  available stop writing
    /** pthread_mutex_lock(&cb->state_mtx); */
    cb->writing_to_mem = FALSE;
    /** pthread_mutex_unlock(&cb->state_mtx); */
    struct sched_param sched;
    const int sched_type = SCHED_RR;
    sched.sched_priority = sched_get_priority_max(sched_type);
    pthread_setschedparam(cb->writer_tid, sched_type, &sched);
    nice(-20);

    /** printf("\t\tlock writing no space available\n"); */
    enableWriteToDisk(cb);
    lockWriteToMem(cb);
  }

  // copy memory in to buff
  void *mem = &cb->mem[cb->mem_idx * cb->sizeof_elem];
  memcpy(mem, elem_mem, cb->sizeof_elem);

  pthread_mutex_lock(&cb->state_mtx);
  cb->writing_to_mem = TRUE;
  // set state as used
  /** cb->state_array[cb->mem_idx] = MEM_USED; */
  // incremet mem_idx
  cb->mem_idx += 1;
  cb->mem_idx %= cb->n_elements;

  // add counter to mem elements
  cb->n_mem_elements++;
  /** printf("finished writting to mem, n_mem_elements:
   * %ld\n",cb->n_mem_elements); */
  pthread_mutex_unlock(&cb->state_mtx);
}

double freeSpaceCircularBuffer(CircularBuffer *const cb) {
  pthread_mutex_lock(&cb->state_mtx);
  size_t elements_in_memory = cb->n_mem_elements - cb->n_disk_elements;
  pthread_mutex_unlock(&cb->state_mtx);
  return 1 - ((double)elements_in_memory) / cb->n_elements;
}

void enableWriteToDisk(CircularBuffer *const cb) {
  pthread_mutex_lock(&cb->write_disk_mtx);
  pthread_cond_signal(&cb->write_disk_cond); // start writting to disk
  pthread_mutex_unlock(&cb->write_disk_mtx);
}

void enableWriteToMem(CircularBuffer *const cb) {
  pthread_mutex_lock(&cb->write_mem_mtx);
  pthread_cond_signal(&cb->write_mem_cond);
  pthread_mutex_unlock(&cb->write_mem_mtx);
}
void lockWriteToDisk(CircularBuffer *const cb) {
  pthread_mutex_lock(&cb->write_disk_mtx);
  pthread_cond_wait(&cb->write_disk_cond, &cb->write_disk_mtx);
  pthread_mutex_unlock(&cb->write_disk_mtx);
}

void lockWriteToMem(CircularBuffer *const cb) {
  pthread_mutex_lock(&cb->write_mem_mtx);
  pthread_cond_wait(&cb->write_mem_cond, &cb->write_mem_mtx);
  pthread_mutex_unlock(&cb->write_mem_mtx);
}

BuffSegmets computeBufferSegments(CircularBuffer *const cb) {
  BuffSegmets segments;
  pthread_mutex_lock(&cb->state_mtx);
  /** printf("computing buffs %ld %ld\n",cb->mem_idx,cb->disk_idx); */

  if (cb->disk_idx > cb->mem_idx) {
    // segmemt 1 is all the elements from disk_idx to the last element in buff
    segments.buffer_segment_1 = &(cb->mem[cb->disk_idx * cb->sizeof_elem]);
    segments.size_buff_1 = (cb->n_elements - cb->disk_idx) * cb->sizeof_elem;
    // segmemt 2 is all the elements from 0 to mem_idx
    segments.buffer_segment_2 = cb->mem;
    segments.size_buff_2 = (cb->mem_idx - 1) * cb->sizeof_elem;
    if (cb->mem_idx == 0) {
      segments.buffer_segment_2 = NULL;
      segments.size_buff_2 = 0;
    }
    /** printf("mem_idx: %ld\n",cb->mem_idx); */
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

void writeBuffToDisk(CircularBuffer *const cb, const void *const mem,
                     const size_t mem_size) {
  if (mem_size == 0)
    return;
  // lop mem with write_buff_length steps
  for (size_t i = 0; i < mem_size; i += cb->write_buff_length) {
    const void *const mem_ptr = &mem[i];
    size_t write_size = cb->write_buff_length;
    if (i + cb->write_buff_length >= mem_size) {
      write_size = mem_size - i;
    }
    /** printf("mem: %ld %ld\n",write_size,writen); */
    fwrite(mem_ptr, write_size, 1, cb->buff_file);
    pthread_mutex_lock(&cb->state_mtx);
    for (size_t i = 0; i < write_size / cb->sizeof_elem; i++) {
      /** cb->state_array[(cb->disk_idx + i) % cb->n_elements] = MEM_AVAILABLE;
       */
      cb->disk_idx += 1;
      cb->disk_idx %= cb->n_elements;
      cb->n_disk_elements++;
    }
    /** printf("mem: %ld %ld\n",mem_size,writen); */
    pthread_mutex_unlock(&cb->state_mtx);
    enableWriteToMem(cb);
  }
}
