#pragma once

#include <cstdint>

#include "esp_event.h"
#include "events/event_bus.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

namespace app {

enum class WifiState : uint8_t {
    DISCONNECTED = 0,
    CONNECTING,
    CONNECTED,
    FAILED,
};

// Сервис WiFi-станции. Обработчики esp_event только фиксируют факт и ставят
// биты; политика переподключений (задержки, счётчик) — в задаче сервиса.
class WifiService {
public:
    WifiService(const char *ssid, const char *password);

    WifiService(const WifiService &) = delete;
    WifiService &operator=(const WifiService &) = delete;

    bool init();
    bool start();
    void stop();

    WifiState getState() const { return m_state; }
    const char *getIpAddress() const { return m_ip_address; }  // "" пока нет IP

private:
    static void wifiEventHandler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);
    static void taskEntry(void *arg);
    void taskLoop();
    void publishEvent(EventType type);

    const char *m_ssid = nullptr;
    const char *m_password = nullptr;

    volatile WifiState m_state = WifiState::DISCONNECTED;
    char m_ip_address[16] = {0};
    uint32_t m_retry_count = 0;

    // Статический буфер EventGroup: динамической памяти нет,
    // живёт в .bss вместе с объектом сервиса
    StaticEventGroup_t m_event_group_buffer{};
    EventGroupHandle_t m_event_group = nullptr;
    TaskHandle_t m_task_handle = nullptr;

    bool m_initialized = false;
    bool m_running = false;

    static constexpr EventBits_t BIT_CONNECTED = BIT0;
    static constexpr EventBits_t BIT_DISCONNECTED = BIT1;
};

}  // namespace app
