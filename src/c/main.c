/** #include <bits/pthreadtypes.h> */
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h> // for clock_gettime()

#include <string.h>
#include <unistd.h>

#include "CircularBuffer.h"

const size_t sizeof_elem = 640 * 480 * 1; // size of each element
const size_t buff_n_elems = 200;          // number of elements in the buffer
const size_t write_buff_length =
    sizeof_elem * buff_n_elems / 10; // chunk size for writting in files
/** const int fps = 60;//pc computer 600fps(550) 150(maybe) MBs */
/** const int fps = 60;//laptop computer 200fps(more like 1666) 365(maybe) MBs
 */
const int fps = 600;
const double sleeptime = 1 / ((double)fps) * 1e6;
const double save_threshold = 0.2;

struct timeval start, end, main_start;

int main(int argc, char *argv[]) {
  // file to write buffer content
  printf("Initializing program...");
  const char *const buf_file_name = "./file.buf";
  // create the circular buffer
  printf("creating circular buffer");
  CircularBuffer frame_buff = createCircularBuffer(
      sizeof_elem, buff_n_elems, write_buff_length,save_threshold, buf_file_name);
  initializeTheadsCircularBuffer(&frame_buff);

  unsigned char buff[sizeof_elem];
  printf("generating frames\n");
  gettimeofday(&main_start, NULL);

  for (int i = 0; i < 50000; i++) {

    printf("%6d ", i);
    gettimeofday(&start, NULL);
    addCircularBuffer(&frame_buff, buff);
    gettimeofday(&end, NULL);

    double elapsed_time =
        (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
    double elapsed_main_time = (end.tv_sec - main_start.tv_sec) +
                               (end.tv_usec - main_start.tv_usec) / 1e6;

    printf("%lf %lf\n", elapsed_time, elapsed_main_time);
    usleep(sleeptime);
  }

  deleteCircularBuffer(&frame_buff);
  return 0;
}
