#include "app_config.hpp"

#include "driver/gpio.h"

namespace app {

static Config s_config;

// Примерное время ожидание в очереди события
// Heartbeat 5 мс Не критично, можно пропустить
// TelemetryUpdated 10 мс Обычная телеметрия, не срочно
// MotionDetected 20 мс Важное, но не критичное
// AlertActivated 50 мс Критичное, нельзя терять
// SystemBoot 100 мс Очень важное, ждём долго
// ErrorOccurred portMAX_DELAY Критичная ошибка, ждём бесконечно

void initConfig() {
    // Debug
    s_config.debug.heartbeat_period_ms = 50000;
    s_config.debug.wait_period_ms = 5;

    // Queue
    s_config.queue.max_queue_size = 32;

    // Dispatcher
    s_config.dispatcher.max_subscribers = 32;

    // WiFi
    s_config.wifi.connect_timeout_ms = 10000;
    s_config.wifi.reconnect_timeout_ms = 15000;
    s_config.wifi.max_retries = 10;
    s_config.wifi.cooldown_ms = 30000;
    s_config.wifi.task_stack_size = 4096;
    s_config.wifi.task_priority = 3;
    s_config.wifi.power_save = false;

    // RGB led
    s_config.rgb_led.blink_period_ms = 500;
    s_config.rgb_led.led_count = 1;
    s_config.rgb_led.fade_step = 3;
    s_config.rgb_led.fade_delay_ticks = 30;

    // Telemetry
    s_config.sensors.telemetry_period_ms = 20000;
    s_config.sensors.wait_period_ms = 10;

    // PIR sensor
    s_config.motion.idle_timeout_ms = 20000;
    s_config.motion.debounce_timeout_ms = 200;
    s_config.motion.wait_period_ms = 20;
    s_config.motion.queue_lenght = 10;
}

const Config &getConfig() { return s_config; }

}  // namespace app
