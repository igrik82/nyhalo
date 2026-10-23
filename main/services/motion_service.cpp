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
    m_debounce_timeout_ms = config.motion.debounce_timeout_ms;

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

// void MotionService::taskLoop() {
//     RawPirData data;
//     Event event{};

//     // Состояние FSM (локально в задаче, сбрасывается при перезагрузке, что
//     // логично)
//     bool motion_active = false;
//     uint32_t last_active_ms = 0;

//     ESP_LOGI(TAG, "motion service started");

//     while (true) {
//         const uint32_t now_ms =
//             static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
//         bool has_event = false;

//         bool received =
//             xQueueReceive(m_raw_data_queue, &data,
//                           pdMS_TO_TICKS(m_idle_timeout_ms)) == pdTRUE;

//         event.timestamp = static_cast<uint32_t>(esp_timer_get_time() /
//         1000ULL);

//         bool has_event = false;

//         if (received) {
//             // has_event = fsm_.process(data, event);
//             has_event = true;
//         } else {
//             // has_event = fsm_.checkTimeout(event.timestamp, event);
//             has_event = true;
//         }

//         if (has_event) {
//             if (!EventBus::publish(event, m_wait_period_ms)) {
//             }
//         }
//     }
// }
void MotionService::taskLoop() {
    RawPirData data;
    Event event{};

    // Состояние FSM (локально в задаче, сбрасывается при перезагрузке, что
    // логично)
    bool motion_active = false;
    uint32_t last_active_ms = 0;
    uint32_t last_edge_ms = 0;  // Для дебаунса

    ESP_LOGI(TAG, "motion fsm started");

    while (true) {
        // Ждём данные не дольше таймаута отсутствия движения
        const BaseType_t received = xQueueReceive(
            m_raw_data_queue, &data, pdMS_TO_TICKS(m_idle_timeout_ms));

        const uint32_t now_ms =
            static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
        bool has_event = false;

        if (received == pdTRUE) {
            // Дебаунс: игнорируем фронт, если с момента последнего прошло <
            // debounce_period_ms
            if ((now_ms - last_edge_ms) < m_debounce_timeout_ms) {
                continue;  // Пропускаем дребезг
            }
            if (data.active) {
                last_active_ms = now_ms;
                if (!motion_active) {
                    motion_active = true;
                    event.type = EventType::MOTION_DETECTED;
                    event.timestamp = now_ms;
                    has_event = true;
                }
            } else {
                // Сброс уровня от PIR игнорируем, ждём таймаута
                // без повторных срабатываний для генерации CLEARED
            }
        } else {
            // Таймаут истёк, новых данных не было
            if (motion_active) {
                // Проверка разности uint32_t безопасна к переполнению счётчика
                // времени (~49 дней)
                if ((now_ms - last_active_ms) >= m_idle_timeout_ms) {
                    motion_active = false;
                    event.type = EventType::MOTION_CLEARED;
                    event.timestamp = now_ms;
                    has_event = true;
                }
            }
        }

        if (has_event) {
            if (!EventBus::publish(event, m_wait_period_ms)) {
                ESP_LOGW(TAG, "failed to publish motion event");
            }
        }
    }
}
}  // namespace app
