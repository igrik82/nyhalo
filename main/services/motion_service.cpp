#include "motion_service.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "events/event_types.hpp"
#include "freertos/projdefs.h"

constexpr static const char *TAG = "MOTION_SERVICE";

namespace app {
MotionService::MotionService(QueueHandle_t raw_data_queue)
    : m_raw_data_queue(raw_data_queue) {}

bool MotionService::init(uint32_t idle_timeout_ms, uint32_t wait_period_ms) {
    if (m_initialized) return true;
    if (m_raw_data_queue == nullptr) {
        ESP_LOGE(TAG, "raw data queue is null");
        return false;
    }

    const auto &config = getConfig();
    m_idle_timeout_ms = config.motion.idle_timeout_ms;
    m_wait_period_ms = config.motion.wait_period_ms;

    m_initialized = true;
    return true;
}

bool MotionService::start() {
    if (!m_initialized || m_task_handle != nullptr) return false;
    if (xTaskCreate(taskEntry, "motion", 2048, this, 3, &m_task_handle) !=
        pdPASS) {
        ESP_LOGE(TAG, "failed to create task");
        return false;
    }
    ESP_LOGI(TAG, "motion service started");
    return true;
}

void MotionService::stop() {
    if (m_task_handle == nullptr) return;
    vTaskDelete(m_task_handle);
    m_task_handle = nullptr;
}

void MotionService::taskEntry(void *arg) {
    MotionService *service = static_cast<MotionService *>(arg);
    service->taskLoop();
}

void MotionService::taskLoop() {
    uint32_t counter{0};
    RawPirData data;
    Event event{};
    event.type = EventType::MOTION;
    ESP_LOGI(TAG, "motion service started");

    while (true) {
        bool received =
            xQueueReceive(m_raw_data_queue, &data,
                          pdMS_TO_TICKS(m_idle_timeout_ms)) == pdTRUE;

        event.timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
        event.data.count = ++counter;

        bool has_event = false;

        if (received) {
            // has_event = fsm_.process(data, event);
            has_event = true;
        } else {
            // has_event = fsm_.checkTimeout(event.timestamp, event);
            has_event = true;
        }

        if (has_event) {
            if (!EventBus::publish(event, m_wait_period_ms)) {
                ESP_LOGW(TAG, "failed to publish motion #%lu",
                         static_cast<unsigned long>(counter));
            }
        }
    }
}
}  // namespace app
