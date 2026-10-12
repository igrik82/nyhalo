#include "led_driver.hpp"

#include "drivers/digital_io.hpp"
#include "esp_log.h"

static const char *TAG = "LED_DRIVER";

static app::io_driver::DigitalOutput s_led;
static bool s_active_high = true;
static bool s_state = false;

namespace app {
namespace led_driver {

bool init(const LedConfig &config) {
    app::io_driver::DigitalOutput::Config output_config;
    output_config.gpio = config.gpio;

    /*  Светодиод по умолчанию должен быть выключен.
        Если активный уровень HIGH:
            выключен = LOW
        Если активный уровень LOW:
            выключен = HIGH
    */
    output_config.initial_high = !config.active_high;

    if (!s_led.init(output_config)) {
        ESP_LOGE(TAG, "led driver init failed");
        return false;
    }

    s_active_high = config.active_high;
    s_state = false;

    ESP_LOGI(TAG, "led driver init");

    return true;
}

bool set(bool on) {
    /*
        Если активный уровень HIGH :
          on=true  -> физический уровень HIGH
          on=false -> физический уровень LOW

        Если активный уровень LOW:
          on=true  -> физический уровень LOW
          on=false -> физический уровень HIGH
    */
    const bool physical_level = s_active_high ? on : !on;

    if (!s_led.set(physical_level)) {
        ESP_LOGE(TAG, "led driver set failed");
        return false;
    }

    s_state = on;
    ESP_LOGD(TAG, "led driver set %s", on ? "on" : "off");
    return true;
}

bool on() { return set(true); }
bool off() { return set(false); }
bool toggle() { return set(!s_state); }
bool isOn() { return s_state; }

}  // namespace led_driver
}  // namespace app
