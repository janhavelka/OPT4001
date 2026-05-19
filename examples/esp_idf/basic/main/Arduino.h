// ESP-IDF example compatibility layer for the shared OPT4001 bring-up CLI.
// This is intentionally local to examples/esp_idf/basic; the driver core does
// not depend on Arduino APIs.
#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <strings.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr int INPUT = 0;
static constexpr int OUTPUT = 1;
static constexpr int INPUT_PULLUP = 2;
static constexpr int LOW = 0;
static constexpr int HIGH = 1;

using byte = uint8_t;

class String {
public:
  String() = default;
  String(const char* value) : _value(value ? value : "") {}
  String(char value) : _value(1, value) {}
  String(const std::string& value) : _value(value) {}

  const char* c_str() const { return _value.c_str(); }
  size_t length() const { return _value.length(); }

  void trim() {
    const size_t first = _value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      _value.clear();
      return;
    }
    const size_t last = _value.find_last_not_of(" \t\r\n");
    _value = _value.substr(first, last - first + 1U);
  }

  int indexOf(char needle) const {
    const size_t pos = _value.find(needle);
    return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
  }

  String substring(size_t start) const {
    if (start >= _value.length()) {
      return String("");
    }
    return String(_value.substr(start));
  }

  String substring(size_t start, size_t end) const {
    if (start > end) {
      const size_t tmp = start;
      start = end;
      end = tmp;
    }
    if (start >= _value.length()) {
      return String("");
    }
    if (end > _value.length()) {
      end = _value.length();
    }
    return String(_value.substr(start, end - start));
  }

  bool startsWith(const char* prefix) const {
    if (prefix == nullptr) {
      return false;
    }
    const size_t prefixLen = std::strlen(prefix);
    return _value.compare(0, prefixLen, prefix) == 0;
  }

  String& operator=(const char* value) {
    _value = value ? value : "";
    return *this;
  }

  String& operator+=(char value) {
    _value.push_back(value);
    return *this;
  }

  bool operator==(const char* rhs) const {
    return _value == (rhs ? rhs : "");
  }

  bool operator!=(const char* rhs) const {
    return !(*this == rhs);
  }

  bool operator==(const String& rhs) const {
    return _value == rhs._value;
  }

  bool operator!=(const String& rhs) const {
    return !(*this == rhs);
  }

private:
  std::string _value;
};

inline bool operator==(const char* lhs, const String& rhs) {
  return rhs == lhs;
}

inline bool operator!=(const char* lhs, const String& rhs) {
  return !(rhs == lhs);
}

class HardwareSerial {
public:
  void begin(unsigned long baud);
  explicit operator bool() const { return true; }
  int available();
  int read();
  int printf(const char* fmt, ...);
  void print(const char* value);
  void print(char value);
  void print(const String& value);
  void println();
  void println(const char* value);
  void println(const String& value);
  void flush();

private:
  bool _started = false;
};

extern HardwareSerial Serial;

uint32_t millis();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void yield();
void pinMode(int pin, int mode);
int digitalRead(int pin);
void digitalWrite(int pin, int value);
