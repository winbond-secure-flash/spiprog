/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2026 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       platform_spi_sim.cpp
* @brief      Simulated SPI NOR flash platform implementation for unit testing
*
* ### project spiprog
*
************************************************************************************************************/
#include "platform_spi_sim.h"
#include "platform_spi.h"
#include "spi_nor_flash.h"
#include <thread>
#include <chrono>

static FlashSimulator g_sim;

FlashSimulator& getFlashSimulator() { return g_sim; }

FlashSimulator::FlashSimulator(uint32_t size, uint32_t jedecId)
    : m_memory(size, 0xFF), m_jedecId(jedecId) {}

void FlashSimulator::reset()
{
    std::fill(m_memory.begin(), m_memory.end(), 0xFF);
    m_writeEnabled = false;
}

void FlashSimulator::preload(uint32_t offset, const uint8_t* data, uint32_t length)
{
    if (offset + length <= m_memory.size())
        std::memcpy(m_memory.data() + offset, data, length);
}

uint32_t FlashSimulator::parseAddress(const uint8_t* buf, uint32_t addrSize) const
{
    uint32_t addr = 0;
    for (uint32_t i = 0; i < addrSize; i++)
        addr = (addr << 8) | buf[i];
    return addr;
}

int FlashSimulator::transaction(const uint8_t* dataOut, uint32_t cmdSize, uint32_t addrSize,
                                 uint32_t dataOutSize, uint32_t dummyCycles,
                                 uint8_t* dataIn, uint32_t dataInSize)
{
    if (cmdSize == 0) return -1;
    (void)dummyCycles;

    uint8_t cmd = dataOut[0];
    uint32_t address = (addrSize > 0) ? parseAddress(dataOut + cmdSize, addrSize) : 0;

    switch (cmd) {
    case FlashCmd::READ_ID:
        if (dataIn && dataInSize >= 3) {
            dataIn[0] = (m_jedecId >> 16) & 0xFF;
            dataIn[1] = (m_jedecId >> 8) & 0xFF;
            dataIn[2] = m_jedecId & 0xFF;
        }
        break;

    case FlashCmd::READ:
    case FlashCmd::READ_4B:
    case FlashCmd::FAST_READ:
    case FlashCmd::FAST_READ_4B:
        if (dataIn && dataInSize > 0) {
            uint32_t len = std::min(dataInSize, static_cast<uint32_t>(m_memory.size()) - address);
            std::memcpy(dataIn, m_memory.data() + address, len);
        }
        break;

    case FlashCmd::WRITE_ENABLE:
        m_writeEnabled = true;
        break;

    case FlashCmd::PAGE_PROGRAM:
    case FlashCmd::PAGE_PROGRAM_4B:
        if (m_writeEnabled && dataOutSize > 0) {
            // Simulate realistic page program delay (~0.5ms per page)
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            const uint8_t* src = dataOut + cmdSize + addrSize;
            uint32_t len = std::min(dataOutSize, static_cast<uint32_t>(m_memory.size()) - address);
            for (uint32_t i = 0; i < len; i++)
                m_memory[address + i] &= src[i];
            m_writeEnabled = false;
        }
        break;

    case FlashCmd::SECTOR_ERASE:
    case FlashCmd::SECTOR_ERASE_4B:
        if (m_writeEnabled) {
            // Simulate realistic erase delay (~50ms per sector)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            uint32_t base = address & ~(4096u - 1);
            uint32_t len = std::min(4096u, static_cast<uint32_t>(m_memory.size()) - base);
            std::memset(m_memory.data() + base, 0xFF, len);
            m_writeEnabled = false;
        }
        break;

    case FlashCmd::BLOCK_ERASE_64K:
    case FlashCmd::BLOCK_ERASE_64K_4B:
        if (m_writeEnabled) {
            // Simulate realistic block erase delay (~50ms per 64KB block)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            uint32_t base = address & ~(65536u - 1);
            uint32_t len = std::min(65536u, static_cast<uint32_t>(m_memory.size()) - base);
            std::memset(m_memory.data() + base, 0xFF, len);
            m_writeEnabled = false;
        }
        break;

    case FlashCmd::CHIP_ERASE:
        if (m_writeEnabled) {
            // Simulate realistic chip erase delay (~500ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::fill(m_memory.begin(), m_memory.end(), 0xFF);
            m_writeEnabled = false;
        }
        break;

    case FlashCmd::READ_STATUS_1:
        if (dataIn && dataInSize >= 1)
            dataIn[0] = 0x00;
        break;

    default:
        return -1;
    }
    return 0;
}

// Platform API – simulator
int PLATFORM_WriteReadTransaction(void* flashHandle,
                                  const uint8_t* dataOutStream,
                                  uint32_t cmdSize, uint32_t addressSize,
                                  uint32_t dataOutSize, uint32_t dummyCycles,
                                  uint8_t* dataIn, uint32_t dataInSize)
{
    (void)flashHandle;
    return getFlashSimulator().transaction(dataOutStream, cmdSize, addressSize,
                                           dataOutSize, dummyCycles, dataIn, dataInSize);
}

void* PLATFORM_FlashHandleGet() { return &getFlashSimulator(); }
void PLATFORM_FlashHandlePut(void*) {}