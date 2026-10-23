#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>

#include "soc/gpio_num.h"

namespace app {

// Сырые данные, передаваемые из ISR в задачу сервиса
struct RawPirData {
    bool active;
    uint32_t timestamp_ms;
};

namespace drivers {

class PirDriver {
public:
    explicit PirDriver(gpio_num_t gpio_num);
    // Очередь не удаляем так как очередь не принадлежит драйверу
    ~PirDriver() = default;

    PirDriver(const PirDriver &) = delete;
    PirDriver &operator=(const PirDriver &) = delete;

    bool init(QueueHandle_t raw_data_queue);
    void start();
    void stop();

private:
    static void IRAM_ATTR pir_isr_handler(void *arg);

    gpio_num_t m_gpio_pin;
    QueueHandle_t m_queue_handle = nullptr;
    bool m_initialized = false;
};

}  // namespace drivers
}  // namespace app
