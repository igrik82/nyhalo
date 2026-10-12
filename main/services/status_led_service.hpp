#pragma once

#include "fsm/status_led_fsm.hpp"

namespace app {
namespace status_led {

bool init();
void start();

bool setState(fsm::StatusLedFsm::State state);

}  // namespace status_led
}  // namespace app
