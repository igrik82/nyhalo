#pragma once

#include "soc/gpio_num.h"

namespace app {
namespace led_driver {

struct LedConfig {
    gpio_num_t gpio;
    bool active_high;
};

bool init(const LedConfig &config);

bool set(bool high);
bool on();
bool off();
bool toggle();

bool isOn();

}  // namespace led_driver
}  // namespace app
