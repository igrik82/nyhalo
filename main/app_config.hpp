#pragma once

#include <cstdint>

namespace app {

struct Config {
    // Максимальный размер очереди событий
    struct {
        size_t max_queue_size;
    } queue;

    struct {
        size_t max_subscribers;
    } dispatcher;

    // Периоды обновления событий
    struct {
        uint32_t heartbeat_period_ms;
        uint32_t wait_period_ms;  // Ожидание события в очереди
    } debug;

    // Телеметрия
    struct {
        uint32_t telemetry_period_ms;
        uint32_t wait_period_ms;  // Ожидание события в очереди
    } sensors;

    // PIR
    struct {
        uint32_t idle_timeout_ms;
        uint32_t renew_period_ms;
        uint32_t wait_period_ms;  // Ожидание события в очереди
    } motion;

    // RGB led
    struct {
        uint32_t blink_period_ms;
        uint8_t led_count;
    } rgb_led;
};

void initConfig();
const Config &getConfig();
}  // namespace app
