/// @file Status.h
/// @brief Error codes and status handling for OPT4001 driver.
#pragma once

#include <cstdint>

namespace OPT4001 {

/// Error codes for all OPT4001 operations.
enum class Err : uint8_t {
  OK = 0,                ///< Operation successful.
  NOT_INITIALIZED,       ///< begin() not called.
  INVALID_CONFIG,        ///< Invalid configuration parameter.
  I2C_ERROR,             ///< I2C communication failure.
  TIMEOUT,               ///< Operation timed out.
  INVALID_PARAM,         ///< Invalid parameter value.
  DEVICE_NOT_FOUND,      ///< OPT4001 did not respond on the bus.
  DEVICE_ID_MISMATCH,    ///< Device ID register did not match OPT4001.
  CRC_ERROR,             ///< Sample CRC verification failed.
  MEASUREMENT_NOT_READY, ///< No fresh measurement is ready yet.
  CONVERSION_NOT_READY = MEASUREMENT_NOT_READY, ///< Cross-library alias.
  BUSY,                  ///< Device is busy with an in-flight conversion.
  IN_PROGRESS,           ///< Operation scheduled; call tick() or poll readiness.
  I2C_NACK_ADDR,         ///< I2C address phase was not acknowledged.
  I2C_NACK_DATA,         ///< I2C data phase was not acknowledged.
  I2C_TIMEOUT,           ///< I2C transaction timed out.
  I2C_BUS                ///< I2C bus or arbitration error.
};

/// Status structure returned by all fallible operations.
struct Status {
  Err code = Err::OK;
  int32_t detail = 0;    ///< Implementation-specific detail code.
  const char* msg = "";  ///< Static string only.

  constexpr Status() = default;
  constexpr Status(Err codeIn, int32_t detailIn, const char* msgIn)
      : code(codeIn), detail(detailIn), msg(msgIn) {}

  /// @return true if operation succeeded.
  constexpr bool ok() const { return code == Err::OK; }

  /// @return true if status matches the supplied error code.
  constexpr bool is(Err err) const { return code == err; }

  /// @return true if the operation is still in progress.
  constexpr bool inProgress() const { return code == Err::IN_PROGRESS; }

  /// @return true if operation succeeded.
  explicit constexpr operator bool() const { return ok(); }

  /// Create a success status.
  static constexpr Status Ok() { return Status{Err::OK, 0, "OK"}; }

  /// Create an error status.
  static constexpr Status Error(Err err, const char* message, int32_t detailCode = 0) {
    return Status{err, detailCode, message};
  }
};

}  // namespace OPT4001
