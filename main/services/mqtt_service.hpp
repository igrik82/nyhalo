#pragma once

#include <cstdint>

#include "events/event_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mqtt_client.h"

namespace app {

class MqttService {
public:
    MqttService(const char *fallback_host, uint16_t fallback_port);

    MqttService(const MqttService &) = delete;
    MqttService &operator=(const MqttService &) = delete;

    bool init();
    bool start();
    void stop();

private:
    static void taskEntry(void *arg);
    void taskLoop();
    void startClient();
    void stopClient();

    static void mqttEventHandler(void *handler_args, esp_event_base_t base,
                                 int32_t event_id, void *event_data);
    void handleMqttEvent(esp_mqtt_event_handle_t event);

    static void onWifiEvent(const Event &event, void *context);
    static void onTimeSyncEvent(const Event &event, void *context);
    static void onMdnsEvent(const Event &event, void *context);
    static void onMotionEvent(const Event &event, void *context);
    static void onTelemetryEvent(const Event &event, void *context);

    void publishTelemetry(const Event &event);
    void publishMotion(const Event &event);

    const char *m_fallback_host;
    uint16_t m_fallback_port;

    char m_mqtt_host[32] = {0};
    uint16_t m_mqtt_port = 0;

    esp_mqtt_client_handle_t m_client = nullptr;
    bool m_wifi_connected = false;
    bool m_connected = false;
    bool m_time_synced = false;

    TaskHandle_t m_task_handle = nullptr;
    StaticEventGroup_t m_event_group_buffer{};
    EventGroupHandle_t m_event_group = nullptr;

    StaticTask_t m_task_buffer{};
    StackType_t m_task_stack[6144];

    bool m_initialized = false;
    bool m_running = false;

    static constexpr EventBits_t BIT_WIFI_CONNECTED = BIT0;
    static constexpr EventBits_t BIT_MDNS_RESOLVED = BIT1;
    static constexpr EventBits_t BIT_MDNS_FAILED = BIT2;
    static constexpr EventBits_t BIT_RECONNECT_REQ = BIT3;
    static constexpr EventBits_t BIT_STOP_REQUESTED = BIT4;
    static constexpr EventBits_t BIT_TIME_SYNCED = BIT5;
};

}  // namespace app
