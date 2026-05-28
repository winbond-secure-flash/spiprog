/************************************************************************************************************
* @internal
* @remark     Winbond Electronics Corporation - Confidential
* @copyright  Copyright (c) 2026 by Winbond Electronics Corporation . All rights reserved
* @endinternal
*
* @file       platform_spi.h
* @brief      Platform SPI transport layer interface declarations
*
* ### project spiprog
*
************************************************************************************************************/
#pragma once

#include <cstdint>

/// @brief Acquire an opaque handle to the flash device.
///
/// The returned handle is passed as the `userData` parameter to all
/// subsequent PLATFORM_WriteReadTransaction() calls. The caller must
/// release it with PLATFORM_FlashHandlePut() when done.
/// @return Opaque flash handle, or nullptr on failure.
void* PLATFORM_FlashHandleGet();

/// @brief Release a previously acquired flash handle.
/// @param flashHandle Handle obtained from PLATFORM_FlashHandleGet().
void PLATFORM_FlashHandlePut(void* flashHandle);

/// @brief Perform a SPI transaction with the flash device.
///
/// The output stream (`dataOutStream`) is structured as:
///   [command bytes (cmdSize)] [address bytes (addressSize)] [data out bytes (dataOutSize)]
///
/// After transmitting the output stream and any dummy cycles, the device
/// response is clocked into `dataIn`.
///
/// @param userData       Opaque flash handle from PLATFORM_FlashHandleGet().
/// @param dataOutStream  Buffer containing command + address + output data.
/// @param cmdSize        Number of command bytes (typically 1).
/// @param addressSize    Number of address bytes (0, 3, or 4).
/// @param dataOutSize    Number of data bytes to send after the address.
/// @param dummyCycles    Number of dummy clock cycles between output and input phases.
/// @param[out] dataIn    Buffer to receive data from the device (may be nullptr if dataInSize is 0).
/// @param dataInSize     Number of bytes to read from the device.
/// @return 0 on success, non-zero on failure.
int PLATFORM_WriteReadTransaction(const void*     userData,
                                  const uint8_t*  dataOutStream,
                                  uint32_t        cmdSize,
                                  uint32_t        addressSize,
                                  uint32_t        dataOutSize,
                                  uint32_t        dummyCycles,
                                  uint8_t*        dataIn,
                                  uint32_t        dataInSize);