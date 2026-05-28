/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2026 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       platform_spi_sim.h
* @brief      Simulated SPI NOR flash memory for unit testing
*
* ### project spiprog
*
************************************************************************************************************/
#pragma once

#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>

/// @brief Simulated SPI NOR flash memory for unit testing.
class FlashSimulator {
public:
    FlashSimulator(uint32_t size = 16 * 1024 * 1024, uint32_t jedecId = 0xEF4018);
    void reset();
    void preload(uint32_t offset, const uint8_t* data, uint32_t length);
    const uint8_t* getData() const { return m_memory.data(); }
    uint32_t getSize() const { return static_cast<uint32_t>(m_memory.size()); }

    int transaction(const uint8_t* dataOut, uint32_t cmdSize, uint32_t addrSize,
                    uint32_t dataOutSize, uint32_t dummyCycles,
                    uint8_t* dataIn, uint32_t dataInSize);

private:
    std::vector<uint8_t> m_memory;
    uint32_t m_jedecId;
    bool m_writeEnabled = false;

    uint32_t parseAddress(const uint8_t* buf, uint32_t addrSize) const;
};

FlashSimulator& getFlashSimulator();