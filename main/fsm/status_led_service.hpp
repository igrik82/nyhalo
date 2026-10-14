#pragma once

#include "../drivers/rgb_led_driver.hpp"
#include "../events/event_types.hpp"
#include "esp_event_base.h"

namespace app {

// Сервис статусного светодиода подписан на обе шины:
// - системные события через esp_event (WiFi, MQTT)
// - прикладные события через свою шину (движение, тревоги)
class StatusLedService {
public:
    explicit StatusLedService(led_driver::RgbLedDriver &led);

    StatusLedService(const StatusLedService &) = delete;
    StatusLedService &operator=(const StatusLedService &) = delete;

    bool init();
    bool start();
    void stop();

private:
    // Обработчик прикладных событий (своя шина)
    static void appEventHandler(const Event &event, void *context);

    // Обработчик системных событий (esp_event)
    static void systemEventHandler(void *arg, esp_event_base_t base, int32_t id,
                                   void *data);

    void onMotionDetected();
    void onMotionCleared();
    void onAlertActivated();
    void onAlertCleared();
    void onWifiConnected();
    void onWifiDisconnected();
    void onMqttConnected();
    void onMqttDisconnected();
    void onSystemBoot();

    led_driver::RgbLedDriver &led_;
    bool initialized_ = false;
};

}  // namespace app
