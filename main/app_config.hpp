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

    // Настройки NTP
    struct {
        char server[64];    // NTP-сервер (например, "pool.ntp.org")
        char timezone[32];  // POSIX-формат (например, "MSK-3" или "UTC-0")
        uint32_t sync_check_period_ms;  // Период проверки синхронизации
        uint32_t sync_timeout_ms;       // Максимальное время ожидания первой
                                        // синхронизации
    } ntp;

    // MQTT
    struct {
        char fallback_host[32];
        uint16_t fallback_port;
        uint32_t keepalive_s;
        uint16_t task_stack_size;
        uint8_t task_priority;
        char client_id[16];
        char topic_motion[32];
        char topic_telemetry[32];
        uint32_t wait_period_ms;
    } mqtt;

    // mDNS
    struct {
        char hostname[16];
        char mqtt_service[16];  // "_mqtt"
        char mqtt_proto[16];    // "_tcp"
        uint32_t query_period_ms;
        uint32_t resolve_timeout_ms;
        uint16_t task_stack_size;
        uint8_t task_priority;
    } mdns;
};

void initConfig();
const Config &getConfig();
}  // namespace app
