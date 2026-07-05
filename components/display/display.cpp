#include "display.hpp"

#include "esp_err.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7701.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "soc/gpio_num.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

static const char* TAG = "Display";

// ── Hardware constants ────────────────────────────────────────────────────────
static constexpr int LCD_W           = 480;
static constexpr int LCD_H           = 800;
static constexpr gpio_num_t GPIO_RST = GPIO_NUM_27;
static constexpr gpio_num_t GPIO_BL  = GPIO_NUM_26;

// ── ST7701 initialization sequence (verbatim from Waveshare BSP) ─────────────
static const st7701_lcd_init_cmd_t init_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x17, 0x08}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0x40, 0xC9, 0x94, 0x0E, 0x10, 0x05, 0x0B, 0x09, 0x08, 0x26, 0x04, 0x52, 0x10, 0x69, 0x6B, 0x69}, 16, 0},
    {0xB1, (uint8_t[]){0x40, 0xD2, 0x98, 0x0C, 0x92, 0x07, 0x09, 0x08, 0x07, 0x25, 0x02, 0x0E, 0x0C, 0x6E, 0x78, 0x55}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x4E}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4E}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t[]){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x08, 0x01}, 2, 0},
    {0xED, (uint8_t[]){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};

// ── Backlight (LEDC) ──────────────────────────────────────────────────────────

static esp_err_t backlight_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_1,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
        .deconfigure     = false,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) return err;

    // The backlight enable on this board is active-low, so the PWM must be
    // inverted (matches the BSP). duty=0 + invert = pin high = backlight off
    // until setBrightness() is called.
    const ledc_channel_config_t ch = {
        .gpio_num    = GPIO_BL,
        .speed_mode  = LEDC_LOW_SPEED_MODE,
        .channel     = LEDC_CHANNEL_1,
        .intr_type   = LEDC_INTR_DISABLE,
        .timer_sel   = LEDC_TIMER_1,
        .duty        = 0,
        .hpoint      = 0,
        .sleep_mode  = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags       = {.output_invert = 1},
        .deconfigure = false,
    };
    return ledc_channel_config(&ch);
}

// ── Display::init ─────────────────────────────────────────────────────────────

bool Display::init()
{
    // 1. Backlight PWM (off until setBrightness is called)
    if (backlight_init() != ESP_OK) {
        ESP_LOGE(TAG, "backlight init failed");
        return false;
    }

    // 2. Power the MIPI DSI PHY (LDO channel 3, 2500 mV)
    const esp_ldo_channel_config_t ldo_cfg = {
        .chan_id    = 3,
        .voltage_mv = 2500,
        .flags      = {},
    };
    if (esp_ldo_acquire_channel(&ldo_cfg, &_ldo) != ESP_OK) {
        ESP_LOGE(TAG, "LDO acquire failed");
        return false;
    }

    // 3. MIPI DSI bus
    const esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id             = 0,
        .num_data_lanes     = 2,
        .phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = 500,
        .flags              = {},
    };
    if (esp_lcd_new_dsi_bus(&bus_cfg, &_dsi_bus) != ESP_OK) {
        ESP_LOGE(TAG, "DSI bus init failed");
        return false;
    }

    // 4. DBI command channel
    const esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel  = 0,
        .lcd_cmd_bits     = 8,
        .lcd_param_bits   = 8,
    };
    if (esp_lcd_new_panel_io_dbi(_dsi_bus, &dbi_cfg, &_io) != ESP_OK) {
        ESP_LOGE(TAG, "DBI IO init failed");
        return false;
    }

    // 5. DPI video stream config (480×800 @ ~30 MHz pixel clock, RGB565)
    esp_lcd_dpi_panel_config_t dpi_cfg = {
        .virtual_channel      = 0,
        .dpi_clk_src          = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz   = 30,
        .in_color_format      = LCD_COLOR_FMT_RGB565,
        .out_color_format     = LCD_COLOR_FMT_RGB565,
        .num_fbs              = 2,   // double buffering: draw to back, flip on present()
        .video_timing = {
            .h_size             = LCD_W,
            .v_size             = LCD_H,
            .hsync_pulse_width  = 12,
            .hsync_back_porch   = 42,
            .hsync_front_porch  = 42,
            .vsync_pulse_width  = 8,
            .vsync_back_porch   = 2,
            .vsync_front_porch  = 60,
        },
        .flags = {},
    };

    // 6. ST7701 vendor config
    st7701_vendor_config_t vendor_cfg = {
        .init_cmds      = init_cmds,
        .init_cmds_size = sizeof(init_cmds) / sizeof(init_cmds[0]),
        .mipi_config    = {.dsi_bus = _dsi_bus, .dpi_config = &dpi_cfg},
        .flags          = {.use_mipi_interface = 1, .mirror_by_cmd = 0, .auto_del_panel_io = 0},
    };

    // 7. Panel device config
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian    = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 16,
        .reset_gpio_num = GPIO_RST,
        .vendor_config  = &vendor_cfg,
        .flags          = {},
    };

    // 8. Create, reset, and initialize the panel
    if (esp_lcd_new_panel_st7701(_io, &panel_cfg, &_panel) != ESP_OK) {
        ESP_LOGE(TAG, "st7701 create failed");
        return false;
    }
    if (esp_lcd_panel_reset(_panel) != ESP_OK) {
        ESP_LOGE(TAG, "panel reset failed");
        return false;
    }
    if (esp_lcd_panel_init(_panel) != ESP_OK) {
        ESP_LOGE(TAG, "panel init failed");
        return false;
    }

    // 9. Enable DMA2D accelerator for draw_bitmap paths
    if (esp_lcd_dpi_panel_enable_dma2d(_panel) != ESP_OK) {
        ESP_LOGE(TAG, "DMA2D enable failed");
        return false;
    }

    // 10. Get both internal framebuffers
    if (esp_lcd_dpi_panel_get_frame_buffer(_panel, 2, &_fbs[0], &_fbs[1]) != ESP_OK) {
        ESP_LOGE(TAG, "get framebuffers failed");
        return false;
    }

    // 11. Refresh-done callback signals when a buffer flip has taken effect,
    //     i.e. the previously displayed buffer is safe to draw into again
    _refresh_sem = xSemaphoreCreateBinary();
    if (!_refresh_sem) {
        ESP_LOGE(TAG, "semaphore alloc failed");
        return false;
    }
    const esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = nullptr,
        .on_refresh_done     = onRefreshDone,
    };
    if (esp_lcd_dpi_panel_register_event_callbacks(_panel, &cbs, this) != ESP_OK) {
        ESP_LOGE(TAG, "register callbacks failed");
        return false;
    }

    // Buffer 0 is being scanned out (driver default); draw into buffer 1
    _back = 1;

    ESP_LOGI(TAG, "ready  fb0=%p fb1=%p  %dx%d", _fbs[0], _fbs[1], LCD_W, LCD_H);
    return true;
}

