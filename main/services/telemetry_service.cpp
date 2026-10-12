#include "telemetry_service.hpp"

#include "../app_events.hpp"
#include "../data/telemetry_queue.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TELEMETRY_SERVICE";

extern "C" void entrance_node_telemetry_task(void *arg);

namespace app {
namespace telemetry {

bool init() {
    ESP_LOGI(TAG, "telemetry service init");
    return true;
}

void start() {
    ESP_LOGI(TAG, "telemetry service start");

    xTaskCreate(entrance_node_telemetry_task, "telemetry", 4096, nullptr, 4,
                nullptr);
}

}  // namespace telemetry
}  // namespace app

extern "C" void entrance_node_telemetry_task(void *arg) {
    bool alert_active = false;

    while (true) {
        app::data::TelemetrySample sample{};

        if (!app::data::TelemetryQueue::pop(sample, 1000)) {
            continue;
        }

        app::events::TelemetryData telemetry{};

        telemetry.timestamp_ms =
            static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);

        telemetry.temp_c = sample.temp_c;
        telemetry.rh_pct = sample.rh_pct;
        telemetry.eco2 = sample.eco2;
        telemetry.tvoc = sample.tvoc;
        telemetry.aqi = sample.aqi;

        ESP_LOGI(TAG,
                 "telemetry: temp=%.1f C, rh=%.1f %%, eco2=%u, tvoc=%u, aqi=%u",
                 static_cast<double>(telemetry.temp_c),
                 static_cast<double>(telemetry.rh_pct),
                 static_cast<unsigned>(telemetry.eco2),
                 static_cast<unsigned>(telemetry.tvoc),
                 static_cast<unsigned>(telemetry.aqi));

        app::events::publish(app::events::TELEMETRY_UPDATED, &telemetry,
                             sizeof(telemetry));

        // Простая логика тревоги.
        //
        // Пороги пока захардкожены для демонстрации.
        // Позже их нужно вынести в конфиг.

        constexpr uint16_t ALERT_THRESHOLD_ON = 1300;
        constexpr uint16_t ALERT_THRESHOLD_OFF = 900;

        if (!alert_active && telemetry.eco2 >= ALERT_THRESHOLD_ON) {
            alert_active = true;

            app::events::AlertData alert{};
            alert.timestamp_ms = telemetry.timestamp_ms;
            alert.level = 1;
            alert.eco2 = telemetry.eco2;

            ESP_LOGW(TAG, "air alert active: eco2=%u",
                     static_cast<unsigned>(alert.eco2));

            app::events::publish(app::events::ALERT_ACTIVE, &alert,
                                 sizeof(alert));
        } else if (alert_active && telemetry.eco2 <= ALERT_THRESHOLD_OFF) {
            alert_active = false;

            app::events::AlertData alert{};
            alert.timestamp_ms = telemetry.timestamp_ms;
            alert.level = 0;
            alert.eco2 = telemetry.eco2;

            ESP_LOGI(TAG, "air alert cleared: eco2=%u",
                     static_cast<unsigned>(alert.eco2));

            app::events::publish(app::events::ALERT_CLEARED, &alert,
                                 sizeof(alert));
        }
    }
}
