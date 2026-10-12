#include "rgb_led_driver.hpp"

#include <driver/rmt_tx.h>

#include <cmath>

#include "esp_log.h"

namespace app::led_driver {
namespace {

constexpr uint32_t kTickMs = 10;  // шаг анимации, мс

// Гамма-таблица (~gamma 2.0), генерируется КОМПИЛЯТОРОМ: ровно 256 элементов,
// лежит во flash, в рантайме не считается.
struct GammaTable {
    uint8_t v[256];
    constexpr GammaTable() : v{} {
        for (int i = 0; i < 256; ++i) {
            const uint32_t g = (i * i + 127) / 255;
            v[i] = (g < 2) ? 0
                           : static_cast<uint8_t>(
                                 g);  // режем 1..2 — это видимое глазом «эхо»
        }
    }
};
constexpr GammaTable kGamma;

}  // namespace

static const char *TAG = "RGB_LED_DRIVER";

RgbLedDriver::RgbLedDriver(int gpio, int strip_len)
    : m_gpio(gpio), m_strip_len(strip_len) {}

RgbLedDriver::~RgbLedDriver() {
    stop();
    if (m_strip) {
        led_strip_del(m_strip);  // компонент сам освободит и свой RMT-канал
        m_strip = nullptr;
    }
}

bool RgbLedDriver::init() {
    if (m_initialized) return true;

    // led_strip >= 3.0: формата LED_PIXEL_FORMAT_GRB больше нет,
    // используется union led_color_component_format_t + макросы.
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = m_gpio;
    strip_config.max_leds = static_cast<uint32_t>(m_strip_len);
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;

    // RMT-канал создаётся ВНУТРИ led_strip_new_rmt_device(),
    // rmt_new_tx_channel() вызывать вручную НЕ нужно.
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
    if (xTaskCreate(task_entry, "rgb_led", 2048, this, 5, &m_task_handle) !=
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
        if (m_task_handle) {
            vTaskDelete(m_task_handle);
            m_task_handle = nullptr;
        }
        if (m_strip)
            led_strip_clear(m_strip);  // гасим защёлку после остановки задачи
        m_current_color = 0;
    }
}

void RgbLedDriver::setColor(uint8_t r, uint8_t g, uint8_t b) {
    m_target_color.store(
        (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b,
        std::memory_order_release);
}

void RgbLedDriver::setMode(Mode mode, uint32_t period_ms) {
    if (period_ms > 0) m_period_ms.store(period_ms, std::memory_order_release);
    m_mode.store(mode, std::memory_order_release);
}

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

void RgbLedDriver::task_entry(void *arg) {
    static_cast<RgbLedDriver *>(arg)->task_loop();
}

void RgbLedDriver::task_loop() {
    constexpr float kPi = 3.14159265359f;
    TickType_t last_wake = xTaskGetTickCount();
    Mode prev_mode = m_mode.load(std::memory_order_acquire);
    uint32_t phase_ms = 0;

    while (m_running.load(std::memory_order_acquire)) {
        const Mode mode = m_mode.load(std::memory_order_acquire);
        const uint32_t period = m_period_ms.load(std::memory_order_acquire);
        const uint32_t packed = m_target_color.load(std::memory_order_acquire);
        const uint8_t tr = static_cast<uint8_t>(packed >> 16);
        const uint8_t tg = static_cast<uint8_t>(packed >> 8);
        const uint8_t tb = static_cast<uint8_t>(packed);

        if (mode != prev_mode) {
            prev_mode = mode;
            phase_ms = 0;  // новая анимация начинается с нуля
        }

        switch (mode) {
            case Mode::OFF:
                update_strip(0, 0, 0);
                break;

            case Mode::SOLID:
                update_strip(tr, tg, tb);
                break;

            case Mode::BLINK: {
                phase_ms += kTickMs;
                if (phase_ms >= period) phase_ms -= period;
                if (phase_ms < period / 2)
                    update_strip(tr, tg, tb);
                else
                    update_strip(0, 0, 0);
                break;
            }

            case Mode::PULSE: {
                phase_ms += kTickMs;
                if (phase_ms >= period) phase_ms -= period;

                // 75% периода — «вдох-выдох» (полусинус 0..1..0), 25% —
                // гарантированная пауза в полной темноте
                const uint32_t breath_ms = period - period / 4;
                if (phase_ms < breath_ms) {
                    const float s =
                        sinf(kPi * static_cast<float>(phase_ms) /
                             static_cast<float>(
                                 breath_ms));  // 0..1..0, без смещения
                    const uint8_t brightness =
                        kGamma.v[static_cast<uint8_t>(s * 255.0f)];
                    const uint8_t r =
                        static_cast<uint8_t>((tr * brightness + 127) / 255);
                    const uint8_t g =
                        static_cast<uint8_t>((tg * brightness + 127) / 255);
                    const uint8_t b =
                        static_cast<uint8_t>((tb * brightness + 127) / 255);
                    update_strip(r, g, b);
                } else {
                    update_strip(0, 0, 0);  // явная полная темнота
                }
                break;
            }
        }

        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kTickMs));
    }
    vTaskDelete(nullptr);
}

}  // namespace app::led_driver
