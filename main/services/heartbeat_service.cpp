#include "heartbeat_service.hpp"

#include "../app_config.hpp"
#include "esp_log.h"
#include "esp_timer.h"

constexpr static const char *TAG = "HEARTBEAT";

namespace app {

HeartbeatService::HeartbeatService() {}

bool HeartbeatService::init() {
    if (initialized_) {
        return true;
    }

    // Кэшируем нужные значения из конфига ОДИН РАЗ
    const auto &config = getConfig();
    publish_timeout_ms_ = config.debug.heartbeat_period_ms;
    period_ms_ = config.debug.wait_period_ms;

    ESP_LOGI(TAG, "heartbeat service init (period=%u ms, timeout=%u ms)",
             period_ms_, publish_timeout_ms_);
    initialized_ = true;
    return true;
}

bool HeartbeatService::start() {
    if (!initialized_ || task_handle_ != nullptr) {
        ESP_LOGE(TAG, "heartbeat service already started");
        return false;
    }

    ESP_LOGI(TAG, "heartbeat service start");

    if (xTaskCreate(taskEntry, "heartbeat", 2048, this, 3, &task_handle_) !=
        pdPASS) {
        ESP_LOGE(TAG, "failed to create task");
        return false;
    }

    return true;
}

void HeartbeatService::stop() {
    if (task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
}

void HeartbeatService::taskEntry(void *arg) {
    static_cast<HeartbeatService *>(arg)->taskLoop();
}

void HeartbeatService::taskLoop() {
    uint32_t counter = 0;

    while (true) {
        Event event{};
        event.type = EventType::HEARTBEAT;
        event.timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
        event.data.count = ++counter;

        // Используем КЭШИРОВАННОЕ значение, без вызова getConfig()
        if (!EventBus::publish(event, period_ms_)) {
            ESP_LOGW(TAG, "failed to publish heartbeat #%lu",
                     static_cast<unsigned long>(counter));
        }

        ESP_LOGI(TAG, "heartbeat #%lu (timestamp=%lu)", counter,
                 static_cast<unsigned long>(event.timestamp));

        // Используем КЭШИРОВАННОЕ значение
        vTaskDelay(pdMS_TO_TICKS(publish_timeout_ms_));
    }
}

}  // namespace app
