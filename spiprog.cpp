/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2026 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       spiprog.cpp
* @brief      Command-line SPI NOR flash programmer application
*
* ### project spiprog
*
************************************************************************************************************/
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>

#include "platform_spi.h"
#include "spi_nor_flash.h"

using namespace std;

static constexpr const char* SPIPROG_VERSION = "1.0.0";

/// @brief Print command-line usage information and examples.
/// @param progName The executable name (argv[0]).
static void printUsage(const char* progName)
{
    std::cout << "spiprog v" << SPIPROG_VERSION << " - SPI NOR Flash Programmer\n\n"
              << "Usage: " << progName << " [global options] <command> [command options]\n"
              << "\nGlobal options:\n"
              << "  --version                         Show version information\n"
              << "  --fast-read                       Use FAST_READ (0x0B) instead of READ (0x03)\n"
              << "  --4byte-mode                      Force 4-byte address mode\n"
              << "\nCommands:\n"
              << "  id                                 Read JEDEC ID\n"
              << "  read  <offset> <length> <file>     Read flash to file\n"
              << "  write <offset> <file> [--verify]   Write file to flash (erase+write, raw)\n"
              << "  erase <offset> <length>            Erase region\n"
              << "  erase_chip                         Erase entire chip\n"
              << "  reflash <offset> <file> [--verify]  Smart reflash (skip/erase only if needed)\n"
              << "  verify <offset> <file>             Verify flash against file\n"
              << "\nOffset and length accept hex (0x...) or decimal.\n"
              << "\nFlash detection:\n"
              << "  On startup, the tool reads the JEDEC ID to identify the flash chip.\n"
              << "  The tool defaults to 3-byte mode. Use --4byte-mode if your flash requires it.\n"
              << "\nExamples:\n"
              << "  " << progName << " id\n"
              << "      Read and display the JEDEC ID of the flash chip.\n"
              << "\n"
              << "  " << progName << " read 0x0 0x100000 dump.bin\n"
              << "      Read 1MB from the start of flash into dump.bin.\n"
              << "\n"
              << "  " << progName << " --fast-read read 0x0 0x2000000 full.bin\n"
              << "      Read entire 32MB flash using FAST_READ for higher throughput.\n"
              << "\n"
              << "  " << progName << " write 0x0 firmware.bin --verify\n"
              << "      Erase, write firmware.bin at offset 0, then verify.\n"
              << "\n"
              << "  " << progName << " reflash 0x10000 config.bin --verify\n"
              << "      Smart reflash config.bin at 64KB offset. Only erases sectors\n"
              << "      that need it, skips pages that already match.\n"
              << "\n"
              << "  " << progName << " erase 0x0 0x10000\n"
              << "      Erase 64KB (one block) at the start of flash.\n"
              << "\n"
              << "  " << progName << " erase_chip\n"
              << "      Erase the entire flash chip.\n"
              << "\n"
              << "  " << progName << " verify 0x0 firmware.bin\n"
              << "      Verify flash contents at offset 0 match firmware.bin.\n";
}

/// @brief Parse a numeric string as decimal or hexadecimal.
///
/// Accepts "0x" or "0X" prefix for hex values, otherwise parses as decimal.
/// @param s The string to parse.
/// @return The parsed 32-bit unsigned value.
static uint32_t parseNumber(const std::string& s)
{
    if (s.find("0x") == 0 || s.find("0X") == 0)
        return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
    return static_cast<uint32_t>(std::stoul(s, nullptr, 10));
}

/// @brief Print a progress indicator to stdout (overwrites current line).
/// @param current Number of bytes processed so far.
/// @param total Total number of bytes to process.
/// @param label Operation label (e.g., "Reading", "Writing").
static void printProgress(uint32_t current, uint32_t total, const std::string& label)
{
    int percent = static_cast<int>((uint64_t)current * 100 / total);
    std::cout << "\r" << label << ": " << percent << "% ("
              << current / 1024 << "/" << total / 1024 << " KB)" << std::flush;
    if (current == total) std::cout << "\n";
}

