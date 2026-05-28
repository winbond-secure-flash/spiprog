
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include "qlib_platform.h"
#include <string.h>
#include <stdio.h>

// this is needed when core reset is done by qlib at the end of the fw update sequence. 
// If you do not want this behaviour leave it empty 
void CORE_RESET(void){}// dummy
int platform_spi_handle_get(void **spi_handle)
{

   #warning "Implement platform_spi_handle_get callback here"
    return 0;
}

void platform_spi_handle_put(void *spi_handle)
{
    #warning "Implement platform_spi_handle_put callback here"
        
}

int PLAT_SPI_WriteReadTransaction(const void*     userData,
                                  QLIB_BUS_MODE_T format,
                                  uint32_t        flags,
                                  const uint8_t*  dataOutStream,
                                  uint32_t        cmdSize,
                                  uint32_t        addressSize,
                                  uint32_t        dataOutSize,
                                  uint32_t        dummyCycles,
                                  uint8_t*        dataIn,
                                  uint32_t        dataInSize)
{
    

    // SPI_DEVICE_HANDLE *dev = (SPI_DEVICE_HANDLE *)userData;
    // dev is a flash handle passed to qlib instance during initialization
    // use it to implement your spi transaction. It also can not be used if you do not need it since only one
    // flash is in system and your way of talking to it is constant

#warning "Implement spi transaction callback here"    
    return 0;
}




