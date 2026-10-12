#pragma once

#include "app_config.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

namespace app {
namespace rgb_led_driver {

class RgbLedDriver {
public:
    enum class Mode {
        OFF,    // Выключен
        SOLID,  // Статичный цвет
        BLINK   // Мигание
    };

    RgbLedDriver(gpio_num_t gpio, uint8_t led_count = 1);
    ~RgbLedDriver();

    bool init();

    // Основной метод: задать цвет и режим
    // color: формат 0xRRGGBB
    // blink_period_ms: при мигании — длительность фазы «вкл» и «выкл» в мс
    void setColor(
        uint32_t color, Mode mode = Mode::SOLID,
        uint32_t blink_period_ms = app::getConfig().rgb_led.blink_period_ms);
    void off();

private:
    static void task_entry(void *arg);
    void task_loop();
    void update_hardware(bool on);

    led_strip_handle_t m_strip;
    TaskHandle_t m_task_handle;
    Mode m_mode;
    uint32_t m_color;
    uint32_t m_blink_period_ticks;
    bool m_blink_state;
};

}  // namespace rgb_led_driver
}  // namespace app
