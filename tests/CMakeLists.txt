cmake_minimum_required(VERSION 3.10)
project(spiprog_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to select platform: SIM (default) or REAL
option(USE_REAL_FLASH "Build tests against real flash hardware" OFF)

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Flash driver source from parent project
set(SPIPROG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Select platform implementation
if(USE_REAL_FLASH)
    set(PLATFORM_SRC platform_spi_real.cpp)
    add_compile_definitions(TEST_ON_REAL_FLASH)
    message(STATUS "Tests configured for REAL flash hardware")
else()
    set(PLATFORM_SRC platform_spi_sim.cpp)
    message(STATUS "Tests configured for simulated flash")
endif()

add_executable(spiprog_tests
    test_spi_nor_flash.cpp
    ${PLATFORM_SRC}
    ${SPIPROG_ROOT}/flash/spi_nor_flash.cpp
)

target_include_directories(spiprog_tests PRIVATE
    ${SPIPROG_ROOT}/flash
    ${SPIPROG_ROOT}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(spiprog_tests PRIVATE gtest gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(spiprog_tests)