#include "mqtt_service.hpp"

#include <cstdio>
#include <cstring>

#include "../app_config.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "events/event_bus.hpp"
#include "events/event_dispatcher.hpp"

static const char *TAG = "MQTT";

namespace app {

MqttService::MqttService(const char *fallback_host, uint16_t fallback_port)
    : m_fallback_host(fallback_host), m_fallback_port(fallback_port) {}

bool MqttService::init() {
    if (m_initialized) return true;

    EventDispatcher::subscribe(EventType::WIFI_CONNECTED, onWifiEvent, this);
    EventDispatcher::subscribe(EventType::WIFI_DISCONNECTED, onWifiEvent, this);
    EventDispatcher::subscribe(EventType::TIME_SYNCED, onTimeSyncEvent, this);
    EventDispatcher::subscribe(EventType::MDNS_MQTT_RESOLVED, onMdnsEvent,
                               this);
    EventDispatcher::subscribe(EventType::MDNS_RESOLVE_FAILED, onMdnsEvent,
                               this);
    EventDispatcher::subscribe(EventType::TELEMETRY_UPDATED, onTelemetryEvent,
                               this);
    EventDispatcher::subscribe(EventType::MOTION_DETECTED, onMotionEvent, this);

    m_event_group = xEventGroupCreateStatic(&m_event_group_buffer);
    if (m_event_group == nullptr) {
        ESP_LOGE(TAG, "xEventGroupCreateStatic failed");
        return false;
    }

    m_initialized = true;
    return true;
}

bool MqttService::start() {
    if (!m_initialized || m_running) return false;
    const auto &config = getConfig();

    m_task_handle = xTaskCreateStatic(
        taskEntry, "mqtt", sizeof(m_task_stack) / sizeof(StackType_t), this,
        config.mqtt.task_priority, m_task_stack, &m_task_buffer);

    if (m_task_handle == nullptr) {
        ESP_LOGE(TAG, "failed to create static task");
        return false;
    }

    m_running = true;
    return true;
}

void MqttService::stop() {
    if (m_running) {
        m_running = false;
        xEventGroupSetBits(m_event_group, BIT_STOP_REQUESTED);
    }
}

void MqttService::taskEntry(void *arg) {
    auto *self = static_cast<MqttService *>(arg);
    self->taskLoop();
    vTaskDelete(nullptr);
}

void MqttService::taskLoop() {
    while (m_running) {
        // 1. Ждем подключения к Wi-Fi И синхронизации времени
        ESP_LOGI(TAG, "waiting for Wi-Fi and Time Sync...");
        xEventGroupWaitBits(m_event_group, BIT_WIFI_CONNECTED | BIT_TIME_SYNCED,
                            pdFALSE, pdTRUE, portMAX_DELAY);

        if (!m_running) break;

        // 2. Ждем результата mDNS (успех или таймаут)
        ESP_LOGI(TAG, "waiting for mDNS resolution...");
        EventBits_t mdns_bits = xEventGroupWaitBits(
            m_event_group,
            BIT_MDNS_RESOLVED | BIT_MDNS_FAILED | BIT_STOP_REQUESTED, pdFALSE,
            pdFALSE, portMAX_DELAY);

        if (mdns_bits & BIT_STOP_REQUESTED) break;

        if (mdns_bits & BIT_MDNS_RESOLVED) {
            ESP_LOGI(TAG, "mDNS resolved, using %s:%u", m_mqtt_host,
                     m_mqtt_port);
        } else if (mdns_bits & BIT_MDNS_FAILED) {
            ESP_LOGW(TAG, "mDNS failed, using fallback %s:%u", m_fallback_host,
                     m_fallback_port);
            strncpy(m_mqtt_host, m_fallback_host, sizeof(m_mqtt_host) - 1);
            m_mqtt_host[sizeof(m_mqtt_host) - 1] = '\0';
            m_mqtt_port = m_fallback_port;
        }

        // 3. Запускаем клиент
        if (!m_client && m_wifi_connected && m_time_synced) {
            startClient();
        }

        // 4. Цикл ожидания событий для реконнекта или остановки
        while (m_running && m_client) {
            EventBits_t bits = xEventGroupWaitBits(
                m_event_group, BIT_RECONNECT_REQ | BIT_STOP_REQUESTED, pdTRUE,
                pdFALSE, portMAX_DELAY);

            if (bits & BIT_STOP_REQUESTED) break;

            if (bits & BIT_RECONNECT_REQ) {
                ESP_LOGI(TAG, "Reconnect requested");
                stopClient();

                if (!m_wifi_connected || !m_time_synced) break;

                vTaskDelay(pdMS_TO_TICKS(2000));
                if (m_wifi_connected && m_time_synced &&
                    m_mqtt_host[0] != '\0') {
                    startClient();
                }
            }
        }
        stopClient();
    }
}

void MqttService::startClient() {
    const auto &config = getConfig();
    if (m_mqtt_host[0] == '\0') return;

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.hostname = m_mqtt_host;
    mqtt_cfg.broker.address.port = m_mqtt_port;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    mqtt_cfg.credentials.client_id = config.mqtt.client_id;
    mqtt_cfg.session.keepalive = config.mqtt.keepalive_s;

    m_client = esp_mqtt_client_init(&mqtt_cfg);
    if (m_client) {
        esp_mqtt_client_register_event(m_client, MQTT_EVENT_ANY,
                                       mqttEventHandler, this);
        esp_mqtt_client_start(m_client);
        ESP_LOGI(TAG, "MQTT client started for %s:%u", m_mqtt_host,
                 m_mqtt_port);
    }
}

void MqttService::stopClient() {
    if (m_client) {
        esp_mqtt_client_stop(m_client);
        esp_mqtt_client_destroy(m_client);
        m_client = nullptr;

        if (m_connected) {
            m_connected = false;
            Event evt{};
            evt.type = EventType::MQTT_DISCONNECTED;
            evt.timestamp =
                static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
            EventBus::publish(evt, getConfig().mqtt.wait_period_ms);
        }
    }
}

void MqttService::mqttEventHandler(void *handler_args, esp_event_base_t base,
                                   int32_t event_id, void *event_data) {
    auto *self = static_cast<MqttService *>(handler_args);
    self->handleMqttEvent(static_cast<esp_mqtt_event_handle_t>(event_data));
}

void MqttService::handleMqttEvent(esp_mqtt_event_handle_t event) {
    const auto &config = getConfig();
    Event evt{};
    evt.timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            m_connected = true;
            evt.type = EventType::MQTT_CONNECTED;
            EventBus::publish(evt, config.mqtt.wait_period_ms);
            break;
        case MQTT_EVENT_DISCONNECTED:
            m_connected = false;
            evt.type = EventType::MQTT_DISCONNECTED;
            EventBus::publish(evt, config.mqtt.wait_period_ms);
            break;
        default:
            break;
    }
}

