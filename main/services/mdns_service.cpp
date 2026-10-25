#include "mdns_service.hpp"

#include <cstring>

#include "../app_config.hpp"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "events/event_bus.hpp"
#include "events/event_dispatcher.hpp"
#include "mdns.h"

static const char *TAG = "MDNS";

namespace app {

MdnsService::MdnsService(const char *hostname, const char *mqtt_service,
                         const char *mqtt_proto)
    : m_hostname(hostname),
      m_mqtt_service(mqtt_service),
      m_mqtt_proto(mqtt_proto) {}

bool MdnsService::init() {
    if (m_initialized) return true;

    EventDispatcher::subscribe(EventType::WIFI_CONNECTED, onWifiEvent, this);
    EventDispatcher::subscribe(EventType::WIFI_DISCONNECTED, onWifiEvent, this);

    esp_err_t err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "mdns_init failed: %s", esp_err_to_name(err));
        return false;
    }

    mdns_hostname_set(m_hostname);
    mdns_instance_name_set("Entrance Node");

    m_event_group = xEventGroupCreateStatic(&m_event_group_buffer);
    if (m_event_group == nullptr) {
        ESP_LOGE(TAG, "xEventGroupCreateStatic failed");
        return false;
    }

    m_initialized = true;
    ESP_LOGI(TAG, "initialized");
    return true;
}

bool MdnsService::start() {
    if (!m_initialized || m_running) return false;
    const auto &config = getConfig();

    m_task_handle = xTaskCreateStatic(
        taskEntry, "mdns", sizeof(m_task_stack) / sizeof(StackType_t), this,
        config.mdns.task_priority, m_task_stack, &m_task_buffer);

    if (m_task_handle == nullptr) {
        ESP_LOGE(TAG, "failed to create static task");
        return false;
    }

    m_running = true;
    return true;
}

void MdnsService::stop() {
    if (m_running) {
        m_running = false;
        xEventGroupSetBits(m_event_group, BIT_STOP_REQUESTED);
    }
}

void MdnsService::onWifiEvent(const Event &event, void *context) {
    auto *self = static_cast<MdnsService *>(context);
    if (event.type == EventType::WIFI_CONNECTED) {
        xEventGroupClearBits(self->m_event_group, BIT_WIFI_DISCONNECTED);
        xEventGroupSetBits(self->m_event_group, BIT_WIFI_CONNECTED);
    } else if (event.type == EventType::WIFI_DISCONNECTED) {
        xEventGroupClearBits(self->m_event_group, BIT_WIFI_CONNECTED);
        xEventGroupSetBits(self->m_event_group, BIT_WIFI_DISCONNECTED);
    }
}

void MdnsService::taskEntry(void *arg) {
    auto *self = static_cast<MdnsService *>(arg);
    self->taskLoop();
    vTaskDelete(nullptr);
}

void MdnsService::taskLoop() {
    const auto &config = getConfig();

    while (m_running) {
        ESP_LOGI(TAG, "waiting for Wi-Fi connection...");
        EventBits_t bits = xEventGroupWaitBits(
            m_event_group, BIT_WIFI_CONNECTED | BIT_STOP_REQUESTED, pdFALSE,
            pdFALSE, portMAX_DELAY);
        if (bits & BIT_STOP_REQUESTED) break;

        ESP_LOGI(TAG, "Wi-Fi connected, starting mDNS queries");
        uint32_t start_time =
            static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
        bool resolved = false;

        while (m_running) {
            mdns_result_t *results = NULL;
            esp_err_t err =
                mdns_query_ptr(m_mqtt_service, m_mqtt_proto, 3000, 1, &results);

            if (err == ESP_OK && results != NULL) {
                char ip_str[16] = {0};
                uint16_t port = 0;

                mdns_ip_addr_t *addr = results->addr;
                while (addr) {
                    if (addr->addr.type == ESP_IPADDR_TYPE_V4) {
                        snprintf(ip_str, sizeof(ip_str), IPSTR,
                                 IP2STR(&addr->addr.u_addr.ip4));
                        port = results->port;
                        break;
                    }
                    addr = addr->next;
                }

                if (ip_str[0] != '\0') {
                    ESP_LOGI(TAG, "mDNS query: found broker %s:%u", ip_str,
                             port);
                    publishMqttResolved(ip_str, port);
                    resolved = true;
                }
                mdns_query_results_free(results);
            }

            // Проверка таймаута
            if (!resolved) {
                uint32_t current_time =
                    static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
                if ((current_time - start_time) >=
                    config.mdns.resolve_timeout_ms) {
                    ESP_LOGW(TAG, "mDNS resolution timeout (%u ms)",
                             config.mdns.resolve_timeout_ms);
                    publishResolveFailed();
                    resolved = true;  // Останавливаем таймер, но фоновый опрос
                                      // продолжается
                }
            }

            bits = xEventGroupWaitBits(
                m_event_group, BIT_STOP_REQUESTED | BIT_WIFI_DISCONNECTED,
                pdTRUE, pdFALSE, pdMS_TO_TICKS(config.mdns.query_period_ms));

            if (bits & BIT_STOP_REQUESTED) return;
            if (bits & BIT_WIFI_DISCONNECTED) {
                ESP_LOGW(TAG, "Wi-Fi disconnected, stopping queries");
                break;  // Выход во внешний цикл для ожидания Wi-Fi
            }
        }
    }
}

void MdnsService::publishMqttResolved(const char *ip, uint16_t port) {
    const auto &config = getConfig();
    Event evt{};
    evt.type = EventType::MDNS_MQTT_RESOLVED;
    evt.timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);

    strncpy(evt.data.mdns.ip, ip, sizeof(evt.data.mdns.ip) - 1);
    evt.data.mdns.ip[sizeof(evt.data.mdns.ip) - 1] = '\0';
    evt.data.mdns.port = port;

    EventBus::publish(evt, config.mqtt.wait_period_ms);
}

void MdnsService::publishResolveFailed() {
    const auto &config = getConfig();
    Event evt{};
    evt.type = EventType::MDNS_RESOLVE_FAILED;
    evt.timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);

    EventBus::publish(evt, config.mqtt.wait_period_ms);
}

}  // namespace app
