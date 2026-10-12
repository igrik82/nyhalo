#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(APP_EVENTS);

namespace app {
namespace events {

enum Id : int32_t {
  SYSTEM_BOOT = 0,

  HEARTBEAT,

  TELEMETRY_UPDATED,

  ALERT_ACTIVE,
  ALERT_CLEARED,

  MOTION_START,
  MOTION_END,

  WIFI_CONNECTED,
  WIFI_DISCONNECTED,

  MQTT_CONNECTED,
  MQTT_DISCONNECTED,

  OTA_STARTED,
  OTA_FINISHED,

  ERROR_OCCURRED
};

struct HeartbeatData {
  uint32_t timestamp_ms;
  uint32_t count;
};

struct TelemetryData {
  uint32_t timestamp_ms;
  float temp_c;
  float rh_pct;
  uint16_t eco2;
  uint16_t tvoc;
  uint8_t aqi;
};

struct AlertData {
  uint32_t timestamp_ms;
  uint8_t level;
  uint16_t eco2;
};

struct MotionData {
  uint32_t timestamp_ms;
  uint32_t duration_ms;
  uint32_t count;
};

bool publish(int32_t event_id, const void *data, size_t data_size);
const char *idToString(int32_t event_id);

} // namespace events
} // namespace app
