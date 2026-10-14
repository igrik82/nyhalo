#pragma once

#include "events/event_bus.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace app {

// Сервис периодической отправки heartbeat событий
class HeartbeatService {
public:
    // Конструктор сохраняет зависимости
    explicit HeartbeatService();

    // Запрет копирования
    HeartbeatService(const HeartbeatService &) = delete;
    HeartbeatService &operator=(const HeartbeatService &) = delete;

    // Двухфазная инициализация
    bool init();
    bool start();
    void stop();

private:
    // Статический трамплин для FreeRTOS
    static void taskEntry(void *arg);
    void taskLoop();

    uint32_t period_ms_;           // Кэшировано в init()
    uint32_t publish_timeout_ms_;  // Кэшировано в init()
    TaskHandle_t task_handle_ = nullptr;
    bool initialized_ = false;
};

}  // namespace app
