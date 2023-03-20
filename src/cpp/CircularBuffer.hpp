#pragma once


extern "C"{
#include "../c/CircularBuffer.h"
}


#include <vector>

using c_CircularBuffer = CircularBuffer;

namespace cb {

template <typename T> class CircularBuf {
private:
  c_CircularBuffer m_buff;

public:
  CircularBuf(const size_t n_elements, const size_t write_buff_length,
              const double save_threshold,
              const char *const buf_file_name) noexcept;
  ~CircularBuf();
  void pushBack(const T& elem) noexcept;
  void pushBlob(const void* const blob) noexcept;
  void emplaceBack(const T& elem) noexcept;
};

}; // namespace cb

template <typename T>
cb::CircularBuf<T>::CircularBuf(const size_t n_elements,
                                const size_t write_buff_length,
                                const double save_threshold,
                                const char *const buf_file_name) noexcept
    : m_buff(createCircularBuffer(sizeof(T), n_elements, write_buff_length,
                                  save_threshold, buf_file_name)) {

  initializeTheadsCircularBuffer(&m_buff);
}

template <typename T> cb::CircularBuf<T>::~CircularBuf() {
  deleteCircularBuffer(&m_buff);
}

template <typename T> void cb::CircularBuf<T>::pushBack(const T& elem) noexcept{
  addCircularBuffer(&m_buff, &elem);
}
template <typename T> void cb::CircularBuf<T>::pushBlob(const void*const blob) noexcept{
  addCircularBuffer(&m_buff, blob);
}
