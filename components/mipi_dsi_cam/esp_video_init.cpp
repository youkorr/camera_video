#include "esp_video_init.h"
#include "esp_log.h"

static const char *TAG = "esp_video_init";

extern "C" esp_err_t esp_video_init(const esp_video_init_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid esp_video_init() configuration");
        return ESP_ERR_INVALID_ARG;
    }

#if __has_include("esp_video_device_internal.h")
    // Version SDK Espressif complète (automatique)
    ESP_LOGI(TAG, "Calling native Espressif video initialization...");
    return ::esp_video_init(config);
#else
    // Version fallback (mode simulation / ESPHome / LVGL)
    ESP_LOGI(TAG, "⚙️ esp_video_init() — fallback mode (no SDK)");
    ESP_LOGI(TAG, "CSI:  %s", config->csi ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "DVP:  %s", config->dvp ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "JPEG: %s", config->jpeg ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "ISP:  %s", config->isp ? "ENABLED" : "DISABLED");

    // Crée un pseudo /dev/video0 si inexistant
    FILE *dev = fopen("/dev/video0", "w");
    if (dev) {
        fputs("ESP-VIDEO virtual device\n", dev);
        fclose(dev);
    } else {
        ESP_LOGW(TAG, "Cannot create /dev/video0 (stub mode)");
    }

    ESP_LOGI(TAG, "✅ esp_video_init() OK — stub active");
    return ESP_OK;
#endif
}

extern "C" esp_err_t esp_video_deinit(void)
{
    ESP_LOGI(TAG, "esp_video_deinit() called");
    return ESP_OK;
}
