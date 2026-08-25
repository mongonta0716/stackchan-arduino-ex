idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES stackchan-arduino-ex spiffs
)

spiffs_create_partition_image(storage ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs_image FLASH_IN_PROJECT)
