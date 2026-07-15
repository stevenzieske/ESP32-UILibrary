#include "canvas.hpp"
#include "display.hpp"
#include "esp_log.h"
#include "shapes.hpp"

static const char *TAG = "app";

extern "C" void app_main() {
  static Display display;

  if (!display.init()) {
    ESP_LOGE(TAG, "Display init failed");
    return;
  }

  display.setBrightness(100);

  const uint16_t bg = Canvas::rgb565(10, 10, 20);

  Rectangle rect{50, 50, 100, 60, Canvas::rgb565(0, 255, 255)};
  rect.brightness = 70; // dimmed toward black
  int rect_vx = 3, rect_vy = 2;

  Circle circle{240, 400, 40, Canvas::rgb565(255, 0, 0)};
  circle.opacity = 60; // blends with whatever it overlaps
  int circle_vx = -4, circle_vy = 3;

  while (true) {
    Canvas canvas(display.framebuffer(), display.width(), display.height());
    canvas.fill_rect(0, 0, display.width(), display.height(), bg);

    rect.move(rect_vx, rect_vy);
    if (rect.x < 0 || rect.x + rect.w > display.width())
      rect_vx = -rect_vx;
    if (rect.y < 0 || rect.y + rect.h > display.height())
      rect_vy = -rect_vy;
    rect.rotate(175.0f);
    rect.draw(canvas);

    circle.move(circle_vx, circle_vy);
    if (circle.x - circle.r < 0 || circle.x + circle.r > display.width())
      circle_vx = -circle_vx;
    if (circle.y - circle.r < 0 || circle.y + circle.r > display.height())
      circle_vy = -circle_vy;
    circle.draw(canvas);

    display.present();
  }
}
