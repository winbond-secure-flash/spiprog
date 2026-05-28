/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                                INCLUDES                                                 */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

#include "ftd2xx.h"
#include "libft4222.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>


#define FLASH_DBG_ERROR(fmt, ...)       printf(fmt, ##__VA_ARGS__)
#define FLASH_DBG_INFO(fmt, ...)        //printf(fmt, ##__VA_ARGS__)
#define FLASH_DBG_DEBUG(fmt, ...)       // printf(fmt, ##__VA_ARGS__)



#define FT_DEVICE_TYPE_TO_STR(type) (\
        type == FT_DEVICE_BM ? "FT_DEVICE_BM" : \
        type == FT_DEVICE_AM ? "FT_DEVICE_AM" : \
        type == FT_DEVICE_100AX ? "FT_DEVICE_100AX" : \
        type == FT_DEVICE_UNKNOWN ? "FT_DEVICE_UNKNOWN" : \
        type == FT_DEVICE_2232C ? "FT_DEVICE_2232C" : \
        type == FT_DEVICE_232R ? "FT_DEVICE_232R" : \
        type == FT_DEVICE_2232H ? "FT_DEVICE_2232H" : \
        type == FT_DEVICE_4232H ? "FT_DEVICE_4232H" : \
        type == FT_DEVICE_232H ? "FT_DEVICE_232H" : \
        type == FT_DEVICE_X_SERIES ? "FT_DEVICE_X_SERIES" : \
        type == FT_DEVICE_4222H_0 ? "FT_DEVICE_4222H_0" : \
        type == FT_DEVICE_4222H_1_2 ? "FT_DEVICE_4222H_1_2" : \
        type == FT_DEVICE_4222H_3 ? "FT_DEVICE_4222H_3" : \
        type == FT_DEVICE_4222_PROG ? "FT_DEVICE_4222_PROG" : \
        type == FT_DEVICE_900 ? "FT_DEVICE_900" : \
        type == FT_DEVICE_UMFTPD3A ? "FT_DEVICE_UMFTPD3A" : \
        "UNKNOWN TYPE")

#define FT_DEVICE_STATUS_TO_STR(status) (\
        status == FT_OK ? "FT_OK" : \
        status == FT_INVALID_HANDLE ? "FT_INVALID_HANDLE" : \
        status == FT_DEVICE_NOT_FOUND ? "FT_DEVICE_NOT_FOUND" : \
        status == FT_DEVICE_NOT_OPENED ? "FT_DEVICE_NOT_OPENED" : \
        status == FT_IO_ERROR ? "FT_IO_ERROR" : \
        status == FT_INSUFFICIENT_RESOURCES ? "FT_INSUFFICIENT_RESOURCES" : \
        status == FT_INVALID_PARAMETER ? "FT_INVALID_PARAMETER" : \
        status == FT_INVALID_BAUD_RATE ? "FT_INVALID_BAUD_RATE" : \
        status == FT_DEVICE_NOT_OPENED_FOR_ERASE ? "FT_DEVICE_NOT_OPENED_FOR_ERASE" : \
        status == FT_DEVICE_NOT_OPENED_FOR_WRITE ? "FT_DEVICE_NOT_OPENED_FOR_WRITE" : \
        status == FT_FAILED_TO_WRITE_DEVICE ? "FT_FAILED_TO_WRITE_DEVICE" : \
        status == FT_EEPROM_READ_FAILED ? "FT_EEPROM_READ_FAILED" : \
        status == FT_EEPROM_WRITE_FAILED ? "FT_EEPROM_WRITE_FAILED" : \
        status == FT_EEPROM_ERASE_FAILED ? "FT_EEPROM_ERASE_FAILED" : \
        status == FT_EEPROM_NOT_PRESENT ? "FT_EEPROM_NOT_PRESENT" : \
        status == FT_EEPROM_NOT_PROGRAMMED ? "FT_EEPROM_NOT_PROGRAMMED" : \
        status == FT_INVALID_ARGS ? "FT_INVALID_ARGS" : \
        status == FT_NOT_SUPPORTED ? "FT_NOT_SUPPORTED" : \
        status == FT_OTHER_ERROR ? "FT_OTHER_ERROR" : \
        status == FT_DEVICE_LIST_NOT_READY ? "FT_DEVICE_LIST_NOT_READY" : \
        "UNKNOWN STATUS")


