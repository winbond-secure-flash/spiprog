/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2026 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       spi_nor_flash.h
* @brief      SPI NOR flash driver class declaration and flash command definitions
*
* ### project spiprog
*
************************************************************************************************************/
#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace FlashCmd {
    constexpr uint8_t READ_ID         = 0x9F;
    constexpr uint8_t READ            = 0x03;
    constexpr uint8_t READ_4B         = 0x13;
    constexpr uint8_t WRITE_ENABLE    = 0x06;
    constexpr uint8_t PAGE_PROGRAM    = 0x02;
    constexpr uint8_t PAGE_PROGRAM_4B = 0x12;
    constexpr uint8_t SECTOR_ERASE    = 0x20;
    constexpr uint8_t SECTOR_ERASE_4B = 0x21;
    constexpr uint8_t BLOCK_ERASE_64K    = 0xD8;
    constexpr uint8_t BLOCK_ERASE_64K_4B = 0xDC;
    constexpr uint8_t CHIP_ERASE      = 0xC7;
    constexpr uint8_t READ_STATUS_1   = 0x05;
    constexpr uint8_t READ_STATUS_3   = 0x15; // Winbond Status Register 3
    constexpr uint8_t FAST_READ       = 0x0B;
    constexpr uint8_t FAST_READ_4B    = 0x0C;

    constexpr uint8_t READ_FR         = 0x70; // Reading Flag Register
    constexpr uint8_t CLEAR_FR        = 0x50; // Clearing Flag Register
}

namespace FlashTimeout {
    constexpr uint32_t PAGE_PROGRAM_MS  = 10;
    constexpr uint32_t SECTOR_ERASE_MS  = 800;
    constexpr uint32_t BLOCK_ERASE_MS   = 4000;
    constexpr uint32_t CHIP_ERASE_MS    = 400000;
    constexpr uint32_t DEFAULT_MS       = 5000;
}

namespace StatusBits {
    constexpr uint8_t BUSY = 0x01;
    constexpr uint8_t WEL  = 0x02;
}

/// @brief Flash device identification and geometry information.
struct FlashInfo {
    uint32_t jedecId    = 0;       ///< 3-byte JEDEC ID (manufacturer + device)
    uint32_t totalSize  = 0;       ///< Total flash size in bytes
    uint32_t pageSize   = 256;     ///< Page size in bytes (write granularity)
    uint32_t sectorSize = 4096;    ///< Sector size in bytes (minimum erase unit)
    uint32_t blockSize  = 65536;   ///< Block size in bytes (64KB erase unit)
    std::string name;              ///< Human-readable flash part name
    bool addrMode4Byte  = false;   ///< true if flash is in 4-byte address mode (resolved from device)
    bool addrModeResolved = false; ///< true if address mode was read from the device
};

/// @brief Runtime configuration for flash operations.
struct FlashConfig {
    bool useFastRead = false;      ///< Use FAST_READ (0x0B) instead of READ (0x03)
    bool force4ByteAddr = false;   ///< Force 4-byte address mode for >16MB flash
    uint32_t readChunkSize = 4096; ///< Maximum bytes per single read transaction
};

/// @brief Driver class for SPI NOR flash operations.
///
/// Provides read, write, erase, and smart reflash capabilities for SPI NOR
/// flash devices. Relies on a platform-specific SPI transport layer.
class SpiNorFlash {
public:
    SpiNorFlash() = default;

    /// @brief Construct with a platform-specific flash handle.
    /// @param userData Opaque handle passed to platform SPI functions.
    SpiNorFlash(void* userData) : m_userData(userData) {}

    /// @brief Set or update the platform flash handle.
    /// @param userData Opaque handle passed to platform SPI functions.
    void setFlashHandle(void* userData){m_userData = userData;}

    void* getFlashHandle() { return m_userData; }

    /// @brief Read the JEDEC ID from the flash device.
    /// @param[out] jedecId The 3-byte JEDEC ID (manufacturer byte in bits [23:16]).
    /// @return 0 on success, non-zero on SPI transaction failure.
    int readId(uint32_t& jedecId);

    /// @brief Detect and identify the connected flash device.
    ///
    /// Reads the JEDEC ID and populates internal FlashInfo with device geometry.
    /// Also auto-configures 4-byte addressing if the device requires it.
    /// @return true if a supported flash device was detected, false otherwise.
    bool detect();

    

    /// @brief Check if the flash is operating in 4-byte address mode.
    ///
    /// Examines bit 0 of the flag register to determine the current address mode.
    /// When set, the flash expects 4-byte addresses for read/write/erase commands.
    /// @param flagRegister The flag register value (obtained via readByteFR()).
    /// @return true if 4-byte address mode is active, false if 3-byte mode.
    bool isAddressModeFourBytes(const uint8_t flagRegister)
    {
    #define FLAG_REG_AM_FOUR_BYTE_MASK 0x01 
    return flagRegister & FLAG_REG_AM_FOUR_BYTE_MASK ? true : false;
    }

    /// @brief Get the detected flash device information.
    /// @return Reference to the FlashInfo structure (valid after detect() succeeds).
    const FlashInfo& getInfo() const { return m_info; }

    /// @brief Get the current flash configuration.
    /// @return Reference to the current FlashConfig.
    const FlashConfig& getConfig() const { return m_config; }

    /// @brief Set the flash configuration (fast read, address mode, etc.).
    /// @param config The configuration to apply.
    void setConfig(const FlashConfig& config) { m_config = config; }

    /// @brief Read data from flash.
    /// @param address Start address in flash.
    /// @param[out] buffer Destination buffer for read data.
    /// @param length Number of bytes to read.
    /// @return 0 on success, non-zero on failure.
    int read(uint32_t address, uint8_t* buffer, uint32_t length);

