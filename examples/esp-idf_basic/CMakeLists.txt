# Plain `idf.py build` project (no Arduino component involved) that
# exercises stackchan-arduino-ex as a native ESP-IDF component -- proof
# that the library really doesn't need arduino-esp32 to run.
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(scex_esp_idf_basic)
