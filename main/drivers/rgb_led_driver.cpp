#include "drivers/rgb_led_driver.hpp"

#include "esp_log.h"
#include "led_strip.h"

constexpr const char *TAG = "RGB_LED_DRIVER";

namespace app {
namespace rgb_led_driver {

RgbLedDriver::RgbLedDriver(gpio_num_t gpio, uint8_t led_count)
    : m_strip(nullptr),
      m_task_handle(nullptr),
      m_mode(Mode::OFF),
      m_color(0),
      m_blink_period_ticks(
          pdMS_TO_TICKS(app::getConfig().rgb_led.blink_period_ms)),
      m_blink_state(false) {
    // Конфигурация светодиодной ленты
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = led_count,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }};

    // Конфигурация RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10 МГц
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        }};
    esp_err_t err =
        led_strip_new_rmt_device(&strip_config, &rmt_config, &m_strip);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s",
                 esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "driver initialized");

    led_strip_clear(m_strip);
    led_strip_refresh(m_strip);

    xTaskCreate(task_entry, "ws2812b_task", 4096, this, 5, &m_task_handle);
};

RgbLedDriver::~RgbLedDriver() {
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }

    if (m_strip != nullptr) {
        led_strip_clear(m_strip);
        led_strip_refresh(m_strip);
        const esp_err_t err = led_strip_del(m_strip);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "led_strip_del failed: %s", esp_err_to_name(err));
        }
        m_strip = nullptr;
        ESP_LOGI(TAG, "driver deleted");
    }
}

void RgbLedDriver::setColor(uint32_t color, Mode mode,
                            uint32_t blink_period_ms) {
    m_color = color;
    m_mode = mode;
    m_blink_state = false;  // сброс фазы мигания

    if (mode == Mode::BLINK && blink_period_ms > 0) {
        m_blink_period_ticks = pdMS_TO_TICKS(blink_period_ms);
    } else {
        m_blink_period_ticks = pdMS_TO_TICKS(
            app::getConfig().rgb_led.blink_period_ms);  // значение по умолчанию
    }

    // Сразу применить состояние для быстрой реакции
    if (mode == Mode::OFF) {
        update_hardware(false);
    } else if (mode == Mode::SOLID) {
        update_hardware(true);
    }
}

void RgbLedDriver::off() {
    {
        setColor(0, RgbLedDriver::Mode::OFF);
    }
}

void RgbLedDriver::task_entry(void *arg) {
    static_cast<RgbLedDriver *>(arg)->task_loop();
}

void RgbLedDriver::task_loop() {
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        if (m_mode == Mode::BLINK) {
            m_blink_state = !m_blink_state;
            update_hardware(m_blink_state);
            vTaskDelayUntil(&last_wake, m_blink_period_ticks);
        } else {
            // В остальных режимах просто ждём, ничего не делаем
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
        }
    }
}

void RgbLedDriver::update_hardware(bool on) {
    if (!m_strip) return;

    if (!on) {
        led_strip_clear(m_strip);
    } else {
        // Извлекаем компоненты RGB
        uint8_t r = (m_color >> 16) & 0xFF;
        uint8_t g = (m_color >> 8) & 0xFF;
        uint8_t b = m_color & 0xFF;

        // Устанавливаем все светодиоды (упрощённо — один)
        // Для управления несколькими разными цветами нужно изменить логику
        uint16_t count = 1;
        for (uint16_t i = 0; i < count; i++) {
            led_strip_set_pixel(m_strip, i, r, g, b);
        }
    }
    led_strip_refresh(m_strip);
}

}  // namespace rgb_led_driver
}  // namespace app
