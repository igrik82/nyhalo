#pragma once

#include "event_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace app {

// Шина событий поверх очереди FreeRTOS
class EventBus {
public:
    static bool init();
    static bool publish(const Event &event, uint32_t timeout_ms);
    static bool receive(Event &event, uint32_t timeout_ms);

private:
    static QueueHandle_t queue_;
};

}  // namespace app
