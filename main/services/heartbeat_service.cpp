#include "heartbeat_service.hpp"

#include "../app_config.hpp"
#include "../app_events.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "HEARTBEAT_SERVICE";

extern "C" void entrance_node_heartbeat_task(void *arg);

namespace app {
namespace heartbeat {

bool init() {
    ESP_LOGI(TAG, "heartbeat service init");
    return true;
}

void start() {
    ESP_LOGI(TAG, "heartbeat service start");

    xTaskCreate(entrance_node_heartbeat_task, "heartbeat", 4096, nullptr, 3,
                nullptr);
}

}  // namespace heartbeat
}  // namespace app

extern "C" void entrance_node_heartbeat_task(void *arg) {
    uint32_t counter = 0;

    const uint32_t period_ms = app::getConfig().debug.heartbeat_period_ms;

    while (true) {
        app::events::HeartbeatData heartbeat{};

        heartbeat.timestamp_ms =
            static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);

        heartbeat.count = ++counter;

        app::events::publish(app::events::HEARTBEAT, &heartbeat,
                             sizeof(heartbeat));

        ESP_LOGD(TAG, "heartbeat %lu",
                 static_cast<unsigned long>(heartbeat.count));

        vTaskDelay(pdMS_TO_TICKS(period_ms));
    }
}
