#pragma once

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

typedef int bool_t;
static const int True = 1;
static const int False = 0;

typedef struct {
  void *mem;
  const size_t sizeof_elem;
  const size_t n_elements;
  const size_t write_buff_length;
  const double save_threshold;
  size_t mem_idx;
  size_t disk_idx;
  size_t n_mem_elements;
  size_t n_disk_elements;

  bool_t writing_to_disk;
  bool_t writing_to_mem;

  bool_t exit;

  FILE *buff_file;

  pthread_t writer_tid;

  pthread_mutex_t write_mem_mtx;
  pthread_cond_t write_mem_cond;

  pthread_mutex_t write_disk_mtx;
  pthread_cond_t write_disk_cond;

  pthread_mutex_t state_mtx;

} CircularBuffer;

/**
 * Initializes the members of CircularBuffer, except the p-thread conds mutex
 * and threads
 *
 * @param const size_t element_size: Size of each element of the buffer
 * @param const size_t n_elements: Number of elements to fit in the buffer
 * @param const size_t write_buff_length: Number of bytes for each write call
 * @param const double save_threshold: if free space in buffer is below
 * save_threshold, write thread is signal
 * @param const char * const buf_file_name: Name file where the data will be
 * stored
 *
 * @see initializeThreads
 *
 * @return CircularBuffer instance
 *
 */
CircularBuffer createCircularBuffer(const size_t element_size /**[in]*/,
                                    const size_t n_elements /**[in]*/,
                                    const size_t write_buff_length /**[in]*/,
                                    const double save_threshold /**[in]*/,
                                    const char *const buf_file_name /**[in]*/);

/**
 * Initializes all the p-thread conds, mutexs and threads, after calling this
 * function the writer thread will be started and waiting for data
 *
 *
 * @param CircularBuffer *const cb: Pointer to CircularBuffer instance
 *
 */
void initializeTheadsCircularBuffer(CircularBuffer *const cb);

/**
 * DO NOT CALL, only for internal use
 * thread function that writes data to disk
 *
 * @param void *cb: Pointer to CircularBuffer instance
 *
 */
void *writeToDiskCircularBuffer(void *cb_ptr);

/**
 * Release all the resources being held by the CircularBuffer, also saves to
 * disk any raiming data in the buffer, if the CircularBuffer instance was
 * allocated in heap it is up to user to free it
 *
 *
 * @param CircularBuffer *const cb: Pointer to CircularBuffer instance to be
 * cleaned
 *
 */
void deleteCircularBuffer(CircularBuffer *const cb /**[in]*/);

/**
 * Inserts one element into the CircularBuffer in a thread-safe manner,
 * if not enough space if buffer this function will block until enough there is
 * memory avialable.
 *
 * If renaming free space in buffer is under the selected value it will signal
 * the writer thread to start moving elements out of the buffer and into the
 * disk.
 *
 * The element is copied in to the buffer, not build in place. The number of
 * bytes to being copied is set in @see createCircularBuffer
 *
 * @param CircularBuffer *const cb: Pointer to CircularBuffer instance to be
 * cleaned
 * @param const void *const elem_mem: Pointer to the memory.
 *
 * @see createCircularBuffer
 */
void addCircularBuffer(CircularBuffer *const cb /**[in]*/,
                       const void *const elem_mem /**[int]*/);

/**
 * Returns the percentage of free space in the buffer
 *
 * @param CircularBuffer *const cb: Pointer to CircularBuffer instance
 *
 *
 * @return double free space: between 0(full) and 1(empty)
 *
 */
double freeSpaceCircularBuffer(CircularBuffer *const cb /**[int]*/);

/**
 * DO NOT CALL, only for internal use
 * Signals the writer thread to start running
 *
 * @param CircularBuffer *const cb: Pointer to CircularBuffer instance
 *
 */
void enableWriteToDisk(CircularBuffer *const cb /**[int]*/);

/**
 * DO NOT CALL, only for internal use
 * Signals the mem thread (usually main thread) to start running
 *
 * @param CircularBuffer *const cb: Pointer to CircularBuffer instance
 *
 */
void enableWriteToMem(CircularBuffer *const cb /**[int]*/);

/**
 * DO NOT CALL, only for internal use
 * Locks the writer thread until signal signal is received
 *
 * @param CircularBuffer *const cb: Pointer to CircularBuffer instance
 *
 */
void lockWriteToDisk(CircularBuffer *const cb /**[int]*/);

/**
 * DO NOT CALL, only for internal use
 * Locks the mem_thread (usually main thread) until there is avialable space in
 * buffer
 *
 * @param CircularBuffer *const cb: Pointer to CircularBuffer instance
 *
 */
void lockWriteToMem(CircularBuffer *const cb /**[int]*/);

typedef struct {
  void *buffer_segment_1;
  size_t size_buff_1;

  void *buffer_segment_2;
  size_t size_buff_2;

} BuffSegmets;

/**
 * Computes 2 continuous buffer segments from the CircularBuffer containing all
 * the element that are yet to be saved
 *
 * @param CircularBuffer *const cb: Pointer to CircularBuffer instance
 * 
 * return BuffSegmets instance
 *
 */
BuffSegmets computeBufferSegments(CircularBuffer *const cb /**[int]*/);

/**
 * Saves a continuous buffer in to memmory in small chunks, size of chunk is
 * defined at construction
 *
 * @see createCircularBuffer
 *
 * @param CircularBuffer *const cb: Pointer to CircularBuffer instance
 * @param const void *const mem: Pointer to the memory to be saved.
 * @param const size_t mem_size: Number of bytes to be saved.
 *
 */
void writeBuffToDisk(CircularBuffer *const cb /**[int]*/,
                     const void *const mem /**[int]*/,
                     const size_t mem_size /**[int]*/);
