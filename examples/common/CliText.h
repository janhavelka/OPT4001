/**
 * @file CliText.h
 * @brief Small fixed-capacity text value used by the Arduino CLI parser.
 * @note Example-only helper; not part of the library API.
 */
#pragma once

#include <cstddef>
#include <cstring>

#include "CliLineBuffer.h"

namespace cli_shell {

class FixedText {
public:
  FixedText() = default;
  FixedText(const char* text) { assign(text); }

  FixedText& operator=(const char* text) {
    assign(text);
    return *this;
  }

  const char* c_str() const { return _data; }
  size_t length() const { return _length; }

  void trim() {
    size_t first = 0U;
    while (first < _length && (_data[first] == ' ' || _data[first] == '\t')) {
      ++first;
    }
    size_t last = _length;
    while (last > first && (_data[last - 1U] == ' ' || _data[last - 1U] == '\t')) {
      --last;
    }
    const size_t kept = last - first;
    if (first > 0U && kept > 0U) {
      std::memmove(_data, _data + first, kept);
    }
    _length = kept;
    _data[_length] = '\0';
  }

  bool startsWith(const char* prefix) const {
    if (prefix == nullptr) return false;
    const size_t prefixLength = std::strlen(prefix);
    return prefixLength <= _length && std::memcmp(_data, prefix, prefixLength) == 0;
  }

  bool endsWith(const char* suffix) const {
    if (suffix == nullptr) return false;
    const size_t suffixLength = std::strlen(suffix);
    return suffixLength <= _length &&
           std::memcmp(_data + (_length - suffixLength), suffix, suffixLength) == 0;
  }

  int indexOf(char value) const {
    const void* found = std::memchr(_data, value, _length);
    return found == nullptr
               ? -1
               : static_cast<int>(static_cast<const char*>(found) - _data);
  }

  FixedText substring(size_t first) const {
    return substring(first, _length);
  }

  FixedText substring(size_t first, size_t last) const {
    FixedText out;
    if (first > _length) first = _length;
    if (last > _length) last = _length;
    if (last < first) last = first;
    out.assign(_data + first, last - first);
    return out;
  }

  bool operator==(const char* rhs) const {
    return rhs != nullptr && std::strcmp(_data, rhs) == 0;
  }
  bool operator!=(const char* rhs) const { return !(*this == rhs); }

private:
  void assign(const char* text) {
    if (text == nullptr) {
      _length = 0U;
      _data[0] = '\0';
      return;
    }
    assign(text, std::strlen(text));
  }

  void assign(const char* text, size_t length) {
    const size_t maxLength = FixedLineBuffer::CAPACITY - 1U;
    _length = length < maxLength ? length : maxLength;
    if (_length > 0U) {
      std::memcpy(_data, text, _length);
    }
    _data[_length] = '\0';
  }

  char _data[FixedLineBuffer::CAPACITY]{};
  size_t _length = 0U;
};

}  // namespace cli_shell
