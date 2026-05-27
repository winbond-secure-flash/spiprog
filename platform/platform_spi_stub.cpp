#include "platform_spi.h"


int PLATFORM_WriteReadTransaction(const void*     userData,
                                  const uint8_t*  dataOutStream,
                                  uint32_t        cmdSize,
                                  uint32_t        addressSize,
                                  uint32_t        dataOutSize,
                                  uint32_t        dummyCycles,
                                  uint8_t*        dataIn,
                                  uint32_t        dataInSize)
{
#pragma message("WARNING: Building PLATFORM_WriteReadTransaction with stub - provide one for your platform ")

    (void)userData;
    (void)dataOutStream; (void)cmdSize; (void)addressSize;
    (void)dataOutSize; (void)dummyCycles; (void)dataIn; (void)dataInSize;
    return -1;
}

void* PLATFORM_FlashHandleGet()
{
#pragma message("WARNING: Building PLATFORM_FlashHandleGet with stub - provide one for your platform if needed")

    return nullptr;
}

void PLATFORM_FlashHandlePut(void* handle)
{
#pragma message("WARNING: Building PLATFORM_FlashHandlePut wiht stub - provide one for your platform if needed")

    (void)handle;
}

