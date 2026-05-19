#include "Arduino.h"
#include "Wire.h"

#include <algorithm>
#include <limits>

#include "esp_err.h"

HardwareSerial Serial;
TwoWire Wire;

namespace {

int clampTimeoutMs(uint32_t timeoutMs) {
  const uint32_t maxTimeout = static_cast<uint32_t>(std::numeric_limits<int>::max());
  return static_cast<int>(timeoutMs > maxTimeout ? maxTimeout : timeoutMs);
}

gpio_num_t toGpio(int pin) {
  return static_cast<gpio_num_t>(pin);
}

}  // namespace

void HardwareSerial::begin(unsigned long baud) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stdin, nullptr, _IONBF, 0);

  if (!_started) {
    const esp_err_t err = uart_driver_install(UART_NUM_0, 4096, 0, 0, nullptr, 0);
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
      _started = true;
    }
  }
  (void)uart_set_baudrate(UART_NUM_0, static_cast<uint32_t>(baud));
}

int HardwareSerial::available() {
  size_t buffered = 0;
  if (uart_get_buffered_data_len(UART_NUM_0, &buffered) != ESP_OK) {
    return 0;
  }
  return static_cast<int>(buffered);
}

int HardwareSerial::read() {
  uint8_t ch = 0;
  const int readLen = uart_read_bytes(UART_NUM_0, &ch, 1, 0);
  return (readLen == 1) ? static_cast<int>(ch) : -1;
}

int HardwareSerial::printf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const int rc = std::vprintf(fmt, args);
  va_end(args);
  std::fflush(stdout);
  return rc;
}

void HardwareSerial::print(const char* value) {
  std::fputs(value ? value : "", stdout);
  std::fflush(stdout);
}

void HardwareSerial::print(char value) {
  std::fputc(value, stdout);
  std::fflush(stdout);
}

void HardwareSerial::print(const String& value) {
  print(value.c_str());
}

void HardwareSerial::println() {
  std::fputc('\n', stdout);
  std::fflush(stdout);
}

void HardwareSerial::println(const char* value) {
  print(value);
  println();
}

void HardwareSerial::println(const String& value) {
  println(value.c_str());
}

void HardwareSerial::flush() {
  std::fflush(stdout);
}

uint32_t millis() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void delay(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

void delayMicroseconds(uint32_t us) {
  esp_rom_delay_us(us);
}

void yield() {
  taskYIELD();
}

void pinMode(int pin, int mode) {
  if (pin < 0) {
    return;
  }

  const gpio_num_t gpio = toGpio(pin);
  if (mode == OUTPUT) {
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(gpio, GPIO_FLOATING);
    return;
  }

  gpio_set_direction(gpio, GPIO_MODE_INPUT);
  gpio_set_pull_mode(gpio, mode == INPUT_PULLUP ? GPIO_PULLUP_ONLY : GPIO_FLOATING);
}

int digitalRead(int pin) {
  if (pin < 0) {
    return LOW;
  }
  return gpio_get_level(toGpio(pin)) ? HIGH : LOW;
}

void digitalWrite(int pin, int value) {
  if (pin < 0) {
    return;
  }
  gpio_set_level(toGpio(pin), value ? 1 : 0);
}

bool TwoWire::begin(int sda, int scl) {
  end();

  _sda = sda;
  _scl = scl;

  i2c_master_bus_config_t busConfig{};
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.i2c_port = I2C_NUM_0;
  busConfig.sda_io_num = static_cast<gpio_num_t>(_sda);
  busConfig.scl_io_num = static_cast<gpio_num_t>(_scl);
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;

  const esp_err_t err = i2c_new_master_bus(&busConfig, &_bus);
  _initialized = (err == ESP_OK);
  clearPendingRead();
  _txLen = 0U;
  _rxLen = 0U;
  _rxIndex = 0U;
  return _initialized;
}

void TwoWire::end() {
  for (i2c_master_dev_handle_t& handle : _devices) {
    if (handle != nullptr) {
      (void)i2c_master_bus_rm_device(handle);
      handle = nullptr;
    }
  }

  if (_bus != nullptr) {
    (void)i2c_del_master_bus(_bus);
    _bus = nullptr;
  }

  _initialized = false;
  clearPendingRead();
  _txLen = 0U;
  _rxLen = 0U;
  _rxIndex = 0U;
}

void TwoWire::setClock(uint32_t freqHz) {
  if (freqHz > 0U) {
    _freqHz = freqHz;
  }
}

void TwoWire::setTimeOut(uint16_t timeoutMs) {
  if (timeoutMs > 0U) {
    _timeoutMs = timeoutMs;
  }
}

void TwoWire::beginTransmission(uint8_t addr) {
  _txAddress = addr;
  _txLen = 0U;
}

size_t TwoWire::write(const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0U || _txLen >= BUFFER_SIZE) {
    return 0U;
  }

  const size_t availableBytes = BUFFER_SIZE - _txLen;
  const size_t toCopy = std::min(len, availableBytes);
  std::memcpy(&_txBuffer[_txLen], data, toCopy);
  _txLen += toCopy;
  return toCopy;
}

