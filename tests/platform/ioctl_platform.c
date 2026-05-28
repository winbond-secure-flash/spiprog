
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "qlib_platform.h"

#define PRINT_ERR(fmt, ...)   printf(fmt, ##__VA_ARGS__);  

#include "bridge_common.h"


// this is needed when core reset is done by qlib at the end of the fw update sequence
void CORE_RESET(void){}// dummy

static int fd_g = -1;

int platform_spi_handle_get(void **spi_handle)
{
	int fd = open("/dev/spi_flash_bridge", O_RDWR);
    // ioctl provided by the kernel module
	ASSERT_MSG_RET( spi_handle, -1, "Error: NULL pointer to handle\n");
	ASSERT_MSG_RET( fd >= 0, -1, "Error: file for ioctl not opened\n");
	fd_g = fd;
	*spi_handle = &fd_g;
	return 0;
}

void platform_spi_handle_put(void *spi_handle)
{
	ASSERT_MSG_VOID( spi_handle, "Error: NULL pointer to handle\n");
	int fd = *((int *)spi_handle);
	ASSERT_MSG_VOID( fd == fd_g, "Error: Wrong handle to close\n");
    close(fd_g);
	fd_g = -1;
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
    if (userData == NULL) {
        fprintf(stderr, "Error: Plaftform handle is NULL\n");
        return -1;
    }

    int fd = *((int *)userData);
    
    struct bridge_transaction xfer;
    memset(&xfer, 0, sizeof(xfer));

    ASSERT_MSG_RET((cmdSize + addressSize + dataOutSize) <= MAX_SPI_BUF &&
                    dataInSize <= MAX_SPI_BUF, -1, "Error: Transfer length exceeds max %u\n", MAX_SPI_BUF);
    
    ASSERT_MSG_RET( dataInSize == 0 || ((addressSize + dataOutSize) <= 8), -1, "Error: write/read transfer (addressSize + dataOutSize) length exceeds 8 \n");

    xfer.cmd_len = cmdSize;
    xfer.addr_len = addressSize;
    xfer.data_out_len = dataOutSize;
    memcpy(xfer.tx_buf, dataOutStream, cmdSize + addressSize + dataOutSize);


    
    xfer.data_in_len = dataInSize;
    xfer.flags = flags;
    xfer.dummy_len = dummyCycles / 8; 

    int ret = ioctl(fd, IOCTL_SEND_SPI, &xfer); 
    if (ret < 0)
    {
       perror("PLAT_SPI_WriteReadTransaction failed, system ioctl function error");
       return ret;
    }

    if (xfer.status)
    {
       perror("PLAT_SPI_WriteReadTransaction failed, error from kernel functions");
       return xfer.status;
    }

    if (dataInSize > 0 && dataIn) 
    {
       memcpy(dataIn, xfer.rx_buf, dataInSize);
    }
   
    return 0;
}




