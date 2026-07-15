#pragma once
#include <cstdint>

struct Point {
  int x, y;
};

class Canvas {
public:
  Canvas(uint16_t *buffer, int width, int height);

  static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

  // Scales each channel of color by percent (0-100), dimming it toward black.
  static uint16_t scale_brightness(uint16_t color, int percent);

  void draw_pixel(int x, int y, uint16_t color);
  void draw_line(int x0, int y0, int x1, int y1, uint16_t color);
  void draw_rect(int x, int y, int w, int h, uint16_t color);

  // alpha: 0 = fully transparent (no-op), 255 = fully opaque (default).
  // Values below 255 blend with whatever is already in the buffer.
  void fill_rect(int x, int y, int w, int h, uint16_t color, uint8_t alpha = 255);
  void draw_circle(int cx, int cy, int r, uint16_t color);
  void fill_circle(int cx, int cy, int r, uint16_t color, uint8_t alpha = 255);
  void draw_polygon(const Point *points, int count, uint16_t color);
  void fill_polygon(const Point *points, int count, uint16_t color, uint8_t alpha = 255);

private:
  uint16_t *_buffer;
  int _width, _height;

  void draw_pixel_blend(int x, int y, uint16_t color, uint8_t alpha);
  void fill_span(int x0, int x1, int y, uint16_t color, uint8_t alpha);
};
