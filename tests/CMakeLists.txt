cmake_minimum_required(VERSION 3.10)
project(spiprog_tests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to select platform: SIM (default) or FTDI
option(USE_FTDI_4222H "Build tests with FT4222H platform over USB-SPI" OFF)

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
if(USE_FTDI_4222H)
    set(PLATFORM_SRC platform/ft4222h_platform.cpp)
    add_compile_definitions(TEST_ON_REAL_FLASH)
    add_compile_definitions(TEST_ON_FTDI_4222H)
    message(STATUS "Tests configured for FT4222H platform (USB-SPI)")
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

# ============================================================================
# FTDI Library Detection and Configuration (for USE_FTDI_4222H)
# ============================================================================
if(USE_FTDI_4222H)
    get_filename_component(FTDI_IMPORTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/libft4222/imports" ABSOLUTE)

    target_include_directories(spiprog_tests PRIVATE
        ${FTDI_IMPORTS}/ftd2xx
        ${FTDI_IMPORTS}/LibFT4222/inc
    )

    if(WIN32)
        find_library(FT2XX_LIB NAMES ftd2xx PATHS ${FTDI_IMPORTS}/ftd2xx/amd64 NO_DEFAULT_PATH)
        find_library(FT4222_LIB NAMES LibFT4222 PATHS ${FTDI_IMPORTS}/LibFT4222/lib/amd64 NO_DEFAULT_PATH)

        if(NOT FT2XX_LIB OR NOT FT4222_LIB)
            message(FATAL_ERROR "Could not find FTDI libraries in: ${FTDI_IMPORTS}")
        endif()

        target_link_libraries(spiprog_tests PRIVATE ${FT2XX_LIB} ${FT4222_LIB})

        # Copy DLLs to executable location
        add_custom_command(
            TARGET spiprog_tests
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FTDI_IMPORTS}/LibFT4222/lib/amd64/LibFT4222-64.dll"
                "$<TARGET_FILE_DIR:spiprog_tests>"
        )
    elseif(UNIX)
        find_library(FT2XX_LIB NAMES ftd2xx d2xx)
        find_library(FT4222_LIB NAMES ft4222 ft4233)

        if(NOT FT2XX_LIB)
            find_library(FT2XX_LIB NAMES ftd2xx PATHS ${FTDI_IMPORTS}/ftd2xx/linux NO_DEFAULT_PATH)
        endif()
        if(NOT FT4222_LIB)
            find_library(FT4222_LIB NAMES ft4222 ft4233 PATHS ${FTDI_IMPORTS}/LibFT4222/lib/linux NO_DEFAULT_PATH)
        endif()

        if(FT2XX_LIB AND FT4222_LIB)
            target_link_libraries(spiprog_tests PRIVATE ${FT2XX_LIB} ${FT4222_LIB})
            message(STATUS "FTDI libraries found: ${FT2XX_LIB}, ${FT4222_LIB}")
        else()
            message(WARNING "FTDI libraries not found. Install libftd2xx and libft4222.")
        endif()
    endif()
endif()

target_link_libraries(spiprog_tests PRIVATE gtest gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(spiprog_tests)