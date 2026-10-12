#include "sensor_emulator_service.hpp"

#include "../app_config.hpp"
#include "../data/telemetry_queue.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SENSOR_EMULATOR";

extern "C" void entrance_node_sensor_emulator_task(void *arg);

namespace app {
namespace sensor_emulator {

bool init() {
    ESP_LOGI(TAG, "sensor emulator init");
    return true;
}

void start() {
    ESP_LOGI(TAG, "sensor emulator start");

    xTaskCreate(entrance_node_sensor_emulator_task, "sensor_emu", 4096, nullptr,
                3, nullptr);
}

}  // namespace sensor_emulator
}  // namespace app

extern "C" void entrance_node_sensor_emulator_task(void *arg) {
    uint32_t counter = 0;

    while (true) {
        app::data::TelemetrySample sample{};

        // Простые псевдоданные.
        // Позже здесь будет реальный драйвер ENS160/AHT21.

        sample.temp_c = 22.0f + static_cast<float>(counter % 5) * 0.2f;
        sample.rh_pct = 40.0f + static_cast<float>(counter % 7);
        sample.tvoc = static_cast<uint16_t>(20 + (counter % 15));
        sample.aqi = 1;

        // Каждые 5 измерений меняем уровень CO2:
        // нормальный / повышенный.
        //
        // Это нужно, чтобы протестировать:
        //   очередь данных,
        //   обработку телеметрии,
        //   событие тревоги,
        //   FSM светодиода.
        const bool high_co2 = ((counter / 5) % 2) == 1;

        if (high_co2) {
            sample.eco2 = 1350;
        } else {
            sample.eco2 = 650;
        }

        if (!app::data::TelemetryQueue::push(sample, 10)) {
            ESP_LOGW(TAG, "failed to push telemetry sample");
        } else {
            ESP_LOGD(TAG, "sample pushed: eco2=%u",
                     static_cast<unsigned>(sample.eco2));
        }

        ++counter;

        vTaskDelay(pdMS_TO_TICKS(app::getConfig().sensors.telemetry_period_ms));
    }
}
