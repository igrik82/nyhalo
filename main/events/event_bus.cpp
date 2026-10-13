#include "event_bus.hpp"

#include "../app_config.hpp"
#include "esp_log.h"

static const char *TAG = "EVENT_BUS";

namespace app {

QueueHandle_t EventBus::queue_ = nullptr;

bool EventBus::init() {
    if (queue_ != nullptr) {
        return true;
    }

    const auto &config = getConfig();

    queue_ = xQueueCreate(config.queue.max_queue_size, sizeof(Event));
    if (queue_ == nullptr) {
        ESP_LOGE(TAG, "failed to create event queue (size=%u)",
                 static_cast<unsigned>(config.queue.max_queue_size));
        return false;
    }

    ESP_LOGI(TAG, "event bus initialized (queue_size=%u)",
             static_cast<unsigned>(config.queue.max_queue_size));
    return true;
}

bool EventBus::publish(const Event &event, uint32_t timeout_ms) {
    if (queue_ == nullptr) {
        ESP_LOGE(TAG, "event bus not initialized");
        return false;
    }

    return xQueueSend(queue_, &event, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

bool EventBus::receive(Event &event, uint32_t timeout_ms) {
    if (queue_ == nullptr) {
        ESP_LOGE(TAG, "event bus not initialized");
        return false;
    }

    return xQueueReceive(queue_, &event, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

}  // namespace app
