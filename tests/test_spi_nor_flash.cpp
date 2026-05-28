/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2026 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       test_spi_nor_flash.cpp
* @brief      Unit tests for the SPI NOR flash driver
*
* ### project spiprog
*
************************************************************************************************************/
#include <chrono>
#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

#include <gtest/gtest.h>

#ifndef TEST_ON_REAL_FLASH
#include "platform_spi_sim.h"
#endif

#include "spi_nor_flash.h"
#include "platform_spi.h"

// TEST_FLASH_OFFSET is defined via CMake's target_compile_definitions
#ifndef TEST_FLASH_OFFSET
#define TEST_FLASH_OFFSET 0x0
#endif

static constexpr uint32_t TEST_OFFSET = TEST_FLASH_OFFSET;

class SpiNorFlashTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifndef TEST_ON_REAL_FLASH
        getFlashSimulator().reset();
#endif
        void* handle = PLATFORM_FlashHandleGet();
        flash.setFlashHandle(handle);
        flash.setConfig({});
        ASSERT_TRUE(flash.detect()) << "Flash detection failed";
    }

    void TearDown() override {
        PLATFORM_FlashHandlePut(flash.getFlashHandle());
    }

    SpiNorFlash flash;
};

// ===== Identification =====

TEST_F(SpiNorFlashTest, ReadId_ReturnsNonZero)
{
    uint32_t id = 0;
    ASSERT_EQ(flash.readId(id), 0);
    EXPECT_NE(id, 0u);
    EXPECT_NE(id, 0xFFFFFFu);
}

TEST_F(SpiNorFlashTest, Detect_PopulatesFlashInfo)
{
    EXPECT_GT(flash.getInfo().totalSize, 0u);
    EXPECT_GT(flash.getInfo().jedecId, 0u);
}

// ===== Read =====

TEST_F(SpiNorFlashTest, Read_DoesNotFail)
{
    std::vector<uint8_t> buf(256);
    ASSERT_EQ(flash.read(0, buf.data(), static_cast<uint32_t>(buf.size())), 0);
}

TEST_F(SpiNorFlashTest, Read_FastRead_DoesNotFail)
{
    FlashConfig cfg{};
    cfg.useFastRead = true;
    flash.setConfig(cfg);

    std::vector<uint8_t> buf(256);
    ASSERT_EQ(flash.read(0, buf.data(), static_cast<uint32_t>(buf.size())), 0);
}

#ifndef TEST_ON_REAL_FLASH
// --- Simulator-only read tests (can preload data) ---

TEST_F(SpiNorFlashTest, Read_ErasedFlash_ReturnsAllFF)
{
    std::vector<uint8_t> buf(256);
    ASSERT_EQ(flash.read(0, buf.data(), static_cast<uint32_t>(buf.size())), 0);
    EXPECT_TRUE(std::all_of(buf.begin(), buf.end(), [](uint8_t b){ return b == 0xFF; }));
}

TEST_F(SpiNorFlashTest, Read_PreloadedData_ReturnsCorrectData)
{
    std::vector<uint8_t> expected(128);
    std::iota(expected.begin(), expected.end(), 0);
    getFlashSimulator().preload(0x1000, expected.data(), static_cast<uint32_t>(expected.size()));

    std::vector<uint8_t> buf(128);
    ASSERT_EQ(flash.read(0x1000, buf.data(), static_cast<uint32_t>(buf.size())), 0);
    EXPECT_EQ(buf, expected);
}
#endif

// ===== Write (destructive) =====

TEST_F(SpiNorFlashTest, EraseSector_ThenRead_AllFF)
{
    ASSERT_EQ(flash.eraseSector(TEST_OFFSET), 0);

    std::vector<uint8_t> buf(4096);
    ASSERT_EQ(flash.read(TEST_OFFSET, buf.data(), static_cast<uint32_t>(buf.size())), 0);
    EXPECT_TRUE(std::all_of(buf.begin(), buf.end(), [](uint8_t b){ return b == 0xFF; }));
}

TEST_F(SpiNorFlashTest, Write_SinglePage_Readback)
{
    ASSERT_EQ(flash.eraseSector(TEST_OFFSET), 0);

    std::vector<uint8_t> data(128, 0x55);
    ASSERT_EQ(flash.write(TEST_OFFSET, data.data(), static_cast<uint32_t>(data.size())), 0);

    std::vector<uint8_t> readback(128);
    ASSERT_EQ(flash.read(TEST_OFFSET, readback.data(), static_cast<uint32_t>(readback.size())), 0);
    EXPECT_EQ(readback, data);
}

TEST_F(SpiNorFlashTest, Write_CrossPageBoundary_Readback)
{
    ASSERT_EQ(flash.eraseSector(TEST_OFFSET), 0);

    std::vector<uint8_t> data(300);
    std::iota(data.begin(), data.end(), 1);
    ASSERT_EQ(flash.write(TEST_OFFSET + 200, data.data(), static_cast<uint32_t>(data.size())), 0);

    std::vector<uint8_t> readback(300);
    ASSERT_EQ(flash.read(TEST_OFFSET + 200, readback.data(), static_cast<uint32_t>(readback.size())), 0);
    EXPECT_EQ(readback, data);
}

