# ESP-IDF Root CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Default target to ESP32-S3 if not explicitly specified
if(NOT IDF_TARGET)
    set(IDF_TARGET esp32s3 CACHE STRING "Target MCU")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_local_ai)
