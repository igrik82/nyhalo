#include "app_core.hpp"

#include "app_config.hpp"
#include "app_events.hpp"
#include "board.hpp"
#include "data/telemetry_queue.hpp"
#include "drivers/rgb_led_driver.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "services/heartbeat_service.hpp"
#include "services/sensor_emulator_service.hpp"
#include "services/status_led_service.hpp"
#include "services/telemetry_service.hpp"

static const char *TAG = "APP_CORE";

namespace app {

bool init() {
    ESP_LOGI(TAG, "application init");

    initConfig();

    // Создаём стандартный цикл событий.
    //
    // Дальше все события будут идти через него.
    const esp_err_t event_loop_err = esp_event_loop_create_default();

    if (event_loop_err != ESP_OK && event_loop_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "failed to create default event loop: %s",
                 esp_err_to_name(event_loop_err));
        return false;
    }

    if (!data::TelemetryQueue::init()) {
        ESP_LOGE(TAG, "failed to init telemetry queue");
        return false;
    }

    if (!heartbeat::init()) {
        ESP_LOGE(TAG, "failed to init heartbeat service");
        return false;
    }

    if (!sensor_emulator::init()) {
        ESP_LOGE(TAG, "failed to init sensor emulator service");
        return false;
    }

    if (!telemetry::init()) {
        ESP_LOGE(TAG, "failed to init telemetry service");
        return false;
    }

    if (!status_led::init()) {
        ESP_LOGE(TAG, "failed to init status led service");
        return false;
    }

    return true;
}

void start() {
    ESP_LOGI(TAG, "application start");

    heartbeat::start();
    sensor_emulator::start();
    telemetry::start();
    status_led::start();

    events::publish(events::SYSTEM_BOOT, nullptr, 0);

    app::led_driver::RgbLedDriver g_rgb_led(app::board::RGB_LED, 1);
    g_rgb_led.init();
    g_rgb_led.start();
    g_rgb_led.setColor(0, 255, 0);  // Зеленый
    g_rgb_led.setMode(app::led_driver::Mode::SOLID);
    vTaskDelay(pdMS_TO_TICKS(5000));
    g_rgb_led.setColor(0, 0, 255);  // Зеленый
    g_rgb_led.setMode(app::led_driver::Mode::BLINK);
    vTaskDelay(pdMS_TO_TICKS(5000));
    g_rgb_led.setColor(255, 0, 0);  // Красный
    g_rgb_led.setMode(app::led_driver::Mode::PULSE);
    vTaskDelay(5000);

    ESP_LOGI(TAG, "application started");
}

}  // namespace app