/// @brief Read and display the JEDEC ID of the connected flash device.
/// @param flash Reference to the SpiNorFlash driver instance.
/// @return 0 on success, 1 on failure.
static int cmdId(SpiNorFlash& flash)
{
    uint32_t id = 0;
    int ret = flash.readId(id);
    if (ret != 0) {
        std::cerr << "Error: Failed to read JEDEC ID\n";
        return 1;
    }
    std::cout << "JEDEC ID: 0x" << std::hex << std::setfill('0') << std::setw(6) << id << std::dec << "\n";
    std::cout << "Manufacturer: 0x" << std::hex << std::setw(2) << ((id >> 16) & 0xFF) << "\n";
    std::cout << "Device:       0x" << std::setw(4) << (id & 0xFFFF) << std::dec << "\n";

    if (flash.detect()) {
        std::cout << "Flash size:   " << flash.getInfo().totalSize / (1024 * 1024) << " MB\n";
    }
    return 0;
}

/// @brief Read a region of flash and save it to a binary file.
/// @param flash Reference to the SpiNorFlash driver instance.
/// @param offset Start address in flash.
/// @param length Number of bytes to read.
/// @param filename Output file path.
/// @return 0 on success, 1 on failure.
static int cmdRead(SpiNorFlash& flash, uint32_t offset, uint32_t length, const std::string& filename)
{
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error: Cannot open file for writing: " << filename << "\n";
        return 1;
    }

    constexpr uint32_t chunkSize = 4096;
    std::vector<uint8_t> buffer(chunkSize);
    uint32_t remaining = length;
    uint32_t addr = offset;

    while (remaining > 0) {
        uint32_t toRead = std::min(chunkSize, remaining);
        int ret = flash.read(addr, buffer.data(), toRead);
        if (ret != 0) {
            std::cerr << "\nError: Read failed at offset 0x" << std::hex << addr << std::dec << "\n";
            return 1;
        }
        outFile.write(reinterpret_cast<char*>(buffer.data()), toRead);
        addr += toRead;
        remaining -= toRead;
        printProgress(length - remaining, length, "Reading");
    }

    std::cout << "Read " << length << " bytes to " << filename << "\n";
    return 0;
}

/// @brief Verify flash contents against a reference binary file.
///
/// Reads the flash in chunks and compares byte-by-byte with the file.
/// Reports the first mismatched offset on failure.
/// @param flash Reference to the SpiNorFlash driver instance.
/// @param offset Start address in flash to compare against.
/// @param filename Reference file path to verify against.
/// @return 0 if flash matches the file, 1 on mismatch or error.
static int cmdVerify(SpiNorFlash& flash, uint32_t offset, const std::string& filename)
{
    std::ifstream inFile(filename, std::ios::binary | std::ios::ate);
    if (!inFile) {
        std::cerr << "Error: Cannot open file: " << filename << "\n";
        return 1;
    }

    uint32_t fileSize = static_cast<uint32_t>(inFile.tellg());
    inFile.seekg(0);

    constexpr uint32_t chunkSize = 4096;
    std::vector<uint8_t> fileBuf(chunkSize);
    std::vector<uint8_t> flashBuf(chunkSize);
    uint32_t remaining = fileSize;
    uint32_t addr = offset;

    while (remaining > 0) {
        uint32_t toRead = std::min(chunkSize, remaining);
        inFile.read(reinterpret_cast<char*>(fileBuf.data()), toRead);

        int ret = flash.read(addr, flashBuf.data(), toRead);
        if (ret != 0) {
            std::cerr << "\nError: Read failed at offset 0x" << std::hex << addr << std::dec << "\n";
            return 1;
        }

        if (std::memcmp(fileBuf.data(), flashBuf.data(), toRead) != 0) {
            for (uint32_t i = 0; i < toRead; i++) {
                if (fileBuf[i] != flashBuf[i]) {
                    std::cerr << "\nVerify FAILED at offset 0x" << std::hex << (addr + i)
                              << " (expected 0x" << std::setw(2) << std::setfill('0') << (int)fileBuf[i]
                              << ", got 0x" << std::setw(2) << (int)flashBuf[i] << ")\n" << std::dec;
                    return 1;
                }
            }
        }

        addr += toRead;
        remaining -= toRead;
        printProgress(fileSize - remaining, fileSize, "Verifying");
    }

    std::cout << "Verify OK (" << fileSize << " bytes match)\n";
    return 0;
}

