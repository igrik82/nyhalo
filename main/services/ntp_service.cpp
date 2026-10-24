#include "ntp_service.hpp"

#include <cstring>

#include "../app_config.hpp"
#include "esp_log.h"
// #include "esp_sntp.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "events/event_dispatcher.hpp"

static const char *TAG = "NTP_SERVICE";

namespace app {

// Глобальный указатель на единственный экземпляр — нужен для колбэка SNTP,
// который вызывается из C API без передачи контекста
static NtpService *s_instance = nullptr;

NtpService::NtpService() {}

bool NtpService::init() {
    if (initialized_) return true;

    const Config &config = getConfig();

    // Кэшируем параметры
    strncpy(ntp_server_, config.ntp.server, sizeof(ntp_server_) - 1);
    ntp_server_[sizeof(ntp_server_) - 1] = '\0';

    strncpy(timezone_, config.ntp.timezone, sizeof(timezone_) - 1);
    timezone_[sizeof(timezone_) - 1] = '\0';

    sync_check_period_ms_ = config.ntp.sync_check_period_ms;
    sync_timeout_ms_ = config.ntp.sync_timeout_ms;

    // Сохраняем единственный экземпляр для статического колбэка SNTP
    s_instance = this;

    // Подписываемся на события WiFi
    EventDispatcher::subscribe(EventType::WIFI_CONNECTED,
                               &NtpService::eventHandler, this);
    EventDispatcher::subscribe(EventType::WIFI_DISCONNECTED,
                               &NtpService::eventHandler, this);

    // Настраиваем часовой пояс — это стандартный POSIX-механизм,
    // tzset() вызывается неявно при первом обращении к localtime()
    setenv("TZ", timezone_, 1);
    tzset();

    ESP_LOGI(TAG,
             "initialized (server=%s, tz=%s, check=%lu ms, timeout=%lu ms)",
             ntp_server_, timezone_,
             static_cast<unsigned long>(sync_check_period_ms_),
             static_cast<unsigned long>(sync_timeout_ms_));

    initialized_ = true;
    return true;
}

bool NtpService::start() {
    if (!initialized_ || running_) return false;

    ESP_LOGI(TAG, "starting NTP service");

    if (xTaskCreate(taskEntry, "ntp", 2048, this, 2, &task_handle_) != pdPASS) {
        ESP_LOGE(TAG, "failed to create task");
        return false;
    }

    running_ = true;
    return true;
}

void NtpService::stop() {
    if (!running_) return;
    running_ = false;

    stopSntp();

    if (task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }

    ESP_LOGI(TAG, "stopped");
}

time_t NtpService::getTime() const {
    time_t now;
    time(&now);
    return now;
}

void NtpService::getLocalTime(struct tm *out_tm) const {
    time_t now;
    time(&now);
    localtime_r(&now, out_tm);
}

// ============================================================
// Статические трамплины
// ============================================================

void NtpService::taskEntry(void *arg) {
    static_cast<NtpService *>(arg)->taskLoop();
}

void NtpService::eventHandler(const Event &event, void *context) {
    NtpService *self = static_cast<NtpService *>(context);
    if (self == nullptr) return;

    switch (event.type) {
        case EventType::WIFI_CONNECTED:
            self->handleWifiConnected();
            break;
        case EventType::WIFI_DISCONNECTED:
            self->handleWifiDisconnected();
            break;
        default:
            break;
    }
}

void NtpService::sntpNotificationCallback(struct timeval *tv) {
    if (s_instance != nullptr) {
        s_instance->handleSntpNotification(tv);
    }
}

// ============================================================
// Внутренняя логика
// ============================================================

void NtpService::handleWifiConnected() {
    wifi_connected_ = true;
    time_synced_ = false;
    ESP_LOGI(TAG, "WiFi connected, starting SNTP");
    startSntp();
}

void NtpService::handleWifiDisconnected() {
    wifi_connected_ = false;
    time_synced_ = false;
    ESP_LOGW(TAG, "WiFi disconnected, stopping SNTP");
    stopSntp();
}

void NtpService::startSntp() {
    if (sntp_started_) return;

    // Стандартный ESP-IDF API для SNTP
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, ntp_server_);
    sntp_set_time_sync_notification_cb(sntpNotificationCallback);
    esp_sntp_init();

    sntp_started_ = true;
    ESP_LOGI(TAG, "SNTP started, server: %s", ntp_server_);
}

void NtpService::stopSntp() {
    if (!sntp_started_) return;

    // В ESP-IDF нет прямой функции остановки SNTP, но можно
    // деинициализировать и пометить как остановленный
    esp_sntp_stop();
    sntp_started_ = false;
}

void NtpService::handleSntpNotification(struct timeval *tv) {
    ESP_LOGI(TAG, "SNTP time sync notification");

    time_t now = tv->tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Проверяем, что год выглядит осмысленно (NTP может вернуть 1970 при
    // ошибке)
    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGW(TAG, "SNTP returned suspicious year: %d, ignoring",
                 timeinfo.tm_year + 1900);
        return;
    }

    time_synced_ = true;
    ESP_LOGI(TAG, "time synchronized: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    publishTimeSyncedEvent(now);
}

void NtpService::publishTimeSyncedEvent(time_t timestamp) {
    Event event{};
    event.type = EventType::TIME_SYNCED;
    event.timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
    event.data.time.timestamp_s = static_cast<uint64_t>(timestamp);

    if (!EventBus::publish(event, 10)) {
        ESP_LOGW(TAG, "failed to publish TIME_SYNCED event");
    }
}

// ============================================================
// Задача: периодическая проверка статуса синхронизации
// ============================================================

void NtpService::taskLoop() {
    uint32_t elapsed_ms = 0;
    bool timeout_reported = false;

    while (running_) {
        vTaskDelay(pdMS_TO_TICKS(sync_check_period_ms_));
        elapsed_ms += sync_check_period_ms_;

        if (!sntp_started_) continue;

        // Проверяем статус синхронизации через стандартный API
        sntp_sync_status_t sync_status = esp_sntp_get_sync_status();

        if (sync_status == SNTP_SYNC_STATUS_COMPLETED && !time_synced_) {
            // Синхронизация завершена, но мы не получили уведомление
            // (бывает при быстром ответе NTP-сервера)
            time_t now;
            time(&now);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);

            if (timeinfo.tm_year >= (2020 - 1900)) {
                time_synced_ = true;
                ESP_LOGI(TAG, "SNTP sync confirmed via status check");
                publishTimeSyncedEvent(now);
            }
            elapsed_ms = 0;
            timeout_reported = false;
            continue;
        }

        if (sync_status == SNTP_SYNC_STATUS_RESET) {
            ESP_LOGW(TAG, "SNTP sync was reset");
            time_synced_ = false;
            elapsed_ms = 0;
            timeout_reported = false;
            continue;
        }

        // Проверяем таймаут ожидания первой синхронизации
        if (!time_synced_ && elapsed_ms >= sync_timeout_ms_ &&
            !timeout_reported) {
            ESP_LOGW(TAG, "SNTP sync timeout (%lu ms)",
                     static_cast<unsigned long>(sync_timeout_ms_));

            Event event{};
            event.type = EventType::TIME_SYNC_FAILED;
            event.timestamp =
                static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
            EventBus::publish(event, 10);

            timeout_reported = true;
        }
    }

    vTaskDelete(nullptr);
}

}  // namespace app