FT4222_ClockRate systemClock_g;
FT4222_SPIClock  spiDivider_g;

#define FLASH_DEVICES_MAX 10

void CORE_RESET(void) {}


FT_STATUS FT4222H_SPI_Enable(bool enable, FT_HANDLE *ft_handles, size_t *devicesNum)
{
    unsigned int i = 0;
    FT_STATUS    ftStatus = 0;
    DWORD        ftdiDevs = 0;
    int          devCounter = 0;
    FT_HANDLE    ftHandle = (FT_HANDLE)NULL;

    if (NULL == ft_handles || NULL == devicesNum)
    {
        FLASH_DBG_ERROR("Bad parameters provided\n");
        return FT_INVALID_PARAMETER;
    }

    // finit flow
    systemClock_g = SYS_CLK_80;
    spiDivider_g = CLK_DIV_32;

    if (enable == false )
    {
        size_t finitHandleNum = *devicesNum;

        if (finitHandleNum >= FLASH_DEVICES_MAX)
        {
            FLASH_DBG_ERROR("Bad spi handles array provided during finit flow\n");
            return FT_INVALID_PARAMETER;
        }

        for ( i = 0; i < finitHandleNum; i++ )
        {
            FT_HANDLE finitHandle = ft_handles[i];
            
            ftStatus =  FT4222_UnInitialize(finitHandle);
            if ( ftStatus != FT4222_OK )
            {
                FLASH_DBG_ERROR("Could not uninitialize ft handle %p \n", finitHandle);
                return ftStatus;
            }

            ftStatus = FT_Close(finitHandle);
            if ( ftStatus != FT4222_OK )
            {
                FLASH_DBG_ERROR("Could not close ft handle %p \n", finitHandle);
                return ftStatus;
            }

            FLASH_DBG_INFO("Handle %p closed successfully \n", finitHandle);
        }
        return ftStatus;
    }


    // get list of all FTDI devices
    ftStatus = FT_CreateDeviceInfoList(&ftdiDevs);
    if (FT_OK != ftStatus )
    {
        FLASH_DBG_ERROR("Could not create a list of FTDI devices\n");
        return ftStatus;
    }

    FT_DEVICE_LIST_INFO_NODE devInfo[FLASH_DEVICES_MAX *4]; // FT4222H can present up to 4 interfaces

    // Populate the list of info nodes
    ftStatus = FT_GetDeviceInfoList(devInfo, &ftdiDevs);
    if (ftStatus != FT_OK)
    {
        FLASH_DBG_ERROR("FT_GetDeviceInfoList failed (error code %d)\n",(int)ftStatus);
        return ftStatus;
    }

    // Iterate over devices and find ours
    for (i = 0; i < ftdiDevs; i++)
    {
        if (devInfo[i].Type != FT_DEVICE_4222H_0)
        {
            FLASH_DBG_ERROR("FT4222H is in mode %s, only mode %s is supported \n",
                             FT_DEVICE_TYPE_TO_STR(devInfo[i].Type), FT_DEVICE_TYPE_TO_STR(FT_DEVICE_4222H_0));
            return FT_NOT_SUPPORTED;
        }

        if (0 != memcmp(devInfo[i].Description, "FT4222 A", 9))
        {
            FLASH_DBG_DEBUG("Skipping interface num %d - %s \n", i, devInfo[i].Description);
            continue;
        }

        FLASH_DBG_INFO("FT4222H USB-SPI chip found at device number %d\n", i);

        if (devCounter >= FLASH_DEVICES_MAX )
        {
            FLASH_DBG_ERROR("Found too many devices, only %d are supported\n", FLASH_DEVICES_MAX);
            return FT_INSUFFICIENT_RESOURCES;
        }

        // now it is our chip, let's configure it with the rest parameters

        ftStatus = FT_OpenEx((PVOID)(uintptr_t)devInfo[i].LocId, FT_OPEN_BY_LOCATION,  &ftHandle);
        if (FT_OK != ftStatus)
        {
            FLASH_DBG_ERROR("Open a FT4222 device failed\n");
            return ftStatus;
        }

        FLASH_DBG_DEBUG("Setting FT4222 system clock to %d\n", systemClock_g);
        ftStatus = FT4222_SetClock(ftHandle, systemClock_g);
        if (FT_OK != ftStatus)
        {
            FLASH_DBG_ERROR("set system clock failed\n");
            return ftStatus;
        }

        FLASH_DBG_DEBUG("Setting FT4222 divider to %d\n", spiDivider_g);
        ftStatus = FT4222_SPIMaster_Init(ftHandle, SPI_IO_SINGLE, spiDivider_g, CLK_IDLE_LOW, CLK_LEADING, 0x01);
        if (FT_OK != ftStatus)
        {
            FLASH_DBG_ERROR("Init FT4222 as SPI master device failed\n");
            return ftStatus;
        }

        ftStatus = FT4222_SPI_SetDrivingStrength(ftHandle, DS_8MA, DS_8MA, DS_8MA);
        if (FT_OK != ftStatus)
        {
            FLASH_DBG_ERROR("set spi driving strength failed\n");
            return ftStatus;
        }

        ft_handles[devCounter] = ftHandle;

        devCounter++;
    }

    *devicesNum = devCounter;
    if (devCounter > 0)
    {
        FLASH_DBG_INFO("Found %d ft4222h usb-spi devices\n", devCounter);
        return FT_OK;
    }
    else
    {
        FLASH_DBG_ERROR("Did not find any ft4222h usb-spi devices\n");
        return FT_DEVICE_NOT_FOUND;
    }
}

