#include "app_events.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

ESP_EVENT_DEFINE_BASE(APP_EVENTS);

static const char *TAG = "APP_EVENTS";

namespace app {
namespace events {

bool publish(int32_t event_id, const void *data, size_t data_size) {
    if (data_size > 0 && data == nullptr) {
        ESP_LOGE(TAG, "publish: data is nullptr while data_size > 0");
        return false;
    }

    const esp_err_t err = esp_event_post(APP_EVENTS, event_id, data, data_size,
                                         pdMS_TO_TICKS(10));

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_event_post failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

const char *idToString(int32_t event_id) {
    switch (event_id) {
        case SYSTEM_BOOT:
            return "SYSTEM_BOOT";

        case HEARTBEAT:
            return "HEARTBEAT";

        case TELEMETRY_UPDATED:
            return "TELEMETRY_UPDATED";

        case ALERT_ACTIVE:
            return "ALERT_ACTIVE";

        case ALERT_CLEARED:
            return "ALERT_CLEARED";

        case MOTION_START:
            return "MOTION_START";

        case MOTION_END:
            return "MOTION_END";

        case WIFI_CONNECTED:
            return "WIFI_CONNECTED";

        case WIFI_DISCONNECTED:
            return "WIFI_DISCONNECTED";

        case MQTT_CONNECTED:
            return "MQTT_CONNECTED";

        case MQTT_DISCONNECTED:
            return "MQTT_DISCONNECTED";

        case OTA_STARTED:
            return "OTA_STARTED";

        case OTA_FINISHED:
            return "OTA_FINISHED";

        case ERROR_OCCURRED:
            return "ERROR_OCCURRED";

        default:
            return "UNKNOWN";
    }
}

}  // namespace events
}  // namespace app
