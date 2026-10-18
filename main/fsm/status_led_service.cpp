#include "status_led_service.hpp"

#include "../app_config.hpp"
#include "../events/event_bus.hpp"
#include "../events/event_dispatcher.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "STATUS_LED";

namespace app {

StatusLedService::StatusLedService(led_driver::RgbLedDriver &led) : led_(led) {}

bool StatusLedService::init() {
    if (initialized_) {
        return true;
    }

    ESP_LOGI(TAG, "status led service init");

    // Подписка на прикладные события через свою шину
    EventDispatcher::subscribe(EventType::SYSTEM_BOOT,
                               &StatusLedService::appEventHandler, this);
    EventDispatcher::subscribe(EventType::HEARTBEAT,
                               &StatusLedService::appEventHandler, this);
    // EventDispatcher::subscribe(EventType::MotionDetected,
    //                            &StatusLedService::appEventHandler, this);
    // EventDispatcher::subscribe(EventType::MotionCleared,
    //                            &StatusLedService::appEventHandler, this);
    // EventDispatcher::subscribe(EventType::AlertActivated,
    //                            &StatusLedService::appEventHandler, this);
    // EventDispatcher::subscribe(EventType::AlertCleared,
    //                            &StatusLedService::appEventHandler, this);

    // // Подписка на системные события через стандартный механизм
    // esp_err_t err =
    //     esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
    //                                &StatusLedService::systemEventHandler,
    //                                this);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "failed to register wifi handler: %s",
    //              esp_err_to_name(err));
    //     return false;
    // }

    // err =
    //     esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
    //                                &StatusLedService::systemEventHandler,
    //                                this);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "failed to register wifi handler: %s",
    //              esp_err_to_name(err));
    //     return false;
    // }

    // err =
    //     esp_event_handler_register(MQTT_EVENT, MQTT_EVENT_CONNECTED,
    //                                &StatusLedService::systemEventHandler,
    //                                this);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "failed to register mqtt handler: %s",
    //              esp_err_to_name(err));
    //     return false;
    // }

    // err =
    //     esp_event_handler_register(MQTT_EVENT, MQTT_EVENT_DISCONNECTED,
    //                                &StatusLedService::systemEventHandler,
    //                                this);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "failed to register mqtt handler: %s",
    //              esp_err_to_name(err));
    //     return false;
    // }

    initialized_ = true;
    return true;
}

bool StatusLedService::start() {
    if (!initialized_) {
        return false;
    }

    ESP_LOGI(TAG, "status led service start");

    QueueHandle_t m_cmd_queue =
        xQueueCreate(10, sizeof(led_driver::RgbLedCommand));
    // Начальное состояние: выключен
    led_.init(m_cmd_queue);
    led_.start();
    // led_.setColor(0, 0, 0);
    // led_.setMode(led_driver::Mode::OFF);

    return true;
}

void StatusLedService::stop() {
    // Отписка от обработчиков (если нужно)
}

void StatusLedService::appEventHandler(const Event &event, void *context) {
    StatusLedService *self = static_cast<StatusLedService *>(context);

    switch (event.type) {
        case EventType::SYSTEM_BOOT:
            self->onSystemBoot();
            break;
        case EventType::HEARTBEAT:
            self->onHeartbeat();
            break;
        case EventType::MOTION_DETECTED:
            self->onMotionDetected();
            break;
        case EventType::MOTION_CLEARED:
            self->onMotionCleared();
            break;
        case EventType::ALERT_ACTIVATED:
            self->onAlertActivated();
            break;
        case EventType::ALERT_CLEARED:
            self->onAlertCleared();
            break;
        default:
            break;
    }
}

void StatusLedService::systemEventHandler(void *arg, esp_event_base_t base,
                                          int32_t id, void *data) {
    // StatusLedService *self = static_cast<StatusLedService *>(arg);

    // if (base == WIFI_EVENT) {
    //     if (id == WIFI_EVENT_STA_CONNECTED) {
    //         self->onWifiConnected();
    //     } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
    //         self->onWifiDisconnected();
    //     }
    // } else if (base == MQTT_EVENT) {
    //     if (id == MQTT_EVENT_CONNECTED) {
    //         self->onMqttConnected();
    //     } else if (id == MQTT_EVENT_DISCONNECTED) {
    //         self->onMqttDisconnected();
    //     }
    // }
}

void StatusLedService::onSystemBoot() {
    ESP_LOGD(TAG, "system boot: blue pulse");
    led_.sendCommand(led_driver::Mode::SOLID, 20, 170, 230, 10000, true);
    // led_.setColor(20, 170, 230);                        // Light blue
    // led_.setMode(led_driver::Mode::SOLID, 1000, true);  // urgent true
}

void StatusLedService::onHeartbeat() {
    ESP_LOGD(TAG, "system boot: heartbeat pulse");
    led_.sendCommand(led_driver::Mode::FADE_DOWN, 0, 255, 255, 1000, false);
    // led_.setColor(0, 255, 255);  // Синий
    // led_.setMode(led_driver::Mode::FADE_DOWN, 1000, false);
}
void StatusLedService::onMotionDetected() {
    ESP_LOGI(TAG, "motion detected: green solid");
    led_.setColor(0, 255, 0);  // Зелёный
    led_.setMode(led_driver::Mode::SOLID);
}

void StatusLedService::onMotionCleared() {
    ESP_LOGI(TAG, "motion cleared: off");
    led_.setColor(0, 0, 0);
    led_.setMode(led_driver::Mode::OFF);
}

void StatusLedService::onAlertActivated() {
    ESP_LOGW(TAG, "alert activated: red blink");
    led_.setColor(255, 0, 0);  // Красный
                               // led_.setMode(led_driver::Mode::BLINK, 500);
}

void StatusLedService::onAlertCleared() {
    ESP_LOGI(TAG, "alert cleared: off");
    led_.setColor(0, 0, 0);
    led_.setMode(led_driver::Mode::OFF);
}

void StatusLedService::onWifiConnected() {
    ESP_LOGI(TAG, "wifi connected: cyan solid");
    led_.setColor(0, 255, 255);  // Циан
    led_.setMode(led_driver::Mode::SOLID);
}

void StatusLedService::onWifiDisconnected() {
    ESP_LOGW(TAG, "wifi disconnected: yellow blink");
    led_.setColor(255, 255, 0);  // Жёлтый
                                 // led_.setMode(led_driver::Mode::BLINK, 1000);
}

void StatusLedService::onMqttConnected() {
    ESP_LOGI(TAG, "mqtt connected: green pulse");
    led_.setColor(0, 255, 0);  // Зелёный
                               // led_.setMode(led_driver::Mode::PULSE, 3000);
}

void StatusLedService::onMqttDisconnected() {
    ESP_LOGW(TAG, "mqtt disconnected: orange blink");
    led_.setColor(255, 165, 0);  // Оранжевый
                                 // led_.setMode(led_driver::Mode::BLINK, 800);
}

}  // namespace app
