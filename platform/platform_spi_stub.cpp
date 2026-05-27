#include <cstdio>
#include "platform_spi.h"

[[deprecated("PLATFORM_WriteReadTransaction() is a stub — provide your implementation")]]
int PLATFORM_WriteReadTransaction(const void*     userData,
                                  const uint8_t*  dataOutStream,
                                  uint32_t        cmdSize,
                                  uint32_t        addressSize,
                                  uint32_t        dataOutSize,
                                  uint32_t        dummyCycles,
                                  uint8_t*        dataIn,
                                  uint32_t        dataInSize)
{
    (void)userData;
    (void)dataOutStream; (void)cmdSize; (void)addressSize;
    (void)dataOutSize; (void)dummyCycles; (void)dataIn; (void)dataInSize;
    return -1;
}

[[deprecated("PLATFORM_FlashHandleGet() is a stub — provide your implementation")]]
void* PLATFORM_FlashHandleGet()
{
    return nullptr;
}

[[deprecated("PLATFORM_FlashHandlePut() is a stub — provide your implementation")]]
void PLATFORM_FlashHandlePut(void* handle)
{
    (void)handle;
}

