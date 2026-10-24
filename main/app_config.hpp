#pragma once

#include <cstdint>

namespace app {

// ================================
// Параметры статических ресурсов
// ================================

// Размеры очередей
constexpr size_t MOTION_RAW_QUEUE_LENGTH = 10;

// ================================
// Параметры динамических ресурсов
// ================================
struct Config {
    // Максимальный размер очереди событий
    struct {
        size_t max_queue_size;
    } queue;

    struct {
        size_t max_subscribers;
    } dispatcher;

    // WiFi
    struct {
        uint32_t connect_timeout_ms;
        uint32_t reconnect_timeout_ms;
        uint32_t max_retries;
        uint32_t cooldown_ms;  // пауза после исчерпания лимита попыток
        uint16_t task_stack_size;
        uint8_t task_priority;
        bool power_save;  // modem sleep;
    } wifi;

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
        uint32_t debounce_timeout_ms;  // Игнорирование дребезга
        uint32_t wait_period_ms;       // Ожидание события в очереди
        uint8_t queue_lenght;
    } motion;

    // RGB led
    struct {
        uint32_t blink_period_ms;
        uint8_t led_count;
        uint8_t fade_step;
        uint32_t fade_delay_ticks;
    } rgb_led;
};

void initConfig();
const Config &getConfig();
}  // namespace app
