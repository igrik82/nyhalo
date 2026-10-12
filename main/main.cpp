#include "app_core.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void) {
    // Set log level
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS issue, erasing and retrying: %s",
                 esp_err_to_name(ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "PIR CAM application starting");

    if (!app::init()) {
        ESP_LOGE(TAG, "application init failed");
        return;
    }

    app::start();

    ESP_LOGI(TAG, "application started");
}
