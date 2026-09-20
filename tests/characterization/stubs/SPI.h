#ifndef CHARACTERIZATION_SPI_H
#define CHARACTERIZATION_SPI_H

#include "Arduino.h"
#include <cstdint>

constexpr std::uint8_t SS = 10;
constexpr std::uint8_t MSBFIRST = 1;
constexpr std::uint8_t SPI_CLOCK_DIV2 = 2;
constexpr std::uint8_t SPI_MODE0 = 0;

class SPIClass {
 public:
  void begin();
  void end();
  void setBitOrder(std::uint8_t order);
  void setClockDivider(std::uint8_t divider);
  void setDataMode(std::uint8_t mode);
  std::uint8_t transfer(std::uint8_t value);
};
extern SPIClass SPI;

#endif
