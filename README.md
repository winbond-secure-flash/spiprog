# spiprog

A cross-platform command-line tool for reading, writing, erasing, and reflashing SPI NOR flash chips.

## Features

- **Read/Write/Erase** — basic flash operations
- **Reflash** — smart programming that reads flash first, skips unchanged pages, and only erases sectors where bit transitions require it (0→1)
- **Verify** — read-back comparison with exact mismatch reporting
- **4-byte address format** — can be set via CLI flag
- **Fast Read** — optional FAST_READ (0x0B) for higher throughput at supported clock speeds

## Architecture

    ┌──────────────────────────┐
    │   CLI (spiprog.cpp)      │  Command parsing, file I/O, progress
    ├──────────────────────────┤
    │  Flash Layer             │  Page-chunked writes, smart erase,
    │  (spi_nor_flash.cpp/h)   │  address mode handling, status polling
    ├──────────────────────────┤
    │  Platform Stubs          │  PLATFORM_WriteReadTransaction()
    │  (platform_spi_stub.cpp) │  PLATFORM_FlashHandleGet/Put()
    └──────────────────────────┘

The flash layer and CLI are fully cross-platform (C++17, no OS-specific code). All platform-specific logic is isolated in three functions that the user must implement.

## Building

### Requirements

- C++17 compiler (GCC, Clang, or MSVC)

### Build steps

    cmake -B build
    cmake --build build

## Platform Integration

Before the tool can communicate with real hardware, you must provide implementations for three platform functions. The stub file `platform_spi_stub.cpp` contains placeholder implementations that will produce **compiler deprecation warnings** as a reminder.

### Functions to implement

#### `void* PLATFORM_FlashHandleGet()`

Returns an opaque handle representing the flash device. This is platform-specific — for example, a file descriptor to `/dev/spi_tunnel`, an FTDI context, or a driver handle.

#### `void PLATFORM_FlashHandlePut(void* handle)`

Releases the handle obtained by `PLATFORM_FlashHandleGet()`. Called once at application exit.

#### `int PLATFORM_WriteReadTransaction(...)`

Performs a single SPI transaction. The `userData` parameter is the handle returned by `PLATFORM_FlashHandleGet()`, forwarded automatically by the flash layer on every call.

    int PLATFORM_WriteReadTransaction(
        const void*     userData,       // flash handle
        const uint8_t*  dataOutStream,  // TX buffer (opcode + address + data)
        uint32_t        cmdSize,        // opcode size in bytes (typically 1)
        uint32_t        addressSize,    // address size: 0, 3, or 4 bytes
        uint32_t        dataOutSize,    // write data size after address
        uint32_t        dummyCycles,    // dummy cycles between address and data-in
        uint8_t*        dataIn,         // RX buffer (read data)
        uint32_t        dataInSize      // expected read data size
    );

### Integration example

Replace `platform_spi_stub.cpp` with your own implementation:

    // platform_spi_linux.cpp
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/ioctl.h>

    void* PLATFORM_FlashHandleGet()
    {
        int* fd = new int;
        *fd = open("/dev/spi_tunnel", O_RDWR);
        return (*fd >= 0) ? fd : nullptr;
    }

    void PLATFORM_FlashHandlePut(void* handle)
    {
        int* fd = static_cast<int*>(handle);
        if (fd) { close(*fd); delete fd; }
    }

    int PLATFORM_WriteReadTransaction(const void* userData, ...)
    {
        int fd = *static_cast<const int*>(userData);
        // ... ioctl to kernel module ...
    }

## Usage

    spiprog [global options] <command> [command options]

### Global options

| Option | Description |
|--------|-------------|
| `--fast-read` | Use FAST_READ (0x0B) instead of READ (0x03) |
| `--4byte-mode` | Force 4-byte address mode |

### Commands

| Command | Description |
|---------|-------------|
| `id` | Read and display JEDEC ID |
| `read <offset> <length> <file>` | Read flash region to file |
| `write <offset> <file> [--verify]` | Erase + write file to flash (raw) |
| `reflash <offset> <file> [--verify]` | Smart reflash (skip unchanged, erase only if needed) |
| `erase <offset> <length>` | Erase a flash region |
| `erase_chip` | Erase entire chip |
| `verify <offset> <file>` | Verify flash contents against file |

Offset and length accept hex (`0x...`) or decimal.

### Examples

    # Identify the flash chip
    spiprog id

    # Read 1MB from flash start
    spiprog read 0x0 0x100000 dump.bin

    # Read 32MB using fast read
    spiprog --fast-read read 0x0 0x2000000 full.bin

    # Write firmware with verification
    spiprog write 0x0 firmware.bin --verify

    # Smart reflash — only erases/writes what changed
    spiprog reflash 0x10000 config.bin --verify

    # Erase one 64KB block
    spiprog erase 0x0 0x10000

    # Erase entire chip
    spiprog erase_chip

    # Verify flash matches file
    spiprog verify 0x0 firmware.bin

## Flash Detection

On startup, the tool reads the JEDEC ID to identify the flash chip.
The tool defaults to 3-byte mode. Use `--4byte-mode` if your flash requires it.

## write vs reflash

| | `write` | `reflash` |
|---|---------|-----------|
| Erase | Always erases entire region | Only erases sectors needing 0→1 bit transitions |
| Write | Writes all data | Skips pages that already match or are all 0xFF |
| Speed | Predictable | Faster for partial updates |
| Use case | Clean flash, first-time programming | Firmware updates, config changes |

## Project Structure

    spiprog/
    ├── CMakeLists.txt              # Top-level CMake
    ├── README.md
    └── spiprog/
        ├── CMakeLists.txt          # Build target
        ├── spiprog.h               # App includes
        ├── spiprog.cpp             # CLI entry point
        ├── spi_nor_flash.h         # Flash operations API
        ├── spi_nor_flash.cpp       # Flash operations implementation
        ├── platform_spi.h          # Platform function declarations
        └── platform_spi_stub.cpp   # Stubs (replace with real implementation)

## Supported Flash Chips

Should work with any JEDEC-compatible SPI NOR flash using standard opcodes. Timeouts are tuned for Winbond datasheets — see `FlashTimeout` namespace in `spi_nor_flash.h` to adjust for other vendors.