void MqttService::onWifiEvent(const Event &event, void *context) {
    auto *self = static_cast<MqttService *>(context);
    if (event.type == EventType::WIFI_CONNECTED) {
        self->m_wifi_connected = true;
        xEventGroupSetBits(self->m_event_group, BIT_WIFI_CONNECTED);
    } else if (event.type == EventType::WIFI_DISCONNECTED) {
        self->m_wifi_connected = false;
        xEventGroupClearBits(self->m_event_group, BIT_WIFI_CONNECTED);
        xEventGroupClearBits(self->m_event_group,
                             BIT_MDNS_RESOLVED | BIT_MDNS_FAILED);
        if (self->m_client)
            xEventGroupSetBits(self->m_event_group, BIT_RECONNECT_REQ);
    }
}

void MqttService::onTimeSyncEvent(const Event &event, void *context) {
    auto *self = static_cast<MqttService *>(context);
    if (event.type == EventType::TIME_SYNCED) {
        self->m_time_synced = true;
        xEventGroupSetBits(self->m_event_group, BIT_TIME_SYNCED);
    }
}

void MqttService::onMdnsEvent(const Event &event, void *context) {
    auto *self = static_cast<MqttService *>(context);
    if (event.type == EventType::MDNS_MQTT_RESOLVED) {
        bool changed = (strncmp(self->m_mqtt_host, event.data.mdns.ip,
                                sizeof(self->m_mqtt_host)) != 0) ||
                       (self->m_mqtt_port != event.data.mdns.port);

        strncpy(self->m_mqtt_host, event.data.mdns.ip,
                sizeof(self->m_mqtt_host) - 1);
        self->m_mqtt_host[sizeof(self->m_mqtt_host) - 1] = '\0';
        self->m_mqtt_port = event.data.mdns.port;

        xEventGroupSetBits(self->m_event_group, BIT_MDNS_RESOLVED);
        xEventGroupClearBits(self->m_event_group, BIT_MDNS_FAILED);

        if (self->m_client && changed) {
            ESP_LOGI(TAG, "mDNS IP changed, requesting reconnect");
            xEventGroupSetBits(self->m_event_group, BIT_RECONNECT_REQ);
        }
    } else if (event.type == EventType::MDNS_RESOLVE_FAILED) {
        xEventGroupSetBits(self->m_event_group, BIT_MDNS_FAILED);
    }
}

void MqttService::onMotionEvent(const Event &event, void *context) {
    auto *self = static_cast<MqttService *>(context);
    if (self->m_connected) self->publishMotion(event);
}

void MqttService::onTelemetryEvent(const Event &event, void *context) {
    auto *self = static_cast<MqttService *>(context);
    if (self->m_connected) self->publishTelemetry(event);
}

void MqttService::publishMotion(const Event &event) {
    const auto &config = getConfig();
    char payload[32];
    snprintf(payload, sizeof(payload), "{\"count\":%u}",
             static_cast<uint16_t>(event.data.motion.count));
    esp_mqtt_client_publish(m_client, config.mqtt.topic_motion, payload, 0, 1,
                            0);
}

void MqttService::publishTelemetry(const Event &event) {
    const auto &config = getConfig();
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"temp\":%.2f,\"rh\":%.2f,\"eco2\":%u,\"tvoc\":%u,\"aqi\":%u}",
             event.data.telemetry.temp_c, event.data.telemetry.rh_pct,
             event.data.telemetry.eco2, event.data.telemetry.tvoc,
             event.data.telemetry.aqi);
    esp_mqtt_client_publish(m_client, config.mqtt.topic_telemetry, payload, 0,
                            1, 0);
}

}  // namespace app