void* PLATFORM_FlashHandleGet()
{
	FT_HANDLE ft_handles[5];
	size_t devicesNum = 0;
	FT_STATUS ret = FT4222H_SPI_Enable(true, ft_handles, &devicesNum);
	if (FT_OK != ret)
	{
		FLASH_DBG_ERROR("PLATFORM_FlashHandleGet failed with %s \n", FT_DEVICE_STATUS_TO_STR(ret));
		return NULL;
	}
	
    if (0 == devicesNum)
	{
		FLASH_DBG_ERROR("No ftdi devices found\n");
		return NULL;
	}
	
	return ft_handles[0];
}



void PLATFORM_FlashHandlePut(void* flashHandle)
{
	FT_HANDLE ft_handles[5];
	size_t devicesNum = 1;
	ft_handles[0] = (FT_HANDLE)flashHandle;
	FT_STATUS ret = FT4222H_SPI_Enable(false, ft_handles, &devicesNum);	
	if (FT_OK != ret)
	{
		FLASH_DBG_ERROR("PLATFORM_FlashHandlePut failed with %s \n", FT_DEVICE_STATUS_TO_STR(ret));
	}
}


int PLATFORM_WriteReadTransaction( void                *flashHandle,
                                   const uint8_t*       dataOutStream,
                                   uint32_t             cmdSize,
                                   uint32_t             addressSize,
                                   uint32_t             dataOutSize,
                                   uint32_t             dummyCycles,
                                   uint8_t*             dataIn,
                                   uint32_t             dataInSize)
{
    uint16_t sizeTransferred = 0;
    uint16_t sizeToSend;
    uint16_t dummyBytes;
    uint8_t* dataOut = NULL;

    if (dataOutSize > 0)
    {
        dataOut = ((uint8_t * )dataOutStream ) + cmdSize + addressSize;
    }
    
    dummyBytes = (uint16_t)(dummyCycles >> 3); // Convert bits to bytes in Single SPI

    // Single SPI
    sizeToSend = (uint16_t)(cmdSize + addressSize + dummyBytes);

    if (0 == dataOutSize && 0 == dataInSize)
    {        
        if (FT4222_OK != FT4222_SPIMaster_SingleWrite(flashHandle, ( uint8 *) dataOutStream, sizeToSend, &sizeTransferred, 1))
        {
            printf("Failed FT4222_SPIMaster_SingleWrite\n");
            return -1;
        }
    }
    else
    {        
        if (FT4222_OK != FT4222_SPIMaster_SingleWrite(flashHandle, (uint8*) dataOutStream, sizeToSend, &sizeTransferred, 0))
        {
            printf("Failed FT4222_SPIMaster_SingleWrite\n");
            return -1;
        }
    }
    
    if (sizeToSend != sizeTransferred)
    {
        printf("Sizes of sent data unalligned\n");
        return -1;
    }

    // transmit data
    if (0 != dataOutSize)
    {
        if (0 == dataInSize)
        {
            // data out is the end of this transaction
            if (FT4222_OK !=  FT4222_SPIMaster_SingleWrite(flashHandle, (uint8_t *)dataOut, (uint16_t)dataOutSize, &sizeTransferred, 1))
            {
                printf("Failed FT4222_SPIMaster_SingleWrite\n");
                return -1;
            }
        }
        else
        {
            if (FT4222_OK != FT4222_SPIMaster_SingleWrite(flashHandle, (uint8_t *)dataOut, (uint16_t)dataOutSize, &sizeTransferred, 0))
            {
                printf("Failed FT4222_SPIMaster_SingleWrite\n");
                return -1;
            }
        }

        //QLIB_ASSERT_RET(dataOutSize == sizeTransferred, QLIB_STATUS__HARDWARE_FAILURE);
        if (dataOutSize != sizeTransferred)
        {
            printf("Sizes of sent data unalligned\n");
            return -1;
        }
    }

    // receive data
    if (0 != dataInSize)
    {
        if (FT4222_OK != FT4222_SPIMaster_SingleRead(flashHandle, dataIn, (uint16_t)dataInSize, &sizeTransferred, 1))
        {
            printf("Failed FT4222_SPIMaster_SingleRead\n");
            return -1;
        }
        
        if (dataInSize != sizeTransferred)
        {
            printf("Sizes of sent data unalligned\n");
            return -1;
        }
    }

    return 0;
}