/// @brief Erase a flash region and write a binary file to it.
///
/// Performs a full erase of the target region followed by sequential page
/// programming. Optionally verifies the written data afterwards.
/// @param flash Reference to the SpiNorFlash driver instance.
/// @param offset Start address in flash to write to.
/// @param filename Input file path containing data to program.
/// @param verify If true, verify flash contents after writing.
/// @return 0 on success, 1 on failure.
static int cmdWrite(SpiNorFlash& flash, uint32_t offset, const std::string& filename, bool verify)
{
    std::ifstream inFile(filename, std::ios::binary | std::ios::ate);
    if (!inFile) {
        std::cerr << "Error: Cannot open file: " << filename << "\n";
        return 1;
    }

    uint32_t fileSize = static_cast<uint32_t>(inFile.tellg());
    inFile.seekg(0);

    std::vector<uint8_t> data(fileSize);
    inFile.read(reinterpret_cast<char*>(data.data()), fileSize);
    inFile.close();

    // Erase the region first
    std::cout << "Erasing " << fileSize << " bytes at offset 0x" << std::hex << offset << std::dec << "...\n";
    int ret = flash.eraseRegion(offset, fileSize);
    if (ret != 0) {
        std::cerr << "Error: Erase failed\n";
        return 1;
    }

    // Write in chunks
    constexpr uint32_t chunkSize = 4096;
    uint32_t remaining = fileSize;
    uint32_t addr = offset;
    const uint8_t* ptr = data.data();

    while (remaining > 0) {
        uint32_t toWrite = std::min(chunkSize, remaining);
        ret = flash.write(addr, ptr, toWrite);
        if (ret != 0) {
            std::cerr << "\nError: Write failed at offset 0x" << std::hex << addr << std::dec << "\n";
            return 1;
        }
        addr += toWrite;
        ptr += toWrite;
        remaining -= toWrite;
        printProgress(fileSize - remaining, fileSize, "Writing");
    }

    std::cout << "Wrote " << fileSize << " bytes from " << filename << "\n";

    if (verify) {
        return cmdVerify(flash, offset, filename);
    }

    return 0;
}

/// @brief Smart reflash: only erases and writes sectors that differ from the input file.
///
/// For each sector, compares flash contents with the source data. Sectors that
/// already match are skipped entirely. Sectors requiring bit-clearing are erased
/// before programming. Reports statistics on skipped and erased sectors.
/// @param flash Reference to the SpiNorFlash driver instance.
/// @param offset Start address in flash.
/// @param filename Input file path containing data to program.
/// @param verify If true, verify flash contents after reflashing.
/// @return 0 on success, 1 on failure.
static int cmdReflash(SpiNorFlash& flash, uint32_t offset, const std::string& filename, bool verify)
{
    std::ifstream inFile(filename, std::ios::binary | std::ios::ate);
    if (!inFile) {
        std::cerr << "Error: Cannot open file: " << filename << "\n";
        return 1;
    }

    uint32_t fileSize = static_cast<uint32_t>(inFile.tellg());
    inFile.seekg(0);

    std::vector<uint8_t> data(fileSize);
    inFile.read(reinterpret_cast<char*>(data.data()), fileSize);
    inFile.close();

    constexpr uint32_t chunkSize = 4096;
    uint32_t remaining = fileSize;
    uint32_t addr = offset;
    const uint8_t* ptr = data.data();
    uint32_t totalSkipped = 0;
    uint32_t totalErased = 0;

    while (remaining > 0) {
        uint32_t toWrite = std::min(chunkSize, remaining);
        uint32_t skipped = 0;
        uint32_t erased = 0;

        int ret = flash.reflash(addr, ptr, toWrite, skipped, erased);
        if (ret != 0) {
            std::cerr << "\nError: Reflash failed at offset 0x" << std::hex << addr << std::dec << "\n";
            return 1;
        }

        totalSkipped += skipped;
        totalErased += erased;
        addr += toWrite;
        ptr += toWrite;
        remaining -= toWrite;
        printProgress(fileSize - remaining, fileSize, "Reflashing");
    }

    std::cout << "Reflashed " << fileSize << " bytes from " << filename
              << " (skipped " << totalSkipped / 1024 << " KB, erased "
              << totalErased << " sectors)\n";

    if (verify) {
        return cmdVerify(flash, offset, filename);
    }

    return 0;
}

