#ifndef CHARACTERIZATION_ARDUINO_H
#define CHARACTERIZATION_ARDUINO_H

#include "Stream.h"
#include <cstdarg>
#include <cstdint>
#include <cstdio>

class __FlashStringHelper;
#define F(text) reinterpret_cast<const __FlashStringHelper *>(text)
constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int OUTPUT = 1;
constexpr int INPUT_PULLUP = 2;
constexpr std::uint8_t MISO = 12;

#define PROGMEM
#define pgm_read_byte(address) (*reinterpret_cast<const std::uint8_t *>(address))
#define pgm_read_word(address) (*reinterpret_cast<const std::uint16_t *>(address))
#define pgm_read_dword(address) (*reinterpret_cast<const std::uint32_t *>(address))

void pinMode(unsigned char pin, int mode);
void digitalWrite(unsigned char pin, int value);
void delay(unsigned long milliseconds);
void delayMicroseconds(unsigned int microseconds);
unsigned long millis();
int vfprintf_P(FILE *file, const char *format, va_list args);
extern Stream &Serial;

#endif