int platform_spi_handle_get(void **spi_handle)
{

    FT_HANDLE ft_handles[FLASH_DEVICES_MAX];
    bool enable = true;
    size_t devicesNum = 0;

    if (spi_handle == NULL)
    {
        printf("null spi handle provided \n");
        return -1;
    }

    FT_STATUS ftStatus = FT4222H_SPI_Enable( enable, ft_handles, &devicesNum);
    if (FT4222_OK != ftStatus)
    {
        printf("FT4222H_SPI_Enable(true) failed \n");
        return -1;
    }
        
    if (devicesNum == 0)
    {
        printf("no devices found \n");
        return -1;
    }
    
    *spi_handle = ft_handles[0];
    return 0;
}

void platform_spi_handle_put(void *spi_handle)
{
    FT_HANDLE ft_handles[FLASH_DEVICES_MAX];
    bool enable = false;
    size_t devicesNum = 1;

    if (spi_handle == NULL)
    {
        printf("null spi handle provided \n");
        return;
    }

    ft_handles[0] = spi_handle;
    FT_STATUS ftStatus = FT4222H_SPI_Enable( enable, ft_handles, &devicesNum);
    if (FT4222_OK != ftStatus)
    {
        printf("FT4222H_SPI_Enable(false) failed \n");
        return;
    }
        
}



