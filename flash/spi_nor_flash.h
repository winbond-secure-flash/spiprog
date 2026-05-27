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

struct FlashInfo {
    uint32_t jedecId    = 0;
    uint32_t totalSize  = 0;
    uint32_t pageSize   = 256;
    uint32_t sectorSize = 4096;
    uint32_t blockSize  = 65536;
    std::string name;
};

struct FlashConfig {
    bool useFastRead = false;
    bool force4ByteAddr = false;
};

class SpiNorFlash {
public:
    SpiNorFlash() = default;
    SpiNorFlash(const void* userData) : m_userData(userData) {}

    // Identification
    void setFlashHandle(const void* userData){m_userData = userData;}
    int readId(uint32_t& jedecId);
    bool detect();
    const FlashInfo& getInfo() const { return m_info; }
    const FlashConfig& getConfig() const { return m_config; }
    void setConfig(const FlashConfig& config) { m_config = config; }

    // Basic primitives (no erase logic, direct flash commands)
    int read(uint32_t address, uint8_t* buffer, uint32_t length);
    int write(uint32_t address, const uint8_t* buffer, uint32_t length);
    int eraseSector(uint32_t address);
    int eraseBlock64K(uint32_t address);
    int eraseChip();
    int eraseRegion(uint32_t address, uint32_t length);

    // Smart reflash: reads first, erases only if needed, skips unchanged pages
    int reflash(uint32_t address, const uint8_t* buffer, uint32_t length,
                uint32_t& skippedBytes, uint32_t& erasedSectors);

private:
    int writeEnable();
    int waitUntilReady(uint32_t timeoutMs = FlashTimeout::DEFAULT_MS);
    int readStatusRegister(uint8_t& status);
    int writePage(uint32_t address, const uint8_t* buffer, uint32_t length);

    int spiTransaction(const uint8_t* out, uint32_t cmdSize, uint32_t addrSize,
                       uint32_t dataOutSize, uint32_t dummyCycles,
                       uint8_t* dataIn, uint32_t dataInSize);

    uint32_t buildCmdAddr(uint8_t* buf, uint8_t cmd3, uint8_t cmd4, uint32_t address) const;
    uint32_t addrSizeFor(uint32_t address) const;

    const void* m_userData = nullptr;
    FlashInfo m_info;
    FlashConfig m_config;
};