size_t TwoWire::write(uint8_t data) {
  return write(&data, 1U);
}

uint8_t TwoWire::endTransmission(bool stop) {
  if (!_initialized || _bus == nullptr) {
    return 4U;
  }

  if (!stop) {
    _pendingAddress = _txAddress;
    _pendingTxLen = _txLen;
    if (_txLen > 0U) {
      std::memcpy(_pendingTx, _txBuffer, _txLen);
    }
    _hasPendingReadPrefix = true;
    _txLen = 0U;
    return 0U;
  }

  esp_err_t err = ESP_OK;
  if (_txLen == 0U) {
    err = i2c_master_probe(_bus, _txAddress, transferTimeoutMs());
  } else {
    i2c_master_dev_handle_t dev = ensureDevice(_txAddress);
    if (dev == nullptr) {
      _txLen = 0U;
      return 4U;
    }
    err = i2c_master_transmit(dev, _txBuffer, _txLen, transferTimeoutMs());
  }

  _txLen = 0U;
  return mapError(err);
}

size_t TwoWire::requestFrom(uint8_t addr, size_t len) {
  _rxLen = 0U;
  _rxIndex = 0U;

  if (!_initialized || len == 0U || len > BUFFER_SIZE) {
    clearPendingRead();
    return 0U;
  }

  i2c_master_dev_handle_t dev = ensureDevice(addr);
  if (dev == nullptr) {
    clearPendingRead();
    return 0U;
  }

  esp_err_t err = ESP_OK;
  if (_hasPendingReadPrefix && _pendingAddress == addr && _pendingTxLen > 0U) {
    err = i2c_master_transmit_receive(
        dev, _pendingTx, _pendingTxLen, _rxBuffer, len, transferTimeoutMs());
  } else {
    err = i2c_master_receive(dev, _rxBuffer, len, transferTimeoutMs());
  }

  clearPendingRead();

  if (err != ESP_OK) {
    return 0U;
  }

  _rxLen = len;
  return _rxLen;
}

int TwoWire::available() const {
  return static_cast<int>(_rxLen - _rxIndex);
}

int TwoWire::read() {
  if (_rxIndex >= _rxLen) {
    return -1;
  }
  return static_cast<int>(_rxBuffer[_rxIndex++]);
}

i2c_master_dev_handle_t TwoWire::ensureDevice(uint8_t addr) {
  if (!_initialized || _bus == nullptr || addr >= ADDRESS_COUNT) {
    return nullptr;
  }

  if (_devices[addr] != nullptr) {
    return _devices[addr];
  }

  i2c_device_config_t devConfig{};
  devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devConfig.device_address = addr;
  devConfig.scl_speed_hz = _freqHz;

  i2c_master_dev_handle_t handle = nullptr;
  const esp_err_t err = i2c_master_bus_add_device(_bus, &devConfig, &handle);
  if (err != ESP_OK) {
    return nullptr;
  }

  _devices[addr] = handle;
  return handle;
}

int TwoWire::transferTimeoutMs() const {
  return clampTimeoutMs(_timeoutMs);
}

uint8_t TwoWire::mapError(esp_err_t err) const {
  if (err == ESP_OK) {
    return 0U;
  }
  if (err == ESP_ERR_TIMEOUT) {
    return 5U;
  }
  if (err == ESP_ERR_INVALID_RESPONSE) {
    return 2U;
  }
  if (err == ESP_ERR_INVALID_ARG) {
    return 1U;
  }
  return 4U;
}

void TwoWire::clearPendingRead() {
  _hasPendingReadPrefix = false;
  _pendingAddress = 0U;
  _pendingTxLen = 0U;
}
