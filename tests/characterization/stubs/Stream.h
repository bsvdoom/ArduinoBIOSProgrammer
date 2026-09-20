#ifndef CHARACTERIZATION_STREAM_H
#define CHARACTERIZATION_STREAM_H

#include <cstddef>
#include <cstdint>

class Stream {
 public:
  virtual ~Stream() = default;

  virtual int available() = 0;
  virtual int read() = 0;
  virtual std::size_t write(std::uint8_t value) = 0;
};

#endif
