#include "display.hpp"
#include "esp_log.h"

static const char* TAG = "app";

extern "C" void app_main()
{
    static Display display;

    if (!display.init()) {
        ESP_LOGE(TAG, "Display init failed");
        return;
    }

    display.setBrightness(100);

    ESP_LOGI(TAG, "Starting display test loop");
    display.runTestLoop();   // never returns
}
