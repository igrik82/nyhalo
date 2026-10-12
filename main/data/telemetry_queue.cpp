#include "telemetry_queue.hpp"

#include "esp_log.h"

static const char *TAG = "TELEMETRY_QUEUE";

namespace app {
namespace data {

QueueHandle_t TelemetryQueue::queue_ = nullptr;

bool TelemetryQueue::init() {
    if (queue_ != nullptr) {
        return true;
    }

    queue_ = xQueueCreate(16, sizeof(TelemetrySample));

    if (queue_ == nullptr) {
        ESP_LOGE(TAG, "failed to create telemetry queue");
        return false;
    }

    ESP_LOGI(TAG, "telemetry queue initialized");
    return true;
}

bool TelemetryQueue::push(const TelemetrySample &sample, uint32_t timeout_ms) {
    if (queue_ == nullptr) {
        ESP_LOGE(TAG, "queue not initialized");
        return false;
    }

    return xQueueSend(queue_, &sample, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

bool TelemetryQueue::pop(TelemetrySample &sample, uint32_t timeout_ms) {
    if (queue_ == nullptr) {
        ESP_LOGE(TAG, "queue not initialized");
        return false;
    }

    return xQueueReceive(queue_, &sample, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

}  // namespace data
}  // namespace app
