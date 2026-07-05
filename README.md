# ESP32-UILibrary

A custom UI library for the **Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3**
(ESP32-P4, 480×800 ST7701 panel on 2-lane MIPI DSI), built directly on
ESP-IDF — no LVGL, no third-party GUI framework.

## Repository layout

```
components/          The library — reusable ESP-IDF components
  display/           Hardware abstraction: panel init, framebuffer, present()
examples/
  basic/             Minimal project running the display test loop
```

## Requirements

- ESP-IDF **v5.3 or newer** (developed against v6.0.1)
- Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 board

## Using the library in your own project

### Option A: git submodule (recommended)

From your ESP-IDF project root:

```sh
git submodule add <this-repo-url> components/ui_library
```

ESP-IDF discovers components in nested directories, but the safest setup is
to point `EXTRA_COMPONENT_DIRS` at the library's `components/` folder in your
top-level `CMakeLists.txt`, before the `project.cmake` include:

```cmake
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components/ui_library/components")
```

To update the library later:

```sh
git submodule update --remote components/ui_library
git add components/ui_library && git commit -m "Bump ui_library"
```

### Option B: plain clone

Clone this repo anywhere and point your project at it:

```cmake
set(EXTRA_COMPONENT_DIRS "/path/to/ESP32-UILibrary/components")
```

Update with `git pull`. Unlike a submodule, your project will not record
which library commit it was built against.

### Required sdkconfig settings

The display framebuffers live in PSRAM; your project **must** enable it.
Copy `examples/basic/sdkconfig.defaults` into your project (or merge it into
your existing one):

```
CONFIG_IDF_TARGET="esp32p4"
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_HEX=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360=y
```

### Using the Display class

Add `display` to your main component's `REQUIRES`:

```cmake
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES display
)
```

```cpp
#include "display.hpp"

extern "C" void app_main()
{
    static Display display;
    if (!display.init()) return;
    display.setBrightness(100);

    while (true) {
        uint16_t* fb = display.framebuffer();  // back buffer, RGB565
        // ... draw into fb (480 wide, 800 tall, row-major) ...
        display.present();  // flips buffers, blocks until vsync
    }
}
```

The display is double-buffered: `framebuffer()` always returns the buffer
that is *not* on screen, and `present()` performs a tear-free flip at the
next frame boundary. `present()` blocks for up to one frame (~16 ms), which
naturally paces a render loop to the panel refresh rate.

## Building the example

```sh
cd examples/basic
idf.py set-target esp32p4
idf.py -p <PORT> flash monitor
```

The example cycles through animated test scenes (color bars, checkerboard,
bouncing rectangles, ripples, plasma) — useful for verifying panel timing,
tearing behavior, and refresh smoothness.

## Roadmap

- [x] Display hardware abstraction (MIPI DSI + ST7701, double buffering)
- [ ] Renderer (drawing primitives, fonts)
- [ ] Widgets and layout
- [ ] Touch input (GT911)
- [ ] Animations
