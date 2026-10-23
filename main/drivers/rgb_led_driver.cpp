#include "rgb_led_driver.hpp"

#include <driver/rmt_tx.h>

#include <cstdint>

#include "../app_config.hpp"
#include "esp_log.h"
#include "freertos/projdefs.h"

namespace app::led_driver {

static const char *TAG = "RGB_LED_DRIVER";

RgbLedDriver::RgbLedDriver(gpio_num_t gpio, uint8_t strip_len)
    : m_gpio(gpio), m_strip_len(strip_len) {}

RgbLedDriver::~RgbLedDriver() {
    stop();
    if (m_strip) {
        led_strip_del(m_strip);  // компонент сам освободит и свой RMT-канал
        m_strip = nullptr;
    }
}

bool RgbLedDriver::init(QueueHandle_t cmd_queue) {
    m_cmd_queue = cmd_queue;
    if (m_initialized) return true;

    if (m_cmd_queue == nullptr) {
        ESP_LOGE(TAG, "Queue is null");
        return false;
    }

    // используется union led_color_component_format_t + макросы.
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = m_gpio;
    ESP_LOGD(TAG, "%u %u", m_gpio, m_strip_len);
    strip_config.max_leds = static_cast<uint32_t>(m_strip_len);
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz = 10 * 1000 * 1000;  // 10 MHz
    rmt_config.mem_block_symbols = 0;             // default
    rmt_config.flags.with_dma = false;

    const esp_err_t err =
        led_strip_new_rmt_device(&strip_config, &rmt_config, &m_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "driver initialized");
    led_strip_clear(m_strip);
    m_initialized = true;
    return true;
}

bool RgbLedDriver::start() {
    if (!m_initialized || m_running.load()) return false;

    m_running.store(true);
    if (xTaskCreate(task_entry, "rgb_led", 3072, this, 5, &m_task_handle) !=
        pdPASS) {
        m_running.store(false);
        ESP_LOGE(TAG, "xTaskCreate failed");
        return false;
    }

    ESP_LOGI(TAG, "driver started");
    return true;
}

void RgbLedDriver::stop() {
    if (m_running.exchange(false)) {
        // Отправляем команду OFF с urgent, чтобы разбудить задачу
        sendCommand(Mode::OFF, 0, 0, 0, 0, true);

        if (m_task_handle) {
            vTaskDelay(pdMS_TO_TICKS(100));  // Дать команде время завершиться
            m_task_handle = nullptr;
        }
    }
}

void RgbLedDriver::sendCommand(Mode mode, uint8_t r, uint8_t g, uint8_t b,
                               uint32_t period_ms, bool urgent) {
    if (!m_cmd_queue) return;

    RgbLedCommand cmd = {mode, r, g, b, period_ms, urgent};

    if (urgent) {
        // Для urgent команды вставляем в начало очереди
        xQueueSendToFront(m_cmd_queue, &cmd, 0);
    } else {
        // Не блокируемся если очередь полна
        xQueueSendToBack(m_cmd_queue, &cmd, 0);
    };
}

// void RgbLedDriver::setColor(uint8_t r, uint8_t g, uint8_t b) {
//     sendCommand(Mode::SOLID, r, g, b, 1000, false);
// }

// void RgbLedDriver::setMode(Mode mode, uint32_t period_ms, bool urgent) {
//     sendCommand(mode, 0, 0, 0, period_ms, urgent);
// }

void RgbLedDriver::update_strip(uint8_t r, uint8_t g, uint8_t b) {
    const uint32_t packed =
        (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
    if (packed == m_current_color)
        return;  // не дёргаем шину RMT без необходимости
    m_current_color = packed;

    if (packed == 0) {
        led_strip_clear(m_strip);
    } else {
        led_strip_set_pixel(m_strip, 0, r, g, b);
        led_strip_refresh(m_strip);
    }
}

void RgbLedDriver::execute_command(const RgbLedCommand &cmd) {
    const uint8_t tr = cmd.r;
    const uint8_t tg = cmd.g;
    const uint8_t tb = cmd.b;
    const uint32_t period_ms = cmd.period_ms;

    switch (cmd.mode) {
        case Mode::OFF:
            update_strip(0, 0, 0);
            break;

        case Mode::SOLID:
            update_strip(tr, tg, tb);
            vTaskDelay(pdMS_TO_TICKS(period_ms));
            update_strip(0, 0, 0);
            break;

        case Mode::FADE_DOWN:
            for (int i = 255; i >= 0; i -= FADE_STEPS) {
                if (!m_running.load(std::memory_order_acquire)) break;

                const uint8_t r = (tr * i) / 255;
                const uint8_t g = (tg * i) / 255;
                const uint8_t b = (tb * i) / 255;

                update_strip(r, g, b);
                vTaskDelay(pdMS_TO_TICKS(FADE_DELAY_MS));
            }

            update_strip(0, 0, 0);
            break;

        case Mode::FADE_UP:
            for (int i = 0; i <= 255; i += FADE_STEPS) {
                if (!m_running.load(std::memory_order_acquire)) break;

                const uint8_t r = (tr * i) / 255;
                const uint8_t g = (tg * i) / 255;
                const uint8_t b = (tb * i) / 255;

                update_strip(r, g, b);
                vTaskDelay(pdMS_TO_TICKS(FADE_DELAY_MS));
            }

            update_strip(0, 0, 0);
            break;

        case Mode::BLINK_PERIODIC:
            break;
    }
}

void RgbLedDriver::task_entry(void *arg) {
    static_cast<RgbLedDriver *>(arg)->task_loop();
}

void RgbLedDriver::task_loop() {
    RgbLedCommand cmd;

    while (m_running.load(std::memory_order_acquire)) {
        if (xQueueReceive(m_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            execute_command(cmd);
        }
    }

    // Финальная очистка
    update_strip(0, 0, 0);
    vTaskDelete(nullptr);
}

}  // namespace app::led_driver
