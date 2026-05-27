# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.10)

project(spiprog LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform implementation (swap for different targets)
set(PLATFORM_SOURCES platform/platform_spi_stub.cpp)
set(PLATFORM_INCLUDE_DIRS platform)

add_executable(spiprog                     flash/spi_nor_flash.cpp spiprog.cpp ${PLATFORM_SOURCES} )
target_include_directories(spiprog PRIVATE flash platform	               ${PLATFORM_INCLUDE_DIRS})

