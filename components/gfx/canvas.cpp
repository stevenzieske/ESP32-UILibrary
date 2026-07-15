#include "canvas.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdint.h>

Canvas::Canvas(uint16_t *buffer, int width, int height)
    : _buffer(buffer), _width(width), _height(height) {}

uint16_t Canvas::rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

uint16_t Canvas::scale_brightness(uint16_t color, int percent) {
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;
  uint8_t r = (uint8_t)((color >> 11) & 0x1F) * percent / 100;
  uint8_t g = (uint8_t)((color >> 5) & 0x3F) * percent / 100;
  uint8_t b = (uint8_t)(color & 0x1F) * percent / 100;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

void Canvas::draw_pixel(int x, int y, uint16_t color) {
  if (x < 0 || x >= _width || y < 0 || y >= _height)
    return;
  _buffer[y * _width + x] = color;
}

void Canvas::draw_pixel_blend(int x, int y, uint16_t color, uint8_t alpha) {
  if (x < 0 || x >= _width || y < 0 || y >= _height)
    return;
  if (alpha == 255) {
    _buffer[y * _width + x] = color;
    return;
  }
  if (alpha == 0)
    return;

  uint16_t dst = _buffer[y * _width + x];
  uint8_t sr = (color >> 11) & 0x1F, sg = (color >> 5) & 0x3F,
          sb = color & 0x1F;
  uint8_t dr = (dst >> 11) & 0x1F, dg = (dst >> 5) & 0x3F, db = dst & 0x1F;
  uint8_t r = (uint8_t)((sr * alpha + dr * (255 - alpha)) / 255);
  uint8_t g = (uint8_t)((sg * alpha + dg * (255 - alpha)) / 255);
  uint8_t b = (uint8_t)((sb * alpha + db * (255 - alpha)) / 255);
  _buffer[y * _width + x] = (uint16_t)((r << 11) | (g << 5) | b);
}

void Canvas::fill_span(int x0, int x1, int y, uint16_t color, uint8_t alpha) {
  if (x0 > x1) {
    int tmp = x0;
    x0 = x1;
    x1 = tmp;
  }
  if (y < 0 || y >= _height)
    return;
  if (x0 < 0)
    x0 = 0;
  if (x1 >= _width)
    x1 = _width - 1;

  if (alpha == 255) {
    uint16_t *row = _buffer + y * _width;
    for (int x = x0; x <= x1; x++)
      row[x] = color;
  } else {
    for (int x = x0; x <= x1; x++)
      draw_pixel_blend(x, y, color, alpha);
  }
}

void Canvas::draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
  int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    draw_pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void Canvas::draw_rect(int x, int y, int w, int h, uint16_t color) {
  if (w <= 0 || h <= 0)
    return;
  draw_line(x, y, x + w - 1, y, color);
  draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
  draw_line(x, y, x, y + h - 1, color);
  draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void Canvas::fill_rect(int x, int y, int w, int h, uint16_t color,
                       uint8_t alpha) {
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > _width)
    w = _width - x;
  if (y + h > _height)
    h = _height - y;
  if (w <= 0 || h <= 0)
    return;

  if (alpha == 255) {
    for (int j = 0; j < h; j++) {
      uint16_t *row = _buffer + (y + j) * _width + x;
      for (int i = 0; i < w; i++)
        row[i] = color;
    }
  } else {
    for (int j = 0; j < h; j++)
      fill_span(x, x + w - 1, y + j, color, alpha);
  }
}

void Canvas::draw_circle(int cx, int cy, int r, uint16_t color) {
  int x = r, y = 0, err = 0;
  while (x >= y) {
    draw_pixel(cx + x, cy + y, color);
    draw_pixel(cx + y, cy + x, color);
    draw_pixel(cx - y, cy + x, color);
    draw_pixel(cx - x, cy + y, color);
    draw_pixel(cx - x, cy - y, color);
    draw_pixel(cx - y, cy - x, color);
    draw_pixel(cx + y, cy - x, color);
    draw_pixel(cx + x, cy - y, color);
    y += 1;
    err += 1 + 2 * y;
    if (2 * (err - x) + 1 > 0) {
      x -= 1;
      err += 1 - 2 * x;
    }
  }
}

void Canvas::fill_circle(int cx, int cy, int r, uint16_t color, uint8_t alpha) {
  if (r < 0)
    return;
  for (int dy = -r; dy <= r; dy++) {
    int dx = (int)sqrtf((float)(r * r - dy * dy));
    fill_span(cx - dx, cx + dx, cy + dy, color, alpha);
  }
}

void Canvas::fill_polygon(const Point *points, int count, uint16_t color,
                          uint8_t alpha) {
  if (count < 3)
    return;
  int minY = points[0].y, maxY = points[0].y;
  for (int i = 1; i < count; i++) {
    minY = std::min(minY, points[i].y);
    maxY = std::max(maxY, points[i].y);
  }
  minY = std::max(minY, 0);
  maxY = std::min(maxY, _height - 1);

  int xs[64];
  for (int y = minY; y <= maxY; y++) {
    int n = 0;
    for (int i = 0; i < count; i++) {
      Point a = points[i], b = points[(i + 1) % count];
      if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
        float t = (float)(y - a.y) / (float)(b.y - a.y);
        xs[n++] = (int)(a.x + t * (b.x - a.x));
      }
    }
    std::sort(xs, xs + n);
    for (int i = 0; i + 1 < n; i += 2)
      fill_span(xs[i], xs[i + 1], y, color, alpha);
  }
}
