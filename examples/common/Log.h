/**
 * @file Log.h
 * @brief Simple serial logging macros for examples.
 *
 * NOT part of the library API. The library itself does not log.
 * These macros are for example/application code only.
 */

#pragma once

#include <Arduino.h>

#include "examples/common/BuildConfig.h"

// Compile-time validation
#if LOG_LEVEL < 0 || LOG_LEVEL > 4
#error "LOG_LEVEL must be 0-4 (0=off, 1=error, 2=info, 3=debug, 4=trace)"
#endif

inline bool& log_color_enabled_storage() {
  static bool enabled = true;
  return enabled;
}

inline bool log_color_enabled() { return log_color_enabled_storage(); }
inline void log_set_color_enabled(bool enabled) {
  log_color_enabled_storage() = enabled;
}

#define LOG_COLOR_RESET  (log_color_enabled() ? "\033[0m" : "")
#define LOG_COLOR_RED    (log_color_enabled() ? "\033[31m" : "")
#define LOG_COLOR_GREEN  (log_color_enabled() ? "\033[32m" : "")
#define LOG_COLOR_YELLOW (log_color_enabled() ? "\033[33m" : "")
#define LOG_COLOR_BLUE   (log_color_enabled() ? "\033[34m" : "")
#define LOG_COLOR_CYAN   (log_color_enabled() ? "\033[36m" : "")
#define LOG_COLOR_GRAY   (log_color_enabled() ? "\033[90m" : "")
#define LOG_COLOR_RESULT(ok) ((ok) ? LOG_COLOR_GREEN : LOG_COLOR_RED)
#define LOG_COLOR_STATE(online, failures) \
  ((online) ? (((failures) > 0U) ? LOG_COLOR_YELLOW : LOG_COLOR_GREEN) : LOG_COLOR_RED)

inline const char* log_bool_str(bool value) { return value ? "yes" : "no"; }

// Colorize only the severity tag; keep message text in terminal default color.
#define LOG_PRINT_WITH_TAG(tagColor, tag, fmt, ...) \
  Serial.printf("%s[" tag "]%s " fmt "\n", tagColor, LOG_COLOR_RESET, ##__VA_ARGS__)

/// @brief Log error message (level >= 1)
#define LOGE(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 1) LOG_PRINT_WITH_TAG(LOG_COLOR_RED, "E", fmt, ##__VA_ARGS__); \
  } while (0)

/// @brief Log warning message (level >= 2)
#define LOGW(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 2) LOG_PRINT_WITH_TAG(LOG_COLOR_YELLOW, "W", fmt, ##__VA_ARGS__); \
  } while (0)

/// @brief Log info message (level >= 2)
#define LOGI(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 2) LOG_PRINT_WITH_TAG(LOG_COLOR_CYAN, "I", fmt, ##__VA_ARGS__); \
  } while (0)

/// @brief Log debug message (level >= 3)
#define LOGD(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 3) LOG_PRINT_WITH_TAG(LOG_COLOR_BLUE, "D", fmt, ##__VA_ARGS__); \
  } while (0)

/// @brief Log trace message (level >= 4)
#define LOGT(fmt, ...) \
  do { \
    if (LOG_LEVEL >= 4) LOG_PRINT_WITH_TAG(LOG_COLOR_GRAY, "T", fmt, ##__VA_ARGS__); \
  } while (0)

// Conditional verbose logging (runtime switch)
#define LOGV(verbose, fmt, ...) \
  do { \
    if (verbose) { \
      LOG_PRINT_WITH_TAG(LOG_COLOR_GRAY, "V", fmt, ##__VA_ARGS__); \
    } \
  } while (0)
