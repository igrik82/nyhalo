#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>

#include "app_config.hpp"
#include "board.hpp"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "led_strip.h"

namespace app::led_driver {

enum class Mode : uint8_t {
    OFF,
    SOLID,      // Постоянное свечение
    FADE_DOWN,  // Вспышка и плавное затухание
    FADE_UP,    // Плавное увеличение яркости и выключение
};

struct RgbLedCommand {
    Mode mode;
    uint8_t r, g, b;
    uint32_t period_ms;
    bool urgent;  // Если true, то команда вставляется в начало очереди
};

class RgbLedDriver {
public:
    RgbLedDriver(gpio_num_t gpio, uint8_t led_count);
    ~RgbLedDriver();

    RgbLedDriver(const RgbLedDriver &) = delete;
    RgbLedDriver &operator=(const RgbLedDriver &) = delete;

    bool init(QueueHandle_t cmd_queue);  // только «железо»
    bool start();                        // запуск задачи
    void stop();

    // Методы для отправки команд (потокобезопасно)
    void sendCommand(Mode mode, uint8_t r, uint8_t g, uint8_t b,
                     uint32_t period_ms = 1000, bool urgent = false);

    void setColor(uint8_t r, uint8_t g, uint8_t b);  // потокобезопасно
    void setMode(Mode mode, uint32_t period_ms = 1000,
                 bool urgent = false);  // потокобезопасно

private:
    static void task_entry(void *arg);
    void task_loop();
    void update_strip(uint8_t r, uint8_t g, uint8_t b);
    void execute_command(const RgbLedCommand &cmd);

    const Config &config = getConfig();
    // Константы
    const uint8_t FADE_STEPS = config.rgb_led.fade_step;
    const uint32_t FADE_DELAY_MS =
        pdMS_TO_TICKS(config.rgb_led.fade_delay_ticks);

    QueueHandle_t m_cmd_queue = nullptr;
    int m_gpio = static_cast<int>(board::RGB_LED);
    uint32_t m_strip_len = static_cast<uint32_t>(config.rgb_led.led_count);
    led_strip_handle_t m_strip =
        nullptr;  // RMT-канал больше не храним: им владеет компонент
    TaskHandle_t m_task_handle = nullptr;
    bool m_initialized = false;
    std::atomic<bool> m_running{false};

    // std::atomic<Mode> m_mode{Mode::OFF};
    // std::atomic<uint32_t> m_period_ms{1000};
    // std::atomic<uint32_t> m_target_color{0};  // упаковка (R<<16)|(G<<8)|B

    uint32_t m_current_color = 0xFFFFFFFF;  // используется только внутри задачи
};

}  // namespace app::led_driver
