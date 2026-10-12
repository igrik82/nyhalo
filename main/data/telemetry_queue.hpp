#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace app {
namespace data {

struct TelemetrySample {
    float temp_c;
    float rh_pct;
    uint16_t eco2;
    uint16_t tvoc;
    uint8_t aqi;
};

class TelemetryQueue {
public:
    static bool init();

    static bool push(const TelemetrySample &sample, uint32_t timeout_ms);
    static bool pop(TelemetrySample &sample, uint32_t timeout_ms);

private:
    static QueueHandle_t queue_;
};

}  // namespace data
}  // namespace app
