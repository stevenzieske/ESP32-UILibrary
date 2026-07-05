#pragma once
#include <cstdint>
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_types.h"
#include "esp_ldo_regulator.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class Display {
public:
    bool init();
    void present();
    void setBrightness(int percent);

    // Bring-up test only: cycles through animated test scenes forever.
    // Will be removed once the real renderer exists.
    [[noreturn]] void runTestLoop();

    // Returns the back buffer (safe to draw into). After present() this
    // points to the other buffer.
    uint16_t* framebuffer();
    int width() const;
    int height() const;

private:
    esp_ldo_channel_handle_t    _ldo         = nullptr;
    esp_lcd_dsi_bus_handle_t    _dsi_bus     = nullptr;
    esp_lcd_panel_io_handle_t   _io          = nullptr;
    esp_lcd_panel_handle_t      _panel       = nullptr;
    void*                       _fbs[2]      = {nullptr, nullptr};
    int                         _back        = 0;        // index of the draw target
    SemaphoreHandle_t           _refresh_sem = nullptr;  // signaled by on_refresh_done

    static bool onRefreshDone(esp_lcd_panel_handle_t panel,
                              esp_lcd_dpi_panel_event_data_t* edata, void* user_ctx);
};
