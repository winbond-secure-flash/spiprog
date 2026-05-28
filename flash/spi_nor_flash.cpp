/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2026 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       spi_nor_flash.cpp
* @brief      SPI NOR flash driver implementation
*
* ### project spiprog
*
************************************************************************************************************/
#include "spi_nor_flash.h"
#include "platform_spi.h"
#include <cstring>
#include <chrono>
#include <thread>
#include <algorithm>

static bool isAllFF(const uint8_t* data, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++) {
        if (data[i] != 0xFF) return false;
    }
    return true;
}

static bool needsErase(const uint8_t* src, const uint8_t* flash, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++) {
        if (src[i] & ~flash[i]) return true;
    }
    return false;
}

int SpiNorFlash::spiTransaction(const uint8_t* out, uint32_t cmdSize, uint32_t addrSize,
                                 uint32_t dataOutSize, uint32_t dummyCycles,
                                 uint8_t* dataIn, uint32_t dataInSize)
{
    return PLATFORM_WriteReadTransaction(m_userData,
        out, cmdSize, addrSize, dataOutSize, dummyCycles,
        dataIn, dataInSize);
}

uint32_t SpiNorFlash::addrSizeFor(uint32_t address) const
{
    if (m_config.force4ByteAddr) return 4u;
    return (address > 0xFFFFFF) ? 4u : 3u;
}

uint32_t SpiNorFlash::buildCmdAddr(uint8_t* buf, uint8_t cmd3, uint8_t cmd4, uint32_t address) const
{
    if (m_config.force4ByteAddr) {
        // Flash is in 4-byte mode: use standard opcode with 4 address bytes
        buf[0] = cmd3;
        buf[1] = (address >> 24) & 0xFF;
        buf[2] = (address >> 16) & 0xFF;
        buf[3] = (address >> 8) & 0xFF;
        buf[4] = address & 0xFF;
        return 5;
    } else if (address > 0xFFFFFF) {
        // Flash is in 3-byte mode, but need >16MB: use 4-byte opcode variant
        buf[0] = cmd4;
        buf[1] = (address >> 24) & 0xFF;
        buf[2] = (address >> 16) & 0xFF;
        buf[3] = (address >> 8) & 0xFF;
        buf[4] = address & 0xFF;
        return 5;
    } else {
        // Flash is in 3-byte mode, addr fits: standard opcode + 3 bytes
        buf[0] = cmd3;
        buf[1] = (address >> 16) & 0xFF;
        buf[2] = (address >> 8) & 0xFF;
        buf[3] = address & 0xFF;
        return 4;
    }
}

int SpiNorFlash::readId(uint32_t& jedecId)
{
    uint8_t cmd = FlashCmd::READ_ID;
    uint8_t id[3] = {};
    int ret = spiTransaction(&cmd, 1, 0, 0, 0, id, 3);
    if (ret != 0) return ret;
    jedecId = (id[0] << 16) | (id[1] << 8) | id[2];
    return 0;
}

bool SpiNorFlash::detect()
{
    uint32_t id = 0;
    if (readId(id) != 0) return false;

    m_info.jedecId = id;
    uint8_t capacityCode = id & 0xFF;
    if (capacityCode >= 0x10 && capacityCode <= 0x22) {
        m_info.totalSize = 1u << capacityCode;
    } else {
        m_info.totalSize = 16 * 1024 * 1024;
    }
    m_info.name = "SPI NOR Flash";

    // Winbond manufacturer ID = 0xEF
    // uint8_t manufacturer = (id >> 16) & 0xFF;
    // if (manufacturer == 0xEF) {
    //   
    //}

    return true;
}

int SpiNorFlash::readStatusRegister(uint8_t& status)
{
    uint8_t cmd = FlashCmd::READ_STATUS_1;
    return spiTransaction(&cmd, 1, 0, 0, 0, &status, 1);
}