bool Display::onRefreshDone(esp_lcd_panel_handle_t /*panel*/,
                            esp_lcd_dpi_panel_event_data_t* /*edata*/, void* user_ctx)
{
    auto* self = static_cast<Display*>(user_ctx);
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR(self->_refresh_sem, &task_woken);
    return task_woken == pdTRUE;
}

// ── Display::present ──────────────────────────────────────────────────────────

void Display::present()
{
    // Drain any stale refresh signal so the wait below is for OUR flip
    xSemaphoreTake(_refresh_sem, 0);

    // Passing an internal framebuffer pointer to draw_bitmap takes the
    // zero-copy path: the driver flushes the cache and switches scan-out
    // to this buffer at the next frame boundary.
    esp_lcd_panel_draw_bitmap(_panel, 0, 0, LCD_W, LCD_H, _fbs[_back]);

    // Wait until the flip has taken effect; after that the previously
    // displayed buffer is no longer scanned out and becomes our back buffer.
    xSemaphoreTake(_refresh_sem, pdMS_TO_TICKS(100));

    _back ^= 1;
}

// ── Display::setBrightness ────────────────────────────────────────────────────

void Display::setBrightness(int percent)
{
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    uint32_t duty = (1023u * (uint32_t)percent) / 100u;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

// ── Accessors ─────────────────────────────────────────────────────────────────

uint16_t* Display::framebuffer() { return static_cast<uint16_t*>(_fbs[_back]); }
int Display::width()  const      { return LCD_W; }
int Display::height() const      { return LCD_H; }

// ── Bring-up test scenes ──────────────────────────────────────────────────────
// Everything below is throwaway test code; the real renderer replaces it.

static int8_t   s_sin[256];   // sine LUT, -127..127
static uint16_t s_pal[256];   // color-wheel palette, RGB565

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// hue 0..255 → fully saturated RGB565 color wheel
static uint16_t hue565(uint8_t h)
{
    uint8_t seg = h / 43;              // 6 segments of ~43
    uint8_t rem = (uint8_t)((h % 43) * 6);
    switch (seg) {
        case 0:  return rgb565(255, rem, 0);
        case 1:  return rgb565(255 - rem, 255, 0);
        case 2:  return rgb565(0, 255, rem);
        case 3:  return rgb565(0, 255 - rem, 255);
        case 4:  return rgb565(rem, 0, 255);
        default: return rgb565(255, 0, 255 - rem);
    }
}

static void lut_init()
{
    for (int i = 0; i < 256; i++) {
        s_sin[i] = (int8_t)lroundf(127.0f * sinf((float)i * 6.2831853f / 256.0f));
        s_pal[i] = hue565((uint8_t)i);
    }
}

// triangle wave 0..range (for stateless bouncing)
static int tri_wave(int t, int range)
{
    if (range <= 0) return 0;
    int m = t % (2 * range);
    return m < range ? m : 2 * range - m;
}

static void fill_rect(uint16_t* fb, int x, int y, int rw, int rh, uint16_t c)
{
    if (x < 0) { rw += x; x = 0; }
    if (y < 0) { rh += y; y = 0; }
    if (x + rw > LCD_W) rw = LCD_W - x;
    if (y + rh > LCD_H) rh = LCD_H - y;
    if (rw <= 0 || rh <= 0) return;
    for (int j = 0; j < rh; j++) {
        uint16_t* row = fb + (y + j) * LCD_W + x;
        for (int i = 0; i < rw; i++) row[i] = c;
    }
}

// Scene 0: full-screen hue fade
static void scene_hue_fade(uint16_t* fb, int t)
{
    uint16_t c = s_pal[(uint8_t)(t * 2)];
    for (int i = 0; i < LCD_W * LCD_H; i++) fb[i] = c;
}

// Scene 1: vertical color bars with a white sweep line
static void scene_color_bars(uint16_t* fb, int t)
{
    static const uint16_t bars[8] = {
        0xFFFF, 0xFFE0, 0x07FF, 0x07E0,   // white yellow cyan green
        0xF81F, 0xF800, 0x001F, 0x0000,   // magenta red blue black
    };
    // render one row, then replicate it
    for (int x = 0; x < LCD_W; x++) fb[x] = bars[x * 8 / LCD_W];
    int sweep = (t * 4) % LCD_W;
    for (int x = sweep; x < sweep + 6 && x < LCD_W; x++) fb[x] = 0xFFFF;
    for (int y = 1; y < LCD_H; y++) {
        memcpy(fb + y * LCD_W, fb, LCD_W * sizeof(uint16_t));
    }
}

// Scene 2: scrolling checkerboard
static void scene_checkerboard(uint16_t* fb, int t)
{
    const int cell = 48;
    int off = t;
    for (int y = 0; y < LCD_H; y++) {
        uint16_t* row = fb + y * LCD_W;
        int cy = ((y + off) / cell) & 1;
        for (int x = 0; x < LCD_W; x++) {
            int cx = ((x + off) / cell) & 1;
            row[x] = (cx ^ cy) ? 0xFFFF : rgb565(30, 30, 60);
        }
    }
}

// Scene 3: three bouncing rectangles
static void scene_bouncing_rects(uint16_t* fb, int t)
{
    // dark background
    uint16_t bg = rgb565(10, 10, 20);
    for (int i = 0; i < LCD_W * LCD_H; i++) fb[i] = bg;

    struct { int w, h, sx, sy; uint16_t c; } r[3] = {
        {120,  90, 5, 3, 0xF800},   // red
        { 90, 120, 3, 5, 0x07E0},   // green
        {100, 100, 4, 4, 0x001F},   // blue
    };
    for (int i = 0; i < 3; i++) {
        int x = tri_wave(t * r[i].sx + i * 173, LCD_W - r[i].w);
        int y = tri_wave(t * r[i].sy + i * 311, LCD_H - r[i].h);
        fill_rect(fb, x, y, r[i].w, r[i].h, r[i].c);
    }
}

// Scene 4: expanding color ripples from the center
static void scene_ripples(uint16_t* fb, int t)
{
    const int cx = LCD_W / 2, cy = LCD_H / 2;
    for (int y = 0; y < LCD_H; y++) {
        uint16_t* row = fb + y * LCD_W;
        int dy = abs(y - cy);
        for (int x = 0; x < LCD_W; x++) {
            int dx = abs(x - cx);
            // fast octagonal distance approximation
            int d = dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
            row[x] = s_pal[(uint8_t)(d - t * 3)];
        }
    }
}

// Scene 5: classic plasma
static void scene_plasma(uint16_t* fb, int t)
{
    static int16_t xterm[LCD_W];
    for (int x = 0; x < LCD_W; x++) {
        xterm[x] = s_sin[(uint8_t)(x + t)];
    }
    for (int y = 0; y < LCD_H; y++) {
        uint16_t* row = fb + y * LCD_W;
        int yterm = s_sin[(uint8_t)(y * 2 - t)];
        for (int x = 0; x < LCD_W; x++) {
            int v = xterm[x] + yterm + s_sin[(uint8_t)(((x + y) >> 1) + t * 2)];
            row[x] = s_pal[(uint8_t)(v / 3 + 128 + t)];
        }
    }
}

void Display::runTestLoop()
{
    lut_init();

    typedef void (*scene_fn)(uint16_t*, int);
    static const scene_fn scenes[] = {
        scene_hue_fade,
        scene_color_bars,
        scene_checkerboard,
        scene_bouncing_rects,
        scene_ripples,
        scene_plasma,
    };
    const int num_scenes      = sizeof(scenes) / sizeof(scenes[0]);
    const int frames_per_scene = 300;   // ~5 s per scene

    int scene = 0, t = 0;
    while (true) {
        // draw into the current back buffer; present() flips and blocks
        // until the flip takes effect, which paces us to the refresh rate
        scenes[scene](framebuffer(), t);
        present();
        if (++t >= frames_per_scene) {
            t = 0;
            scene = (scene + 1) % num_scenes;
        }
    }
}
