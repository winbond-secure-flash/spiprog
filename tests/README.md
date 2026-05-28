# spiprog Tests

Unit tests for the SPI NOR flash driver using Google Test.

## Prerequisites

- CMake 3.10+
- C++17 compiler (MSVC, GCC, or Clang)
- Internet access (GoogleTest is fetched automatically via CMake FetchContent)

## Build and Run (Simulated Flash)

No hardware required. Uses an in-memory flash simulator.
```
cd tests
cmake -B build
cmake --build build
cd build && ctest --output-on-failure
```

## Build and Run (Real Flash Hardware)

Runs tests against a physical SPI NOR flash device. Destructive tests use a safe region at offset `0xF00000` (last 1MB).

> **⚠️ Warning:** This will erase and write to the flash chip. `TEST_OFFSET` is adjustable for your device via cmake option.

Before building with `-DUSE_REAL_FLASH=ON`, implement your SPI driver in `platform_spi_real.cpp`.
```
cd tests
cmake -B build -G Ninja -DUSE_REAL_FLASH=ON
cmake --build build
cd build && ctest --output-on-failure
```
### Run a specific test
```
./spiprog_tests --gtest_filter=SpiNorFlashTest.Write_SinglePage_Readback
```
### Run with verbose output
```
./spiprog_tests --gtest_print_time=1
```
### Run only non-destructive tests
```
./spiprog_tests --gtest_filter=SpiNorFlashTest.ReadId_:SpiNorFlashTest.Detect_:SpiNorFlashTest.Read_*
```
### Customize test offset
```
cmake -B build -DUSE_REAL_FLASH=ON -DTEST_FLASH_OFFSET=0xE00000
```
## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `USE_REAL_FLASH` | `OFF` | Use real hardware instead of simulator |
| `TEST_FLASH_OFFSET` | `0x0` (sim) / `0xF00000` (real) | Flash offset for destructive test operations |

## Configuration Overview

This project uses CMake for build configuration. The minimum required version is 3.10, and the project is set to use C++17 standard.

- **Option to select platform:** Configure with `USE_REAL_FLASH` option to build tests against real flash hardware or simulator (default).
- **Configurable test offset:** Set `TEST_FLASH_OFFSET` for destructive tests. Default is `0x0` for simulator and `0xF00000` for real hardware.

### Fetching GoogleTest

GoogleTest is fetched automatically using CMake's FetchContent module. The specified tag is `v1.14.0`. The `gtest_force_shared_crt` option is enabled to use shared CRT for GoogleTest.

### Platform Selection

The platform implementation is selected based on the `USE_REAL_FLASH` option:

- If `USE_REAL_FLASH` is ON, `platform_spi_real.cpp` is used, and the build is configured for REAL flash hardware.
- If `USE_REAL_FLASH` is OFF, `platform_spi_sim.cpp` is used, and the build is configured for simulated flash.

The effective test flash offset is displayed during configuration.

### Executable and Testing

The test executable `spiprog_tests` is created from `test_spi_nor_flash.cpp`, the selected platform source, and the SPI NOR flash driver source from the parent project.

Include directories are set for the SPI NOR flash driver and platform headers.

Compile definitions are set for the test flash offset.

GoogleTest libraries are linked, and tests are discovered for execution.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `Flash detection failed` | Check SPI wiring and `platform_spi_real.cpp` implementation |
| `FetchContent` download fails | Ensure internet access or pre-clone GoogleTest locally |
| Tests pass on sim but fail on real | Verify `TEST_FLASH_OFFSET` is within your chip's address range |
| Erase/write tests corrupt firmware | Change `TEST_FLASH_OFFSET` to an unused region |

## Note

Ensure that your SPI driver is implemented in `platform_spi_real.cpp` before running tests with real flash hardware to avoid data loss.
