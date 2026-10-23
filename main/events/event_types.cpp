#include "events/event_types.hpp"

namespace app {

const char *eventTypeToString(EventType type) {
    switch (type) {
        case EventType::SYSTEM_BOOT:
            return "SystemBoot";
        case EventType::HEARTBEAT:
            return "Heartbeat";
        case EventType::ERROR_OCCURRED:
            return "ErrorOccurred";

        case EventType::WIFI_CONNECTED:
            return "WifiConnected";
        case EventType::WIFI_DISCONNECTED:
            return "WifiDisconnected";
        case EventType::MQTT_CONNECTED:
            return "MqttConnected";
        case EventType::MQTT_DISCONNECTED:
            return "MqttDisconnected";

        case EventType::MOTION:
            return "TriggeredMotionSensor";
        case EventType::MOTION_DETECTED:
            return "MotionDetected";
        case EventType::MOTION_CLEARED:
            return "MotionCleared";
        case EventType::TELEMETRY_UPDATED:
            return "TelemetryUpdated";

        case EventType::ALERT_ACTIVATED:
            return "AlertActivated";
        case EventType::ALERT_CLEARED:
            return "AlertCleared";

        case EventType::OTA_STARTED:
            return "OtaStarted";
        case EventType::OTA_COMPLETED:
            return "OtaCompleted";
        case EventType::OTA_FAILED:
            return "OtaFailed";

        default:
            return "Unknown";
    }
}

}  // namespace app
