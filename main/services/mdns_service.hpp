#pragma once

#include <cstdint>

#include "events/event_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

namespace app {

class MdnsService {
public:
    MdnsService(const char *hostname, const char *mqtt_service,
                const char *mqtt_proto);

    MdnsService(const MdnsService &) = delete;
    MdnsService &operator=(const MdnsService &) = delete;

    bool init();
    bool start();
    void stop();

private:
    static void taskEntry(void *arg);
    void taskLoop();
    void publishMqttResolved(const char *ip, uint16_t port);
    void publishResolveFailed();

    static void onWifiEvent(const Event &event, void *context);

    const char *m_hostname;
    const char *m_mqtt_service;
    const char *m_mqtt_proto;

    TaskHandle_t m_task_handle = nullptr;
    StaticEventGroup_t m_event_group_buffer{};
    EventGroupHandle_t m_event_group = nullptr;

    StaticTask_t m_task_buffer{};
    StackType_t m_task_stack[4096];

    bool m_initialized = false;
    bool m_running = false;

    static constexpr EventBits_t BIT_WIFI_CONNECTED = BIT0;
    static constexpr EventBits_t BIT_WIFI_DISCONNECTED = BIT1;
    static constexpr EventBits_t BIT_STOP_REQUESTED = BIT2;
};

}  // namespace app
