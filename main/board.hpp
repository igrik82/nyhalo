#pragma once

#include "drivers/led_driver.hpp"
#include "soc/gpio_num.h"

namespace app {
namespace board {
// Used pins for the board

// Led pin
constexpr led_driver::LedConfig BLINK_LED = {.gpio = GPIO_NUM_19,
                                             .active_high = true};

// RGB led pin
constexpr gpio_num_t RGB_LED = GPIO_NUM_13;

// PIR pin
constexpr gpio_num_t PIR_SENSOR = GPIO_NUM_27;

}  // namespace board
}  // namespace app
