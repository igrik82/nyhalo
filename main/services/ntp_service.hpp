#pragma once

#include <sys/time.h>
#include <time.h>

#include "events/event_bus.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace app {

// Сервис синхронизации времени через NTP.
// Подписывается на WIFI_CONNECTED / WIFI_DISCONNECTED,
// публикует TIME_SYNCED при успешной синхронизации.
class NtpService {
public:
    NtpService();

    NtpService(const NtpService &) = delete;
    NtpService &operator=(const NtpService &) = delete;

    // Двухфазная инициализация
    bool init();
    bool start();
    void stop();

    // Публичный API для других сервисов
    bool isTimeValid() const { return time_synced_; }
    time_t getTime() const;
    void getLocalTime(struct tm *out_tm) const;

private:
    // Статический трамплин для FreeRTOS задачи
    static void taskEntry(void *arg);
    void taskLoop();

    // Обработчики событий — вызываются диспетчером через статический трамплин
    static void eventHandler(const Event &event, void *context);

    // Обработчик события SNTP — вызывается из колбэка esp_sntp
    static void sntpNotificationCallback(struct timeval *tv);

    // Внутренние методы
    void handleWifiConnected();
    void handleWifiDisconnected();
    void handleSntpNotification(struct timeval *tv);
    void startSntp();
    void stopSntp();
    void publishTimeSyncedEvent(time_t timestamp);

    TaskHandle_t task_handle_ = nullptr;
    bool initialized_ = false;
    bool running_ = false;

    // Кэшированные параметры (заполняются в init)
    char ntp_server_[64] = {0};
    char timezone_[32] = {0};
    uint32_t sync_check_period_ms_ = 0;
    uint32_t sync_timeout_ms_ = 0;

    // Состояние
    volatile bool wifi_connected_ = false;
    volatile bool sntp_started_ = false;
    volatile bool time_synced_ = false;
};

}  // namespace app
