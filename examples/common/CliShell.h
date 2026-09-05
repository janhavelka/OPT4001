#pragma once

#include <Arduino.h>

#include "CliLineBuffer.h"
#include "Log.h"

namespace cli_shell {

inline LineResult readLine(char* output, size_t outputCapacity) {
  static FixedLineBuffer line;
  LineResult result = LineResult::NONE;
  while (Serial.available() > 0 && result == LineResult::NONE) {
    result = line.push(static_cast<char>(Serial.read()), output,
                       outputCapacity);
  }
  return result;
}

}  // namespace cli_shell
