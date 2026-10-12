#include "app_config.hpp"

#include "driver/gpio.h"

namespace app {

static Config s_config;

void initConfig() {
    s_config.debug.heartbeat_period_ms = 5000;

    // RGB led
    s_config.rgb_led.blink_period_ms = 500;
    s_config.rgb_led.led_count = 1;

    s_config.sensors.telemetry_period_ms = 20000;

    // PIR sensor
    s_config.motion.idle_timeout_ms = 8000;
    s_config.motion.renew_period_ms = 5000;
}

const Config &getConfig() { return s_config; }

}  // namespace app
