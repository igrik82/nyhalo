#include "wifi_service.hpp"

#include <cstdio>
#include <cstring>

#include "app_config.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/projdefs.h"

namespace app {

static const char *TAG = "WIFI_SERVICE";

WifiService::WifiService(const char *ssid, const char *password)
    : m_ssid(ssid), m_password(password) {}

bool WifiService::init() {
    if (m_initialized) return true;

    if (m_ssid == nullptr) {
        ESP_LOGE(TAG, "ssid is null");
        return false;
    }

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default: %s",
                 esp_err_to_name(ret));
        return false;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_init);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &WifiService::wifiEventHandler,
                                              this, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register WIFI_EVENT: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &WifiService::wifiEventHandler,
                                              this, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register IP_EVENT: %s", esp_err_to_name(ret));
        return false;
    }

    m_event_group = xEventGroupCreateStatic(&m_event_group_buffer);
    if (m_event_group == nullptr) {
        ESP_LOGE(TAG, "xEventGroupCreateStatic failed");
        return false;
    }

    m_initialized = true;
    ESP_LOGI(TAG, "initialized");
    return true;
}

bool WifiService::start() {
    if (!m_initialized || m_running) return false;

    const Config &config = getConfig();

    wifi_config_t wifi_config = {};
    strncpy(reinterpret_cast<char *>(wifi_config.sta.ssid), m_ssid,
            sizeof(wifi_config.sta.ssid) - 1);

    const bool has_password = (m_password != nullptr && m_password[0] != '\0');
    if (has_password) {
        strncpy(reinterpret_cast<char *>(wifi_config.sta.password), m_password,
                sizeof(wifi_config.sta.password) - 1);
    }

    wifi_config.sta.threshold.authmode =
        has_password ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;

    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start: %s", esp_err_to_name(ret));
        return false;
    }

    esp_wifi_set_ps(config.wifi.power_save ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);

    if (xTaskCreate(taskEntry, "wifi", config.wifi.task_stack_size, this,
                    config.wifi.task_priority, &m_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "failed to create task");
        return false;
    }

    m_running = true;
    ESP_LOGI(TAG, "started, connecting to \"%s\"", m_ssid);
    return true;
}

void WifiService::stop() {
    if (!m_running) return;
    m_running = false;

    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }

    esp_wifi_disconnect();
    esp_wifi_stop();
    ESP_LOGI(TAG, "stopped");
}

void WifiService::wifiEventHandler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data) {
    WifiService *self = static_cast<WifiService *>(arg);
    if (self == nullptr) return;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
        self->m_state = WifiState::CONNECTING;
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *dc =
            static_cast<const wifi_event_sta_disconnected_t *>(event_data);
        ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED, reason=%d", dc->reason);
        self->m_state = WifiState::DISCONNECTED;
        self->m_ip_address[0] = '\0';
        self->publishEvent(EventType::WIFI_DISCONNECTED);
        xEventGroupSetBits(self->m_event_group, BIT_DISCONNECTED);
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *ip_event =
            static_cast<const ip_event_got_ip_t *>(event_data);
        snprintf(self->m_ip_address, sizeof(self->m_ip_address), IPSTR,
                 IP2STR(&ip_event->ip_info.ip));
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP: %s", self->m_ip_address);
        self->m_state = WifiState::CONNECTED;
        self->publishEvent(EventType::WIFI_CONNECTED);
        xEventGroupSetBits(self->m_event_group, BIT_CONNECTED);
        return;
    }

    ESP_LOGD(TAG, "unhandled event: base=%p, id=%ld", event_base, event_id);
}

void WifiService::taskEntry(void *arg) {
    static_cast<WifiService *>(arg)->taskLoop();
}

void WifiService::taskLoop() {
    const Config &config = getConfig();

    while (m_running) {
        EventBits_t bits = xEventGroupWaitBits(
            m_event_group, BIT_CONNECTED | BIT_DISCONNECTED, pdTRUE, pdFALSE,
            pdMS_TO_TICKS(config.wifi.connect_timeout_ms));

        if (bits & BIT_CONNECTED) {
            m_retry_count = 0;
            ESP_LOGI(TAG, "connected, ip=%s", m_ip_address);
            // Продолжаем цикл - следующий xEventGroupWaitBits будет ждать
            // обрыва
        } else if (bits & BIT_DISCONNECTED) {
            ESP_LOGW(TAG, "disconnected, will retry");
        } else {
            ESP_LOGW(
                TAG, "no connect result within %lu ms",
                static_cast<unsigned long>(config.wifi.connect_timeout_ms));
        }

        if (!m_running) break;

        // Если подключены - просто ждём следующего события (обрыва)
        if (m_state == WifiState::CONNECTED) {
            continue;
        }

        // Политика повторов при обрыве или таймауте
        if (m_retry_count >= config.wifi.max_retries) {
            ESP_LOGE(TAG, "retry limit reached, cool-down %lu ms",
                     static_cast<unsigned long>(config.wifi.cooldown_ms));
            m_state = WifiState::FAILED;
            m_retry_count = 0;
            vTaskDelay(pdMS_TO_TICKS(config.wifi.cooldown_ms));
        } else {
            ++m_retry_count;
            m_state = WifiState::CONNECTING;
            ESP_LOGI(
                TAG, "retry %lu/%lu in %lu ms",
                static_cast<unsigned long>(m_retry_count),
                static_cast<unsigned long>(config.wifi.max_retries),
                static_cast<unsigned long>(config.wifi.reconnect_timeout_ms));
            vTaskDelay(pdMS_TO_TICKS(config.wifi.reconnect_timeout_ms));
        }

        esp_err_t ret = esp_wifi_connect();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_wifi_connect: %s", esp_err_to_name(ret));
        }
    }

    vTaskDelete(nullptr);
}

void WifiService::publishEvent(EventType type) {
    Event event{};
    event.type = type;
    event.timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);

    if (!EventBus::publish(event, getConfig().debug.wait_period_ms)) {
        ESP_LOGW(TAG, "failed to publish %s", eventTypeToString(type));
    }
}

}  // namespace app
