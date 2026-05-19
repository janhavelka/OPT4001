#include <cstdint>

#include "OPT4001/OPT4001.h"
#include "Opt4001IdfI2cTransport.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char TAG[] = "opt4001_basic";
constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
constexpr gpio_num_t INT_PIN = GPIO_NUM_NC;
constexpr uint32_t I2C_FREQ_HZ = 400000;
constexpr uint8_t OPT4001_ADDRESS = 0x45;

}  // namespace

extern "C" void app_main(void) {
  Opt4001IdfI2c transport{};
  transport.address = OPT4001_ADDRESS;
  transport.intPin = INT_PIN;

  i2c_master_bus_config_t busConfig{};
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.i2c_port = I2C_NUM_0;
  busConfig.sda_io_num = I2C_SDA;
  busConfig.scl_io_num = I2C_SCL;
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;
  ESP_ERROR_CHECK(i2c_new_master_bus(&busConfig, &transport.bus));

  i2c_device_config_t devConfig{};
  devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devConfig.device_address = OPT4001_ADDRESS;
  devConfig.scl_speed_hz = I2C_FREQ_HZ;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(transport.bus, &devConfig, &transport.dev));

  if (INT_PIN != GPIO_NUM_NC) {
    gpio_config_t gpioConfig{};
    gpioConfig.pin_bit_mask = 1ULL << static_cast<uint32_t>(INT_PIN);
    gpioConfig.mode = GPIO_MODE_INPUT;
    gpioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
    gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&gpioConfig));
  }

  OPT4001::OPT4001 sensor;
  OPT4001::Config cfg{};
  cfg.i2cWrite = opt4001IdfI2cWrite;
  cfg.i2cWriteRead = opt4001IdfI2cWriteRead;
  cfg.i2cUser = &transport;
  cfg.nowMs = opt4001IdfNowMs;
  cfg.cooperativeYield = opt4001IdfYield;
  cfg.packageVariant = OPT4001::PackageVariant::SOT_5X3;
  cfg.i2cAddress = OPT4001_ADDRESS;
  cfg.i2cTimeoutMs = 50;
  cfg.mode = OPT4001::Mode::POWER_DOWN;
  if (INT_PIN != GPIO_NUM_NC) {
    cfg.intPin = static_cast<int>(INT_PIN);
    cfg.gpioRead = opt4001IdfGpioRead;
    cfg.gpioUser = &transport;
  }

  OPT4001::Status st = sensor.begin(cfg);
  if (!st.ok()) {
    ESP_LOGE(TAG, "begin failed: %s (%d detail=%ld)", st.msg, static_cast<int>(st.code),
             static_cast<long>(st.detail));
    return;
  }

  OPT4001::DeviceIdInfo id{};
  (void)sensor.readDeviceId(id);
  ESP_LOGI(TAG, "device id raw=0x%04X didh=0x%03X didl=0x%X match=%u",
           id.raw, id.didh, id.didl, static_cast<unsigned>(id.matchesExpected));

  float lux = 0.0f;
  st = sensor.readBlockingLux(lux, OPT4001::Mode::ONE_SHOT_FORCED_AUTO, 1500);
  if (st.ok() || st.code == OPT4001::Err::CRC_ERROR) {
    ESP_LOGI(TAG, "lux=%.6f status=%s", static_cast<double>(lux), st.msg);
  } else {
    ESP_LOGW(TAG, "read unavailable: %s (%d detail=%ld)", st.msg, static_cast<int>(st.code),
             static_cast<long>(st.detail));
  }

  OPT4001::Flags flags{};
  st = sensor.readFlags(flags);
  if (st.ok()) {
    ESP_LOGI(TAG, "flags ready=%u overload=%u high=%u low=%u",
             static_cast<unsigned>(flags.conversionReady),
             static_cast<unsigned>(flags.overload),
             static_cast<unsigned>(flags.highThreshold),
             static_cast<unsigned>(flags.lowThreshold));
  }

  ESP_LOGI(TAG, "state=%u successes=%lu failures=%lu", static_cast<unsigned>(sensor.state()),
           static_cast<unsigned long>(sensor.totalSuccess()),
           static_cast<unsigned long>(sensor.totalFailures()));
}
