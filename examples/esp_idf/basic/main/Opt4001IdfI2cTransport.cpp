#include "Opt4001IdfI2cTransport.h"

#include <limits>

#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr uint8_t GENERAL_CALL_ADDRESS = 0x00;

int clampTimeoutMs(uint32_t timeoutMs) {
  const uint32_t maxTimeout = static_cast<uint32_t>(std::numeric_limits<int>::max());
  return static_cast<int>(timeoutMs > maxTimeout ? maxTimeout : timeoutMs);
}

OPT4001::Status mapEspErr(esp_err_t err, const char* context) {
  if (err == ESP_OK) {
    return OPT4001::Status::Ok();
  }
  if (err == ESP_ERR_TIMEOUT) {
    return OPT4001::Status::Error(OPT4001::Err::I2C_TIMEOUT, "I2C timeout",
                                  static_cast<int32_t>(err));
  }
  if (err == ESP_ERR_INVALID_ARG) {
    return OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM, "IDF I2C invalid argument",
                                  static_cast<int32_t>(err));
  }
  if (err == ESP_ERR_INVALID_STATE) {
    return OPT4001::Status::Error(OPT4001::Err::I2C_BUS, "IDF I2C invalid state",
                                  static_cast<int32_t>(err));
  }
  if (err == ESP_ERR_INVALID_RESPONSE) {
    // The IDF transaction API used here reports NACK/invalid response without
    // exposing whether the address or data phase failed.
    return OPT4001::Status::Error(OPT4001::Err::I2C_ERROR,
                                  "I2C NACK/invalid response; phase unknown",
                                  static_cast<int32_t>(err));
  }
  return OPT4001::Status::Error(OPT4001::Err::I2C_BUS, context, static_cast<int32_t>(err));
}

OPT4001::Status validateContext(const void* user, const Opt4001IdfI2c*& ctx) {
  ctx = static_cast<const Opt4001IdfI2c*>(user);
  if (ctx == nullptr || ctx->dev == nullptr) {
    return OPT4001::Status::Error(OPT4001::Err::I2C_BUS, "IDF I2C device not configured");
  }
  return OPT4001::Status::Ok();
}

}  // namespace

OPT4001::Status opt4001IdfI2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                                   uint32_t timeoutMs, void* user) {
  const Opt4001IdfI2c* ctx = nullptr;
  OPT4001::Status st = validateContext(user, ctx);
  if (!st.ok()) {
    return st;
  }
  if (data == nullptr || len == 0) {
    return OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM, "Invalid I2C write buffer");
  }

  i2c_master_dev_handle_t handle = nullptr;
  if (addr == ctx->address) {
    handle = ctx->dev;
  } else if (addr == GENERAL_CALL_ADDRESS) {
    handle = ctx->generalCallDev;
  }
  if (handle == nullptr) {
    return OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM, "Unsupported I2C write address",
                                  static_cast<int32_t>(addr));
  }

  const esp_err_t err = i2c_master_transmit(handle, data, len, clampTimeoutMs(timeoutMs));
  return mapEspErr(err, "I2C write failed");
}

OPT4001::Status opt4001IdfI2cWriteRead(uint8_t addr, const uint8_t* txData,
                                       size_t txLen, uint8_t* rxData,
                                       size_t rxLen, uint32_t timeoutMs,
                                       void* user) {
  const Opt4001IdfI2c* ctx = nullptr;
  OPT4001::Status st = validateContext(user, ctx);
  if (!st.ok()) {
    return st;
  }
  if (addr != ctx->address) {
    return OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM, "Unsupported I2C read address",
                                  static_cast<int32_t>(addr));
  }
  if (txData == nullptr || txLen == 0 || (rxLen > 0 && rxData == nullptr)) {
    return OPT4001::Status::Error(OPT4001::Err::INVALID_PARAM, "Invalid I2C read buffer");
  }

  const esp_err_t err = i2c_master_transmit_receive(
      ctx->dev, txData, txLen, rxData, rxLen, clampTimeoutMs(timeoutMs));
  return mapEspErr(err, "I2C write-read failed");
}

bool opt4001IdfGpioRead(int pin, void*) {
  return gpio_get_level(static_cast<gpio_num_t>(pin)) != 0;
}

uint32_t opt4001IdfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void opt4001IdfYield(void*) {
  taskYIELD();
}
