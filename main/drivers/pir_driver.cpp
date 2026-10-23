#include "pir_driver.hpp"

#include "drivers/digital_io.hpp"
#include "esp_log.h"
#include "esp_timer.h"

namespace app::drivers {

static const char *TAG = "PIR_DRIVER";

PirDriver::PirDriver(gpio_num_t gpio_num) : m_gpio_pin(gpio_num) {}

// bool PirDriver::init(QueueHandle_t raw_data_queue) {
//     if (m_initialized) return true;

//     if (raw_data_queue == nullptr) {
//         ESP_LOGE(TAG, "raw data queue is null");
//         return false;
//     }

//     io_driver::DigitalInputISR input_driver;
//     input_driver.init(pir_isr_handler, m_gpio_pin);

//     m_queue_handle = raw_data_queue;
//     m_initialized = true;
//     ESP_LOGI(TAG, "driver initialized");
//     return true;
// }
bool PirDriver::init(QueueHandle_t raw_data_queue) {
    if (m_initialized) return true;
    if (raw_data_queue == nullptr) {
        ESP_LOGE(TAG, "raw data queue is null");
        return false;
    }

    // 1. Настройка GPIO
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << static_cast<int>(m_gpio_pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    if (gpio_config(&io_conf) != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed");
        return false;
    }

    // 2. Установка ISR-сервиса (один раз на всё приложение!)
    static bool isr_service_installed = false;
    if (!isr_service_installed) {
        gpio_install_isr_service(0);
        isr_service_installed = true;
    }

    // 3. Регистрация обработчика с ПРАВИЛЬНЫМ указателем (this)
    if (gpio_isr_handler_add(m_gpio_pin, pir_isr_handler, this) != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed");
        return false;
    }

    m_queue_handle = raw_data_queue;
    m_initialized = true;
    ESP_LOGI(TAG, "driver initialized");
    return true;
}

void PirDriver::start() {
    if (!m_initialized) {
        ESP_LOGE(TAG, "driver not initialized");
        return;
    }

    ESP_LOGI(TAG, "driver started");
}

void PirDriver::stop() {
    if (!m_initialized) return;
    gpio_isr_handler_remove(m_gpio_pin);
}

void IRAM_ATTR PirDriver::pir_isr_handler(void *arg) {
    PirDriver *self = static_cast<PirDriver *>(arg);
    if (self == nullptr || self->m_queue_handle == nullptr) return;

    RawPirData data;
    data.active = (gpio_get_level(self->m_gpio_pin) == 1);
    data.timestamp_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(self->m_queue_handle, &data, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

}  // namespace app::drivers
