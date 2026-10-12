#include "status_led_fsm.hpp"

#include "../app_events.hpp"
#include "../drivers/led_driver.hpp"
#include "esp_log.h"

static const char *TAG = "STATUS_LED_FSM";

namespace app {
namespace fsm {

bool StatusLedFsm::init() {
    ESP_LOGI(TAG, "status LED FSM init");

    state_ = State::Normal;
    led_driver::off();

    return true;
}

void StatusLedFsm::handleEvent(int32_t event_id, const void *event_data) {
    switch (event_id) {
        case events::HEARTBEAT: {
            if (state_ == State::Normal) {
                led_driver::toggle();
            }
            break;
        }

        case events::TELEMETRY_UPDATED: {
            // Здесь позже можно менять поведение,
            // например показывать качество воздуха.
            break;
        }

        case events::ALERT_ACTIVE: {
            if (state_ != State::Alert) {
                enterAlert();
            }
            break;
        }

        case events::ALERT_CLEARED: {
            if (state_ == State::Alert) {
                enterNormal();
            }
            break;
        }

        case events::ERROR_OCCURRED: {
            if (state_ != State::Error) {
                enterError();
            }
            break;
        }

        default:
            break;
    }
}

void StatusLedFsm::enterNormal() {
    ESP_LOGI(TAG, "state -> Normal");

    state_ = State::Normal;
    led_driver::off();
}

void StatusLedFsm::enterAlert() {
    ESP_LOGW(TAG, "state -> Alert");

    state_ = State::Alert;

    // Пока просто включим светодиод.
    // Позже здесь можно сделать быстрое мигание по таймеру.
    led_driver::on();
}

void StatusLedFsm::enterError() {
    ESP_LOGE(TAG, "state -> Error");

    state_ = State::Error;

    // Пока просто включим светодиод.
    // Позже можно сделать отдельный аварийный паттерн.
    led_driver::on();
}

}  // namespace fsm
}  // namespace app
