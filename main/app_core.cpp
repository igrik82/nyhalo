#include "app_core.hpp"

#include "app_config.hpp"
// #include "data/telemetry_queue.hpp"
#include "board.hpp"
#include "drivers/pir_driver.hpp"
#include "drivers/rgb_led_driver.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "events/event_bus.hpp"
#include "events/event_dispatcher.hpp"
#include "events/event_dispatcher_task.hpp"
#include "freertos/idf_additions.h"
#include "services/heartbeat_service.hpp"
// #include "services/sensor_emulator_service.hpp"
#include "fsm/status_led_service.hpp"
// #include "services/telemetry_service.hpp"
#include "services/motion_service.hpp"
#include "static_resources.hpp"

static const char *TAG = "APP_CORE";

namespace app {

// TODO: Перевести все на статическое выделение памяти
static led_driver::RgbLedDriver *s_rgb_led = nullptr;
static StatusLedService *s_status_led_service = nullptr;

// Очередь движения
static QueueHandle_t s_motion_raw_queue = nullptr;
static drivers::PirDriver s_pir_driver(board::PIR_SENSOR);
static MotionService *s_motion_service = nullptr;

// static data::TelemetryQueue s_telemetry_queue;
static HeartbeatService s_heartbeat_service;
// static TelemetryService s_telemetry_service(s_telemetry_queue);
// static SensorEmulatorService s_sensor_emulator_service(s_telemetry_queue);
// static StatusLedService s_status_led_service(led_driver::RgbLedDriver(led));

bool init() {
    ESP_LOGI(TAG, "application init");

    initConfig();
    const Config &config = getConfig();

    // =================================================
    // Создание статических очередей
    // =================================================

    // Очередь движения
    s_motion_raw_queue = xQueueCreateStatic(
        static_resources::MotionRawQueue::LENGTH, sizeof(RawPirData),
        static_resources::motion_raw_queue.storage,
        &static_resources::motion_raw_queue.structure);
    if (s_motion_raw_queue == nullptr) {
        ESP_LOGE(TAG, "failed to create motion raw queue");
        return false;
    }

    // =================================================
    // Инициализация драйверов
    // =================================================

    // Создание объекта драйвера светодиодов
    static led_driver::RgbLedDriver rgb_led(
        board::RGB_LED, static_cast<uint32_t>(config.rgb_led.led_count));
    static StatusLedService status_led_service(rgb_led);
    s_rgb_led = &rgb_led;
    s_status_led_service = &status_led_service;

    // Драйвер датчика движения
    if (!s_pir_driver.init(s_motion_raw_queue)) {
        ESP_LOGE(TAG, "failed to init pir driver");
        return false;
    }

    // =================================================
    // Инициализация сервисов
    // =================================================

    // Инициализация инфраструктуры событий
    if (!EventBus::init()) {
        ESP_LOGE(TAG, "failed to init event bus");
        return false;
    }

    if (!EventDispatcher::init()) {
        ESP_LOGE(TAG, "failed to init event dispatcher");
        return false;
    }

    // // Инициализация очередей данных
    // if (!s_telemetry_queue.init()) {
    //     ESP_LOGE(TAG, "failed to init telemetry queue");
    //     return false;
    // }

    // Инициализация сервисов
    if (!s_heartbeat_service.init()) {
        ESP_LOGE(TAG, "failed to init heartbeat service");
        return false;
    }

    // Инициализация сервиса движения
    static MotionService motion_service(s_motion_raw_queue);
    s_motion_service = &motion_service;
    if (!s_motion_service->init(config.motion.idle_timeout_ms,
                                config.motion.wait_period_ms)) {
        ESP_LOGE(TAG, "failed to init motion service");
        return false;
    }

    // if (!s_sensor_emulator_service.init()) {
    //     ESP_LOGE(TAG, "failed to init sensor emulator service");
    //     return false;
    // }

    // if (!s_telemetry_service.init()) {
    //     ESP_LOGE(TAG, "failed to init telemetry service");
    //     return false;
    // }

    if (!s_status_led_service->init()) {
        ESP_LOGE(TAG, "failed to init status led service");
        return false;
    }

    // Инициализация задачи диспетчера
    if (!EventDispatcherTask::init()) {
        ESP_LOGE(TAG, "failed to init event dispatcher task");
        return false;
    }

    return true;
}

void start() {
    ESP_LOGI(TAG, "application start");

    // Запуск задачи диспетчера событий
    if (!EventDispatcherTask::start()) {
        ESP_LOGE(TAG, "failed to start event dispatcher task");
        return;
    }

    // Запуск сервисов
    s_heartbeat_service.start();
    // s_sensor_emulator_service.start();
    // s_telemetry_service.start();
    s_status_led_service->start();
    s_motion_service->start();

    // Публикация события загрузки
    Event boot_event{};
    boot_event.type = EventType::SYSTEM_BOOT;
    boot_event.timestamp =
        static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
    EventBus::publish(boot_event, 10);

    ESP_LOGI(TAG, "application started");
}

}  // namespace app
