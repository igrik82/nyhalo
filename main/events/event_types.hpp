#pragma once

#include <cstdint>

namespace app {

// Компактные события: тип + временная метка + полезные данные
enum class EventType : uint16_t {
    // Системные
    SYSTEM_BOOT = 0,
    HEARTBEAT,
    ERROR_OCCURRED,

    // Сеть и MQTT
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    MQTT_CONNECTED,
    MQTT_DISCONNECTED,

    // Датчики и телеметрия
    MOTION,
    MOTION_DETECTED,
    MOTION_CLEARED,
    TELEMETRY_UPDATED,

    // Тревога
    ALERT_ACTIVATED,
    ALERT_CLEARED,

    // NTP сервис
    TIME_SYNCED,
    TIME_SYNC_FAILED,

    // OTA
    OTA_STARTED,
    OTA_COMPLETED,
    OTA_FAILED,
};

// Преобразование типа события в его строковое представление
const char *eventTypeToString(EventType event_type);

// Временная метка в миллисекундах от старта системы
using TimestampMs = uint32_t;

// Универсальная структура события
struct Event {
    EventType type;
    TimestampMs timestamp;

    // Объединение для компактного хранения
    union {
        // События движения
        struct MotionEventData {
            uint32_t duration_ms;
            uint32_t count;
        } motion;

        // Телеметрия
        struct TelemetryEventData {
            float temp_c;
            float rh_pct;
            uint16_t eco2;
            uint16_t tvoc;
            uint8_t aqi;
        } telemetry;

        // Тревога
        struct AlertEventData {
            uint8_t level;
            uint16_t eco2;
        } alert;

        struct TimeEventData {
            uint64_t timestamp_s;  // Unix-время синхронизации
        } time;

        // ...

        // для heartbeat и других счётчиков
        uint32_t count;
    } data;
};

}  // namespace app