TEST_F(SpiNorFlashTest, Write_WithoutErase_OnlyClearsBits)
{
    ASSERT_EQ(flash.eraseSector(TEST_OFFSET), 0);

    std::vector<uint8_t> initial(8, 0xF0);
    ASSERT_EQ(flash.write(TEST_OFFSET, initial.data(), static_cast<uint32_t>(initial.size())), 0);

    std::vector<uint8_t> overlay(8, 0x0F);
    ASSERT_EQ(flash.write(TEST_OFFSET, overlay.data(), static_cast<uint32_t>(overlay.size())), 0);

    std::vector<uint8_t> readback(8);
    ASSERT_EQ(flash.read(TEST_OFFSET, readback.data(), static_cast<uint32_t>(readback.size())), 0);
    std::vector<uint8_t> expected(8, 0x00);
    EXPECT_EQ(readback, expected);
}

// ===== Erase =====

TEST_F(SpiNorFlashTest, EraseBlock64K_AllFF)
{
    std::vector<uint8_t> data(256, 0x42);
    ASSERT_EQ(flash.eraseSector(TEST_OFFSET), 0);
    ASSERT_EQ(flash.write(TEST_OFFSET, data.data(), static_cast<uint32_t>(data.size())), 0);

    ASSERT_EQ(flash.eraseBlock64K(TEST_OFFSET), 0);

    std::vector<uint8_t> readback(65536);
    ASSERT_EQ(flash.read(TEST_OFFSET, readback.data(), static_cast<uint32_t>(readback.size())), 0);
    EXPECT_TRUE(std::all_of(readback.begin(), readback.end(), [](uint8_t b){ return b == 0xFF; }));
}

TEST_F(SpiNorFlashTest, EraseRegion_MultiSector)
{
    ASSERT_EQ(flash.eraseRegion(TEST_OFFSET, 3 * 4096), 0);

    std::vector<uint8_t> readback(3 * 4096);
    ASSERT_EQ(flash.read(TEST_OFFSET, readback.data(), static_cast<uint32_t>(readback.size())), 0);
    EXPECT_TRUE(std::all_of(readback.begin(), readback.end(), [](uint8_t b){ return b == 0xFF; }));
}

// ===== Reflash =====

TEST_F(SpiNorFlashTest, Reflash_IdenticalData_SkipsAll)
{
    // Setup: erase + write data via SPI (works on real hardware too)
    ASSERT_EQ(flash.eraseSector(TEST_OFFSET), 0);
    std::vector<uint8_t> data(4096, 0x55);
    ASSERT_EQ(flash.write(TEST_OFFSET, data.data(), static_cast<uint32_t>(data.size())), 0);

    // Measure: reflash with identical data should skip everything
    auto start = std::chrono::steady_clock::now();
    uint32_t skipped = 0, erased = 0;
    ASSERT_EQ(flash.reflash(TEST_OFFSET, data.data(), static_cast<uint32_t>(data.size()), skipped, erased), 0);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(skipped, 4096u);
    EXPECT_EQ(erased, 0u);

    std::cout << "  [Reflash skip time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
              << " ms]\n";
}

TEST_F(SpiNorFlashTest, Reflash_ErasedFlash_NoEraseNeeded)
{
    // Setup: just erase (flash is all 0xFF)
    ASSERT_EQ(flash.eraseSector(TEST_OFFSET), 0);

    // Measure: reflash with new data, no erase needed (0xFF -> 0x55 is AND-only)
    std::vector<uint8_t> data(4096, 0x55);
    auto start = std::chrono::steady_clock::now();
    uint32_t skipped = 0, erased = 0;
    ASSERT_EQ(flash.reflash(TEST_OFFSET, data.data(), static_cast<uint32_t>(data.size()), skipped, erased), 0);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(erased, 0u);

    std::vector<uint8_t> readback(4096);
    ASSERT_EQ(flash.read(TEST_OFFSET, readback.data(), static_cast<uint32_t>(readback.size())), 0);
    EXPECT_EQ(readback, data);

    std::cout << "  [Reflash write-only time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
              << " ms]\n";
}

TEST_F(SpiNorFlashTest, Reflash_DifferentData_ErasesAndWrites)
{
    // Setup: erase + write initial data
    ASSERT_EQ(flash.eraseSector(TEST_OFFSET), 0);
    std::vector<uint8_t> oldData(4096, 0xAA);
    ASSERT_EQ(flash.write(TEST_OFFSET, oldData.data(), static_cast<uint32_t>(oldData.size())), 0);

    // Measure: reflash with data that requires erase (0xAA -> 0x55 needs bit-setting)
    std::vector<uint8_t> newData(4096, 0x55);
    auto start = std::chrono::steady_clock::now();
    uint32_t skipped = 0, erased = 0;
    ASSERT_EQ(flash.reflash(TEST_OFFSET, newData.data(), static_cast<uint32_t>(newData.size()), skipped, erased), 0);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_GT(erased, 0u);

    std::vector<uint8_t> readback(4096);
    ASSERT_EQ(flash.read(TEST_OFFSET, readback.data(), static_cast<uint32_t>(readback.size())), 0);
    EXPECT_EQ(readback, newData);

    std::cout << "  [Reflash erase+write time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
              << " ms]\n";
}