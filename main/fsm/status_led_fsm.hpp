#pragma once

#include <cstdint>

namespace app {
namespace fsm {

class StatusLedFsm {
public:
    enum class State { Normal, Alert, Error, Offline };

    bool init();

    void handleEvent(int32_t event_id, const void *event_data);

    State state() const { return state_; }

private:
    void enterNormal();
    void enterAlert();
    void enterError();

    State state_ = State::Normal;
};

}  // namespace fsm
}  // namespace app
