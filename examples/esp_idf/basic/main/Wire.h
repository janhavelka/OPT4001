// ESP-IDF new-driver implementation of the small TwoWire subset used by the
// shared OPT4001 bring-up CLI.
#pragma once

#include <cstddef>
#include <cstdint>

#include "Arduino.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

class TwoWire {
public:
  bool begin(int sda, int scl);
  void end();
  void setClock(uint32_t freqHz);
  void setTimeOut(uint16_t timeoutMs);

  void beginTransmission(uint8_t addr);
  size_t write(const uint8_t* data, size_t len);
  size_t write(uint8_t data);
  uint8_t endTransmission(bool stop = true);

  size_t requestFrom(uint8_t addr, size_t len);
  int available() const;
  int read();

private:
  static constexpr size_t BUFFER_SIZE = 128U;
  static constexpr uint8_t ADDRESS_COUNT = 128U;

  i2c_master_dev_handle_t ensureDevice(uint8_t addr);
  int transferTimeoutMs() const;
  uint8_t mapError(esp_err_t err) const;
  void clearPendingRead();

  i2c_master_bus_handle_t _bus = nullptr;
  i2c_master_dev_handle_t _devices[ADDRESS_COUNT] = {};
  bool _initialized = false;
  int _sda = -1;
  int _scl = -1;
  uint32_t _freqHz = 400000U;
  uint16_t _timeoutMs = 50U;

  uint8_t _txAddress = 0U;
  uint8_t _txBuffer[BUFFER_SIZE] = {};
  size_t _txLen = 0U;

  bool _hasPendingReadPrefix = false;
  uint8_t _pendingAddress = 0U;
  uint8_t _pendingTx[BUFFER_SIZE] = {};
  size_t _pendingTxLen = 0U;

  uint8_t _rxBuffer[BUFFER_SIZE] = {};
  size_t _rxLen = 0U;
  size_t _rxIndex = 0U;
};

extern TwoWire Wire;
