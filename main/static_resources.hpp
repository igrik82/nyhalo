#pragma once
#include "app_config.hpp"
#include "drivers/pir_driver.hpp"
#include "events/event_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace app::static_resources {

// ======================================
// Очередь движения
// ======================================
struct MotionRawQueue {
    static constexpr size_t LENGTH = MOTION_RAW_QUEUE_LENGTH;
    StaticQueue_t structure;
    uint8_t storage[LENGTH * sizeof(RawPirData)];
};

// Глобальные статистические объекты
extern MotionRawQueue motion_raw_queue;
}  // namespace app::static_resources