    /// @brief Write data to flash (page-program, no implicit erase).
    ///
    /// Handles page boundary splitting internally. The target region must
    /// already be erased (all 0xFF) for successful programming.
    /// @param address Start address in flash (does not need to be page-aligned).
    /// @param buffer Source data to write.
    /// @param length Number of bytes to write.
    /// @return 0 on success, non-zero on failure.
    int write(uint32_t address, const uint8_t* buffer, uint32_t length);

    /// @brief Erase a single 4KB sector.
    /// @param address Any address within the sector to erase (will be aligned down).
    /// @return 0 on success, non-zero on failure.
    int eraseSector(uint32_t address);

    /// @brief Erase a single 64KB block.
    /// @param address Any address within the block to erase (will be aligned down).
    /// @return 0 on success, non-zero on failure.
    int eraseBlock64K(uint32_t address);

    /// @brief Erase the entire flash chip.
    /// @return 0 on success, non-zero on failure.
    int eraseChip();

    /// @brief Erase a region of flash using optimal erase commands.
    ///
    /// Automatically selects between sector erase (4KB) and block erase (64KB)
    /// to minimize the number of erase operations.
    /// @param address Start address (will be aligned down to sector boundary).
    /// @param length Number of bytes to erase (will be rounded up to sector boundary).
    /// @return 0 on success, non-zero on failure.
    int eraseRegion(uint32_t address, uint32_t length);

    /// @brief Smart reflash: only erases and writes sectors that differ.
    ///
    /// For each sector-sized chunk, reads the current flash content and compares
    /// with the source buffer. Skips sectors that already match, erases only
    /// sectors where bit-clearing is required, and writes only changed pages.
    /// @param address Start address in flash.
    /// @param buffer Source data to program.
    /// @param length Number of bytes to program.
    /// @param[out] skippedBytes Total bytes skipped (already matching).
    /// @param[out] erasedSectors Number of sectors that were erased.
    /// @return 0 on success, non-zero on failure.
    int reflash(uint32_t address, const uint8_t* buffer, uint32_t length,
                uint32_t& skippedBytes, uint32_t& erasedSectors);

private:
    /// @brief Issue a Write Enable (WREN) command.
    /// @return 0 on success, non-zero on failure.
    int writeEnable();

    /// @brief Poll the status register until the BUSY bit clears.
    /// @param timeoutMs Maximum time to wait in milliseconds.
    /// @return 0 on success, non-zero on timeout or SPI failure.
    int waitUntilReady(uint32_t timeoutMs = FlashTimeout::DEFAULT_MS);

    /// @brief Read Status Register 1.
    /// @param[out] status The status register value.
    /// @return 0 on success, non-zero on failure.
    int readStatusRegister(uint8_t& status);

    /// @brief Read the Flag Register from the flash device.
    ///
    /// Issues the READ_FR (0x70) command to retrieve the flag register byte.
    /// The flag register provides status information such as address mode,
    /// erase/program errors, and protection status.
    ///
    /// In multi-I/O SPI modes (dual/quad), the flash requires dummy cycles
    /// between the command and data phase. In single-wire SPI mode, no dummy
    /// cycles are needed. Since the driver does not control the physical line
    /// configuration, the caller must specify the bus mode so the correct
    /// number of dummy cycles is applied.
    ///
    /// @param[out] fr The flag register value read from flash.
    /// @param isSpiSingle If true, no dummy cycles are inserted (single-wire SPI);
    ///                    if false, dummy cycles are inserted for multi-I/O mode.
    /// @return 0 on success, non-zero on SPI transaction failure.
    int readByteFR(uint8_t& fr, bool isSpiSingle);

    /// @brief Program a single page (up to 256 bytes, must not cross page boundary).
    /// @param address Start address (must be within a single page).
    /// @param buffer Data to write.
    /// @param length Number of bytes (max pageSize, must not cross page boundary).
    /// @return 0 on success, non-zero on failure.
    int writePage(uint32_t address, const uint8_t* buffer, uint32_t length);

    /// @brief Execute a raw SPI transaction via the platform layer.
    /// @param out Output buffer containing command + address + data bytes.
    /// @param cmdSize Number of command bytes (typically 1).
    /// @param addrSize Number of address bytes (0, 3, or 4).
    /// @param dataOutSize Number of data bytes to transmit after address.
    /// @param dummyCycles Number of dummy clock cycles before read phase.
    /// @param[out] dataIn Buffer for received data (may be nullptr if dataInSize is 0).
    /// @param dataInSize Number of bytes to read from the device.
    /// @return 0 on success, non-zero on failure.
    int spiTransaction(const uint8_t* out, uint32_t cmdSize, uint32_t addrSize,
                       uint32_t dataOutSize, uint32_t dummyCycles,
                       uint8_t* dataIn, uint32_t dataInSize);

    /// @brief Build command + address bytes into a buffer.
    ///
    /// Selects between 3-byte opcode (cmd3) and 4-byte opcode (cmd4) based
    /// on the current address mode configuration.
    /// @param[out] buf Output buffer (must be at least 5 bytes).
    /// @param cmd3 Opcode for 3-byte address mode.
    /// @param cmd4 Opcode for 4-byte address mode.
    /// @param address The target flash address.
    /// @return Total number of bytes written to buf (cmd + address).
    uint32_t buildCmdAddr(uint8_t* buf, uint8_t cmd3, uint8_t cmd4, uint32_t address) const;

    /// @brief Determine the address size for a given address.
    /// @param address The flash address.
    /// @return 3 or 4 (bytes), based on config and address range.
    uint32_t addrSizeFor(uint32_t address) const;

    void* m_userData = nullptr;
    FlashInfo m_info;
    FlashConfig m_config;
};