#pragma once

#include <cstddef>
#include <cstdint>

#include "OPT4001/OPT4001.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

struct Opt4001IdfI2c {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t dev = nullptr;
  i2c_master_dev_handle_t generalCallDev = nullptr;
  uint8_t address = 0x45;
  gpio_num_t intPin = GPIO_NUM_NC;
};

OPT4001::Status opt4001IdfI2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                                   uint32_t timeoutMs, void* user);
OPT4001::Status opt4001IdfI2cWriteRead(uint8_t addr, const uint8_t* txData,
                                       size_t txLen, uint8_t* rxData,
                                       size_t rxLen, uint32_t timeoutMs,
                                       void* user);
bool opt4001IdfGpioRead(int pin, void* user);
uint32_t opt4001IdfNowMs(void* user);
void opt4001IdfYield(void* user);
