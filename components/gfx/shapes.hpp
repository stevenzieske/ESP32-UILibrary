#pragma once
#include "canvas.hpp"
#include <cmath>

namespace detail {
// percent: 0-100 -> alpha: 0-255, clamped.
inline uint8_t opacity_to_alpha(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return (uint8_t)(percent * 255 / 100);
}
}

struct Rectangle {
    int x, y, w, h;
    uint16_t color;
    float angle = 0.0f;    // degrees, clockwise, about the rectangle's center
    int brightness = 100;  // 0-100%, dims color toward black
    int opacity = 100;     // 0-100%, blends with what's already drawn underneath

    void draw(Canvas &canvas) const {
        uint16_t c = Canvas::scale_brightness(color, brightness);
        uint8_t alpha = detail::opacity_to_alpha(opacity);

        if (angle == 0.0f) {
            // Fast, exact path: no rotation, so the plain axis-aligned fill applies.
            canvas.fill_rect(x, y, w, h, c, alpha);
            return;
        }

        float cx = x + w / 2.0f;
        float cy = y + h / 2.0f;
        float hw = w / 2.0f;
        float hh = h / 2.0f;
        float rad = angle * (3.14159265f / 180.0f);
        float cs = cosf(rad), sn = sinf(rad);

        // Rotate each corner (relative to center) by (cs, sn), then translate back.
        const float local[4][2] = {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
        Point corners[4];
        for (int i = 0; i < 4; i++) {
            float lx = local[i][0], ly = local[i][1];
            corners[i].x = (int)(cx + lx * cs - ly * sn);
            corners[i].y = (int)(cy + lx * sn + ly * cs);
        }
        canvas.fill_polygon(corners, 4, c, alpha);
    }

    void move(int dx, int dy) {
        x += dx;
        y += dy;
    }

    // Relative rotation, matching move()'s delta convention.
    void rotate(float degrees) {
        angle = fmodf(angle + degrees, 360.0f);
    }
};

struct Circle {
    int x, y, r;
    uint16_t color;
    int brightness = 100;  // 0-100%, dims color toward black
    int opacity = 100;     // 0-100%, blends with what's already drawn underneath

    void draw(Canvas &canvas) const {
        uint16_t c = Canvas::scale_brightness(color, brightness);
        uint8_t alpha = detail::opacity_to_alpha(opacity);
        canvas.fill_circle(x, y, r, c, alpha);
    }

    void move(int dx, int dy) {
        x += dx;
        y += dy;
    }
};
