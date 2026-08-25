idf_component_register(
    SRC_DIRS "src" "src/drivers"
    INCLUDE_DIRS "src"
    # driver is kept for older (<5.3) IDF versions where the peripheral
    # drivers used here weren't split into their own components yet; the
    # esp_driver_* entries are what modern IDF (I2C master driver, UART,
    # LEDC) actually resolves driver/*.h against.
    REQUIRES driver esp_driver_i2c esp_driver_uart esp_driver_ledc freertos esp_timer log esp_common
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
