#pragma once

#include "freertos/idf_additions.h"
namespace app {

// Задача диспетчера событий
class EventDispatcherTask {
public:
    static bool init();
    static bool start();
    static void stop();

private:
    static void taskEntry(void *arg);
    static void taskLoop();

    static TaskHandle_t task_handle_;
};

}  // namespace app