/// @brief Erase a specified region of flash.
/// @param flash Reference to the SpiNorFlash driver instance.
/// @param offset Start address of the region to erase.
/// @param length Number of bytes to erase (rounded up to sector boundary).
/// @return 0 on success, 1 on failure.
static int cmdErase(SpiNorFlash& flash, uint32_t offset, uint32_t length)
{
    std::cout << "Erasing " << length << " bytes at offset 0x"
              << std::hex << offset << std::dec << "...\n";

    int ret = flash.eraseRegion(offset, length);
    if (ret != 0) {
        std::cerr << "Error: Erase failed\n";
        return 1;
    }

    std::cout << "Erase complete\n";
    return 0;
}

/// @brief Application entry point.
///
/// Parses global options and command arguments, initializes the platform SPI
/// interface, detects the flash device, and dispatches to the appropriate
/// command handler.
/// @param argc Argument count.
/// @param argv Argument vector.
/// @return 0 on success, 1 on failure.
int main(int argc, char* argv[])
{
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    // Parse global options before the command
    FlashConfig config;
    int argStart = 1;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--version") {
            std::cout << "spiprog v" << SPIPROG_VERSION << "\n";
            return 0;
        } else if (arg == "--fast-read") {
            config.useFastRead = true;
        } else if (arg == "--4byte-mode") {
            config.force4ByteAddr = true;
        } else {
            argStart = i;
            break;
        }
    }

    void* handle = PLATFORM_FlashHandleGet();
    if (!handle) {
        std::cerr << "Error: Failed to get flash handle\n";
        return 1;
    }

    SpiNorFlash flash;
    flash.setFlashHandle(handle);
    flash.setConfig(config);

    int result = 1;

    if (!flash.detect()) {
        std::cerr << "Error: Cannot communicate with flash (JEDEC ID read failed)\n";
        PLATFORM_FlashHandlePut(handle);
        return 1;
    }

    std::cout << "Flash detected: " << flash.getInfo().name
              << " (" << flash.getInfo().totalSize / (1024 * 1024) << " MB)\n";
    std::cout << "Address mode: " << (flash.getConfig().force4ByteAddr ? "4-byte" : "3-byte") << "\n";

    if (config.force4ByteAddr && !flash.getConfig().force4ByteAddr) {
        std::cerr << "Error: --4byte-mode requested but flash is in 3-byte address mode\n";
        PLATFORM_FlashHandlePut(handle);
        return 1;
    }

    std::string command = argv[argStart];

    if (command == "id") {
        result = cmdId(flash);
    }
    else if (command == "read" && argc >= argStart + 4) {
        uint32_t offset = parseNumber(argv[argStart + 1]);
        uint32_t length = parseNumber(argv[argStart + 2]);
        result = cmdRead(flash, offset, length, argv[argStart + 3]);
    }
    else if (command == "write" && argc >= argStart + 3) {
        uint32_t offset = parseNumber(argv[argStart + 1]);
        std::string filename = argv[argStart + 2];
        bool verify = (argc >= argStart + 4 && std::string(argv[argStart + 3]) == "--verify");
        result = cmdWrite(flash, offset, filename, verify);
    }
    else if (command == "reflash" && argc >= argStart + 3) {
        uint32_t offset = parseNumber(argv[argStart + 1]);
        std::string filename = argv[argStart + 2];
        bool verify = (argc >= argStart + 4 && std::string(argv[argStart + 3]) == "--verify");
        result = cmdReflash(flash, offset, filename, verify);
    }
    else if (command == "erase" && argc >= argStart + 3) {
        uint32_t offset = parseNumber(argv[argStart + 1]);
        uint32_t length = parseNumber(argv[argStart + 2]);
        result = cmdErase(flash, offset, length);
    }
    else if (command == "erase_chip") {
        std::cout << "Erasing entire chip...\n";
        int ret = flash.eraseChip();
        if (ret != 0) {
            std::cerr << "Error: Chip erase failed\n";
        } else {
            std::cout << "Chip erase complete\n";
            result = 0;
        }
    }
    else if (command == "verify" && argc >= argStart + 3) {
        uint32_t offset = parseNumber(argv[argStart + 1]);
        result = cmdVerify(flash, offset, argv[argStart + 2]);
    }
    else {
        printUsage(argv[0]);
    }

    PLATFORM_FlashHandlePut(handle);
    return result;
}
