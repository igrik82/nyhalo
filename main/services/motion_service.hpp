#pragma once

#include "../static_resources.hpp"
#include "app_config.hpp"
#include "drivers/pir_driver.hpp"
#include "events/event_bus.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace app {

class MotionService {
public:
    explicit MotionService(QueueHandle_t raw_data_queue);

    ~MotionService() = default;
    MotionService(const MotionService &) = delete;
    MotionService &operator=(const MotionService &) = delete;

    bool init(uint32_t idle_timeout_ms, uint32_t wait_period_ms);
    bool start();
    void stop();

private:
    static void taskEntry(void *arg);
    void taskLoop();

    uint32_t m_idle_timeout_ms;
    uint32_t m_wait_period_ms;
    uint32_t m_debounce_timeout_ms;

    QueueHandle_t m_raw_data_queue = nullptr;
    TaskHandle_t m_task_handle = nullptr;
    bool m_initialized = false;
};
}  // namespace app
