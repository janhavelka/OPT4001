/**
 * @file CliLineBuffer.h
 * @brief Framework-neutral fixed-storage command-line accumulator.
 * @note Example-only helper; not part of the library API.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cli_shell {

enum class LineResult : uint8_t {
  NONE,
  READY,
  TOO_LONG,
  OUTPUT_TOO_SMALL
};

/// Shared bounded parser for the Arduino and ESP-IDF diagnostic consoles.
class FixedLineBuffer {
public:
  static constexpr size_t CAPACITY = 192U;

  LineResult push(char value, char* output, size_t outputCapacity) {
    if (value != '\r' && value != '\n') {
      if (_discarding) {
        return LineResult::NONE;
      }
      if (value == '\b' || value == 0x7F) {
        if (_length > 0U) {
          --_length;
        }
        return LineResult::NONE;
      }
      if (_length >= CAPACITY - 1U) {
        _discarding = true;
        _length = 0U;
        return LineResult::NONE;
      }
      _buffer[_length++] = value;
      return LineResult::NONE;
    }

    if (_discarding) {
      reset();
      return LineResult::TOO_LONG;
    }
    if (_length == 0U) {
      return LineResult::NONE;
    }

    size_t first = 0U;
    while (first < _length &&
           (_buffer[first] == ' ' || _buffer[first] == '\t')) {
      ++first;
    }
    while (_length > first &&
           (_buffer[_length - 1U] == ' ' ||
            _buffer[_length - 1U] == '\t')) {
      --_length;
    }

    const size_t commandLength = _length - first;
    if (commandLength == 0U) {
      reset();
      return LineResult::NONE;
    }
    if (output == nullptr || outputCapacity == 0U ||
        commandLength >= outputCapacity) {
      reset();
      return LineResult::OUTPUT_TOO_SMALL;
    }

    std::memcpy(output, _buffer + first, commandLength);
    output[commandLength] = '\0';
    reset();
    return LineResult::READY;
  }

  void reset() {
    _length = 0U;
    _discarding = false;
  }

private:
  char _buffer[CAPACITY]{};
  size_t _length = 0U;
  bool _discarding = false;
};

}  // namespace cli_shell
