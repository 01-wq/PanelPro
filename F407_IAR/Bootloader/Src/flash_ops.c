#include "flash_ops.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "boot_config.h"
#include "SEGGER_RTT.h"

/**
 * @brief  擦除APP区域（扇区3~7）
 * @param  None
 * @retval 0:成功 1:失败
 */
uint8_t Flash_Erase_App_Area(void)
{
    HAL_FLASH_Unlock();
    uint32_t sector_err = 0;      // ← 接收失败扇区号的变量
    FLASH_EraseInitTypeDef FLASH_EraseInitStructure = {0};
    FLASH_EraseInitStructure.TypeErase = FLASH_TYPEERASE_SECTORS;
    FLASH_EraseInitStructure.Banks = FLASH_BANK_1;
    FLASH_EraseInitStructure.Sector = FLASH_SECTOR_3;
    FLASH_EraseInitStructure.NbSectors = 5;
    FLASH_EraseInitStructure.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if(HAL_FLASHEx_Erase(&FLASH_EraseInitStructure, &sector_err) != HAL_OK)
    {
        SEGGER_RTT_printf(0, "[FLASH] sector_err=%d\r\n",(int)sector_err);
        HAL_FLASH_Lock();
        return 1;
    }
    else
    {
        HAL_FLASH_Lock();
        return 0;
    }
}

/**
 * @brief  擦除指定扇区
 * @param  sector: 扇区号（FLASH_SECTOR_0~FLASH_SECTOR_7）
 * @retval 0:成功 1:失败
 */
uint8_t MY_FLASH_Erase_Sector(uint32_t sector)
{
    HAL_FLASH_Unlock();
    uint32_t sector_err = 0;
    FLASH_EraseInitTypeDef FLASH_EraseInitStructure = {0};
    FLASH_EraseInitStructure.TypeErase = FLASH_TYPEERASE_SECTORS;
    FLASH_EraseInitStructure.Banks = FLASH_BANK_1;
    FLASH_EraseInitStructure.Sector = sector;
    FLASH_EraseInitStructure.NbSectors = 1;
    FLASH_EraseInitStructure.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if(HAL_FLASHEx_Erase(&FLASH_EraseInitStructure, &sector_err) == HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0;
    }
    else
    {
        HAL_FLASH_Lock();
        return 1;
    }
}

/**
 * @brief  向指定地址写入数据
 * @param  addr: 写入的起始地址* 
 * @param  data: 数据指针
 * @param  len: 数据长度（字节）
 * @retval 0:成功 1:失败 2:校验失败
 */
uint8_t MY_Flash_Write(uint32_t addr,const uint8_t *data,uint32_t len)
{
    HAL_FLASH_Unlock();
    for(uint32_t i = 0; i < len; i += 4)
    {
        uint32_t word = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK)
        {
            SEGGER_RTT_printf(0, "[FLASH] Program fail at i=%d, SR=0x%08X\r\n", i, (unsigned int)FLASH->SR);
            HAL_FLASH_Lock();
            return 1;  //写失败
        }
        if(*(volatile uint8_t *)(addr + i) != data[i])
        {
            HAL_FLASH_Lock();
            return 2;  //校验失败
        }
    }
    
    HAL_FLASH_Lock();
    return 0;  //写入成功
}

void Flash_Test(void)
{
    
    uint32_t APP_START_ADDR = 0x0800C000; // Example start address for application area
    uint8_t data[256] = {0};
    for(uint16_t i = 0; i < 256; i++)
    {
        data[i] = i;
    }
    uint8_t retE = Flash_Erase_App_Area();
    if(retE != 0)
    {
        SEGGER_RTT_printf(0, "[FLASH TEST] Erase ret=%d\r\n", retE);
        return;
    }
    SEGGER_RTT_printf(0, "[FLASH TEST] Erase OK\r\n");
    uint8_t v = *(volatile uint8_t *)APP_BASE;
    SEGGER_RTT_printf(0, "[FLASH TEST] After erase, APP_BASE[0] = 0x%02X(expect 0xFF)\r\n", v);
    uint8_t retW = MY_Flash_Write(APP_START_ADDR, data, 256);
    if(retW != 0)
    {
        SEGGER_RTT_printf(0, "[FLASH TEST] Write ret=%d\r\n", retW);
        return;
    }
    // 完整校验 256 字节
    uint8_t ok = 1;
    for (int i = 0; i < 256; i++)
    {
        if (*(volatile uint8_t *)(APP_BASE + i) != data[i]) 
        {
            ok = 0;
        }
    }
    SEGGER_RTT_printf(0, "[FLASH TEST] Verify: %s\r\n", ok ? "PASS" : "FAIL");
    // 回读验证关键位置
    SEGGER_RTT_printf(0, "[FLASH TEST] APP_BASE[0]=0x%02X [1]=0x%02X [100]=0x%02X [255]=0x%02X\r\n",*(volatile uint8_t *)(APP_BASE+0),*(volatile uint8_t *)(APP_BASE+1),*(volatile uint8_t *)(APP_BASE+100),*(volatile uint8_t *)(APP_BASE+255));
    // 验证 5 个扇区全部擦除
    uint32_t bases[5] = {0x0800C000, 0x08010000, 0x08020000, 0x08040000,
    0x08060000};
    for (int i = 0; i < 5; i++)
    {
        uint8_t b = *(volatile uint8_t *)bases[i];
        SEGGER_RTT_printf(0, "[FLASH TEST] Sector %d first byte = 0x%02X(expect 0xFF)\r\n", 3+i, b);
    }
}
