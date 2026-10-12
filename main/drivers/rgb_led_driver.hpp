#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>

#include "led_strip.h"

namespace app::led_driver {

enum class Mode : uint8_t {
    OFF,
    SOLID,  // Постоянное свечение
    BLINK,  // Жёсткое мигание вкл/выкл
    PULSE   // Плавное «дыхание»
};

class RgbLedDriver {
public:
    RgbLedDriver(int gpio, int strip_len);
    ~RgbLedDriver();

    RgbLedDriver(const RgbLedDriver &) = delete;
    RgbLedDriver &operator=(const RgbLedDriver &) = delete;

    bool init();   // только «железо»
    bool start();  // запуск задачи
    void stop();

    void setColor(uint8_t r, uint8_t g, uint8_t b);      // потокобезопасно
    void setMode(Mode mode, uint32_t period_ms = 1000);  // потокобезопасно

private:
    static void task_entry(void *arg);
    void task_loop();
    void update_strip(uint8_t r, uint8_t g, uint8_t b);

    int m_gpio;
    int m_strip_len;
    led_strip_handle_t m_strip =
        nullptr;  // RMT-канал больше не храним: им владеет компонент
    TaskHandle_t m_task_handle = nullptr;
    bool m_initialized = false;
    std::atomic<bool> m_running{false};

    std::atomic<Mode> m_mode{Mode::OFF};
    std::atomic<uint32_t> m_period_ms{1000};
    std::atomic<uint32_t> m_target_color{0};  // упаковка (R<<16)|(G<<8)|B

    uint32_t m_current_color = 0xFFFFFFFF;  // используется только внутри задачи
};

}  // namespace app::led_driver
