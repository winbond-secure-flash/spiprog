#pragma once

#include <cstdint>

// User must implement: returns an opaque handle to the flash device.
// This handle is forwarded as 'userData' to every PLAT_SPI_WriteReadTransaction() call.
void* PLATFORM_FlashHandleGet();

void PLATFORM_FlashHandlePut(void* flashHandle);

// User must implement: performs a SPI transaction to the flash device.
int PLATFORM_WriteReadTransaction(const void*     userData,
                                  const uint8_t*  dataOutStream,
                                  uint32_t        cmdSize,
                                  uint32_t        addressSize,
                                  uint32_t        dataOutSize,
                                  uint32_t        dummyCycles,
                                  uint8_t*        dataIn,
                                  uint32_t        dataInSize);