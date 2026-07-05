idf_component_register(
    SRCS "display.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_lcd
        esp_lcd_st7701
        esp_hw_support
        esp_mm
        esp_driver_ledc
)