int SpiNorFlash::waitUntilReady(uint32_t timeoutMs)
{
    auto start = std::chrono::steady_clock::now();
    while (true) {
        uint8_t status = 0;
        int ret = readStatusRegister(status);
        if (ret != 0) return ret;
        if (!(status & StatusBits::BUSY)) return 0;

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeoutMs)
            return -1;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

int SpiNorFlash::writeEnable()
{
    uint8_t cmd = FlashCmd::WRITE_ENABLE;
    return spiTransaction(&cmd, 1, 0, 0, 0, nullptr, 0);
}

int SpiNorFlash::read(uint32_t address, uint8_t* buffer, uint32_t length)
{
    uint8_t cmd[5];
    uint32_t dummyCycles = 0;

    if (m_config.useFastRead) {                          // ← used here
        buildCmdAddr(cmd, FlashCmd::FAST_READ, FlashCmd::FAST_READ_4B, address);
        dummyCycles = 8;
    }
    else {
        buildCmdAddr(cmd, FlashCmd::READ, FlashCmd::READ_4B, address);
        dummyCycles = 0;
    }

    return spiTransaction(cmd, 1, addrSizeFor(address), 0, dummyCycles, buffer, length);
}

int SpiNorFlash::writePage(uint32_t address, const uint8_t* buffer, uint32_t length)
{
    if (length > m_info.pageSize) length = m_info.pageSize;

    int ret = writeEnable();
    if (ret != 0) return ret;

    uint8_t hdr[5];
    uint32_t hdrSize = buildCmdAddr(hdr, FlashCmd::PAGE_PROGRAM, FlashCmd::PAGE_PROGRAM_4B, address);

    std::vector<uint8_t> txBuf(hdrSize + length);
    std::memcpy(txBuf.data(), hdr, hdrSize);
    std::memcpy(txBuf.data() + hdrSize, buffer, length);

    ret = spiTransaction(txBuf.data(), 1, addrSizeFor(address), length, 0, nullptr, 0);
    if (ret != 0) return ret;

    return waitUntilReady(10);
}

int SpiNorFlash::write(uint32_t address, const uint8_t* buffer, uint32_t length)
{
    uint32_t offset = 0;
    while (offset < length) {
        uint32_t pageOffset = (address + offset) % m_info.pageSize;
        uint32_t chunkSize = std::min(m_info.pageSize - pageOffset, length - offset);

        int ret = writePage(address + offset, buffer + offset, chunkSize);
        if (ret != 0) return ret;
        offset += chunkSize;
    }
    return 0;
}

int SpiNorFlash::eraseSector(uint32_t address)
{
    int ret = writeEnable();
    if (ret != 0) return ret;

    uint8_t cmd[5];
    buildCmdAddr(cmd, FlashCmd::SECTOR_ERASE, FlashCmd::SECTOR_ERASE_4B, address);
    ret = spiTransaction(cmd, 1, addrSizeFor(address), 0, 0, nullptr, 0);
    if (ret != 0) return ret;
    return waitUntilReady(500);
}

int SpiNorFlash::eraseBlock64K(uint32_t address)
{
    int ret = writeEnable();
    if (ret != 0) return ret;

    uint8_t cmd[5];
    buildCmdAddr(cmd, FlashCmd::BLOCK_ERASE_64K, FlashCmd::BLOCK_ERASE_64K_4B, address);
    ret = spiTransaction(cmd, 1, addrSizeFor(address), 0, 0, nullptr, 0);
    if (ret != 0) return ret;
    return waitUntilReady(2000);
}

int SpiNorFlash::eraseChip()
{
    int ret = writeEnable();
    if (ret != 0) return ret;

    uint8_t cmd = FlashCmd::CHIP_ERASE;
    ret = spiTransaction(&cmd, 1, 0, 0, 0, nullptr, 0);
    if (ret != 0) return ret;
    return waitUntilReady(200000);
}

int SpiNorFlash::eraseRegion(uint32_t address, uint32_t length)
{
    uint32_t end = address + length;
    address &= ~(m_info.sectorSize - 1);

    while (address < end) {
        int ret;
        if ((address % m_info.blockSize) == 0 && (end - address) >= m_info.blockSize) {
            ret = eraseBlock64K(address);
            if (ret != 0) return ret;
            address += m_info.blockSize;
        } else {
            ret = eraseSector(address);
            if (ret != 0) return ret;
            address += m_info.sectorSize;
        }
    }
    return 0;
}

int SpiNorFlash::reflash(uint32_t address, const uint8_t* buffer, uint32_t length,
                          uint32_t& skippedBytes, uint32_t& erasedSectors)
{
    skippedBytes = 0;
    erasedSectors = 0;

    uint32_t offset = 0;

    while (offset < length) {
        // Work in sector-aligned chunks
        uint32_t sectorBase = (address + offset) & ~(m_info.sectorSize - 1);
        uint32_t offsetInSector = (address + offset) - sectorBase;
        uint32_t chunkSize = std::min(m_info.sectorSize - offsetInSector, length - offset);

        const uint8_t* src = buffer + offset;

        // Read current flash content for this chunk
        std::vector<uint8_t> flashBuf(chunkSize);
        int ret = read(address + offset, flashBuf.data(), chunkSize);
        if (ret != 0) return ret;

        // Already matches — skip entirely
        if (std::memcmp(src, flashBuf.data(), chunkSize) == 0) {
            skippedBytes += chunkSize;
            offset += chunkSize;
            continue;
        }

        // Source is all 0xFF — nothing to program (flash is either erased or has data we don't care about)
        if (isAllFF(src, chunkSize)) {
            // Only skip if flash is also all FF, otherwise we'd need to erase
            if (isAllFF(flashBuf.data(), chunkSize)) {
                skippedBytes += chunkSize;
                offset += chunkSize;
                continue;
            }
            // Flash has data but we want FF — need erase
        }

        if (needsErase(src, flashBuf.data(), chunkSize)) {
            // Read full sector to preserve surrounding data
            std::vector<uint8_t> sectorBuf(m_info.sectorSize);
            ret = read(sectorBase, sectorBuf.data(), m_info.sectorSize);
            if (ret != 0) return ret;

            // Merge new data into sector image
            std::memcpy(sectorBuf.data() + offsetInSector, src, chunkSize);

            // Erase and rewrite
            ret = eraseSector(sectorBase);
            if (ret != 0) return ret;
            erasedSectors++;

            // Write back full sector (skip pages that are all FF)
            for (uint32_t p = 0; p < m_info.sectorSize; p += m_info.pageSize) {
                if (!isAllFF(sectorBuf.data() + p, m_info.pageSize)) {
                    ret = writePage(sectorBase + p, sectorBuf.data() + p, m_info.pageSize);
                    if (ret != 0) return ret;
                }
            }
        } else {
            // Can program directly — all transitions are 1→0
            ret = write(address + offset, src, chunkSize);
            if (ret != 0) return ret;
        }

        offset += chunkSize;
    }

    return 0;
}