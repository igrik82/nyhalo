#include "event_dispatcher_task.hpp"

#include "../app_config.hpp"
#include "esp_log.h"
#include "event_bus.hpp"
#include "event_dispatcher.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "EVENT_DISPATCHER_TASK";

namespace app {

TaskHandle_t EventDispatcherTask::task_handle_ = nullptr;

bool EventDispatcherTask::init() {
    ESP_LOGI(TAG, "event dispatcher task init");
    return true;
}

bool EventDispatcherTask::start() {
    if (task_handle_ != nullptr) {
        return false;
    }

    ESP_LOGI(TAG, "event dispatcher task start");

    if (xTaskCreate(taskEntry, "evt_disp", 4096, nullptr, 5, &task_handle_) !=
        pdPASS) {
        ESP_LOGE(TAG, "failed to create task");
        return false;
    }

    return true;
}

void EventDispatcherTask::stop() {
    if (task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
}

void EventDispatcherTask::taskEntry(void *arg) { taskLoop(); }

void EventDispatcherTask::taskLoop() {
    Event event{};

    while (true) {
        // NOTE: Рассмотреть случай, когда диспетчер должен делать что-то ещё
        // (watchdog, таймеры)
        if (EventBus::receive(event, portMAX_DELAY)) {
            EventDispatcher::dispatch(event);
        }
    }
}

}  // namespace app
