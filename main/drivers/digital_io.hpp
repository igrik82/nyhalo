#pragma once
#include "soc/gpio_num.h"

namespace app {
namespace io_driver {

// NOTE: Not used intentionally
enum class PullMode { None, Up, Down };

class DigitalOutput {
public:
    struct Config {
        gpio_num_t gpio;
        bool initial_high;
    };

    bool init(const Config &config);

    bool set(bool high);
    bool toggle();

    bool state() const { return _state; }

private:
    gpio_num_t _gpio = static_cast<gpio_num_t>(-1);
    bool _initialized = false;
    bool _state = false;
};

// TODO: Add digital input
class DigitalInput {};

}  // namespace io_driver
}  // namespace app
