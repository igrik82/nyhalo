#pragma once

#include <cstdint>

namespace app {

struct Config {
    struct {
        uint32_t heartbeat_period_ms;
    } debug;

    struct {
        uint32_t telemetry_period_ms;
    } sensors;

    struct {
        uint32_t idle_timeout_ms;
        uint32_t renew_period_ms;
    } motion;

    struct {
        uint32_t blink_period_ms;
        uint8_t led_count;
    } rgb_led;
};

void initConfig();
const Config &getConfig();
}  // namespace app
