#include "digital_io.hpp"

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "DIGITAL_IO";

namespace app {
namespace io_driver {

bool DigitalOutput::init(const Config &config) {
    gpio_config_t io_conf{};

    io_conf.pin_bit_mask = 1ULL << static_cast<int>(config.gpio);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    const esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return false;
    }

    _gpio = config.gpio;
    _initialized = true;
    _state = config.initial_high;
    return set(_state);
}

bool DigitalOutput::set(bool high) {
    if (!_initialized) {
        ESP_LOGE(TAG, "DigitalOutput not initialized");
        return false;
    }

    const esp_err_t err = gpio_set_level(_gpio, high ? 1 : 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_set_level failed: %s", esp_err_to_name(err));
        return false;
    }

    _state = high;
    return true;
}

bool DigitalOutput::toggle() { return set(!_state); }

bool DigitalInputISR::init(gpio_isr_t isr_handler, gpio_num_t gpio) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << static_cast<int>(gpio);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    const esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return false;
    }

    _gpio = gpio;
    _initialized = true;

    gpio_install_isr_service(0);
    const esp_err_t res = gpio_isr_handler_add(gpio, isr_handler, this);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed: %s", esp_err_to_name(res));
        return false;
    }
    ESP_LOGI(TAG, "DigitalInputISR initialized");

    return true;
}

}  // namespace io_driver
}  // namespace app
