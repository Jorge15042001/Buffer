//////////////////////////////////////////////////////////////////
/*
Tcam Software Trigger
This sample shows, how to trigger the camera by software and use a callback for
image handling.

Prerequisits
It uses the the examples/cpp/common/tcamcamera.cpp and .h files of the
*tiscamera* repository as wrapper around the GStreamer code and property
handling. Adapt the CMakeList.txt accordingly.
*/
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CircularBuffer.hpp"
#include "gst/app/gstappsink.h"
#include "gst/gstinfo.h"
#include "gst/gstmemory.h"
#include "gst/gstsample.h"
#include "tcamcamera.h"
#include <string>
#include <unistd.h>
#include <sys/time.h> // for clock_gettime()

#include "opencv2/opencv.hpp"


const size_t sizeof_elem = 320 * 240 * 1; // size of each element
const size_t buff_n_elems = 200;          // number of elements in the buffer
const size_t write_buff_length =
    sizeof_elem * buff_n_elems / 10; // chunk size for writting in files
const double save_threshold = 0.2;
const char *const buf_file_name = "./file.buf";

struct timeval start, end, main_start;
using namespace gsttcam;

using frame_t = std::uint8_t[sizeof_elem];

////////////////////////////////////////////////////////////////////
// List available properties helper function.
void ListProperties(TcamCamera &cam) {
  // Get a list of all supported properties and print it out
  auto properties = cam.get_camera_property_list();
  std::cout << "Properties:" << std::endl;
  for (auto &prop : properties) {
    std::cout << prop << std::endl;
  }
}

class GstFrameInfo {
  GstFrameInfo(const GstFrameInfo &) = delete;
  GstFrameInfo(GstFrameInfo &&) = delete;
  GstFrameInfo &operator=(GstFrameInfo &) = delete;
  GstFrameInfo &operator=(GstFrameInfo &&) = delete;

public:
  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;
  int width;
  int height;
  std::size_t color_depth;
  std::string capture_format;

  GstFrameInfo(GstAppSink *appsink)
      : sample(gst_app_sink_pull_sample(appsink)),
        buffer(gst_sample_get_buffer(sample)) {
    gst_buffer_map(buffer, &info, GST_MAP_READ);
    const GstStructure *str;
    GstCaps *caps = gst_sample_get_caps(sample);
    str = gst_caps_get_structure(caps, 0);
    capture_format = gst_structure_get_string(str, "format");
    color_depth = [this] {
      if (capture_format == "BGRx")
        return 4;
      if (capture_format == "GRAY8")
        return 1;
      throw "unknown capture format";
    }();
    gst_structure_get_int(str, "width", &width);
    gst_structure_get_int(str, "height", &height);
  }
  ~GstFrameInfo() {
    gst_buffer_unmap(buffer, &info);
    gst_sample_unref(sample);
  }
};

////////////////////////////////////////////////////////////////////
// Callback called for new images by the internal appsink

GstFlowReturn new_frame_cb(GstAppSink *appsink, gpointer data) {

  static int frame_count =0;
    gettimeofday(&end, NULL);

    const double elapsed_time =
        (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    std::cout<<"fps: "<<1/elapsed_time<<'\n';
    start = end;
  
  cb::CircularBuf<frame_t>* const cb =(cb::CircularBuf<frame_t>*) data;

  GstFrameInfo frame_info(appsink);
  std::cout<<"frame "<<frame_count << '\n';
  frame_count++;

  if (frame_info.info.data == NULL) {
    return GST_FLOW_OK;
  }

  // pCustomData->frame_writer.writeFrame(frame_info.info.data, frame_info.width * frame_info.height * frame_info.color_depth);
  assert(sizeof(frame_t)==frame_info.width*frame_info.height);
  cb->pushBlob(frame_info.info.data);
  std::cout<<"pushBlob done\n";
  

  return GST_FLOW_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) {
  gst_init(&argc, &argv);
  gst_debug_set_default_threshold(GST_LEVEL_WARNING);


  cb::CircularBuf<frame_t> a(buff_n_elems,write_buff_length,save_threshold,buf_file_name);


  // Open camera by serial number
  TcamCamera cam("15710238-aravis");
  // TcamCamera cam("15710238-v4l2");

  // Set video format, resolution and frame rate
  cam.set_capture_format("GRAY8", FrameSize{320, 240}, FrameRate{1100, 1});
  // cam.set_capture_format("GRAY8", FrameSize{640, 480}, FrameRate{550, 1});
    // GstElement *xi = gst_element_factory_make("ximagesink", NULL);
    // cam.enable_video_display(xi);

  // Register a callback to be called for each new frame
  cam.set_new_frame_callback(new_frame_cb, &a);

  // cam.set_property("Denoise",1 );

  cam.set_property("ExposureAuto", "Off");
  cam.set_property("ExposureTime", 100.0);

  // Start the camera

  const bool cam_succ_start = cam.start();

  if (!cam_succ_start) {
    return 1;
    ;
  }
  gettimeofday(&start, NULL);

  printf("Press Enter to end the program");
  std::string buf;
  std::cin >> buf;

  cam.stop();

  return 0;
}
