#include "status_led_service.hpp"

#include "../app_events.hpp"
#include "../board.hpp"
#include "../drivers/led_driver.hpp"
#include "../fsm/status_led_fsm.hpp"
#include "esp_event.h"
#include "esp_log.h"

static const char *TAG = "STATUS_LED_SERVICE";

static app::fsm::StatusLedFsm s_status_led_fsm;

extern "C" void status_led_event_handler(void *handler_arg,
                                         esp_event_base_t event_base,
                                         int32_t event_id, void *event_data);

namespace app {
namespace status_led {

bool init() {
    ESP_LOGI(TAG, "status led service init");

    if (!led_driver::init(board::BLINK_LED)) {
        ESP_LOGE(TAG, "failed to init LED driver");
        return false;
    }

    if (!s_status_led_fsm.init()) {
        ESP_LOGE(TAG, "failed to init status LED FSM");
        return false;
    }

    const esp_err_t err = esp_event_handler_register(
        APP_EVENTS, ESP_EVENT_ANY_ID, &status_led_event_handler, nullptr);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to register event handler: %s",
                 esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "status led service initialized");
    return true;
}

void start() { ESP_LOGI(TAG, "status led service started"); }

}  // namespace status_led
}  // namespace app

extern "C" void status_led_event_handler(void *handler_arg,
                                         esp_event_base_t event_base,
                                         int32_t event_id, void *event_data) {
    if (event_base != APP_EVENTS) {
        return;
    }

    s_status_led_fsm.handleEvent(event_id, event_data);
}
