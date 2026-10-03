#include "metadata.h"
#include "flash_ops.h"
#include "stm32f4xx_hal.h"
#include "SEGGER_RTT.h"

typedef struct
{
    uint32_t magic; // 应用有效标志
    uint32_t size;  // 应用大小
    uint32_t version; // 应用版本号
}Metadata_t;

/**
 * @brief  检验APP是否有效
 * @param  None
 * @retval 1:有效 0:无效
 */
uint8_t Metadata_IsValid(void)
{
    return (*(volatile uint32_t *)METADATA_BASE == APP_VALID_MAGIC) ? 1 : 0;
}

/**
* @brief  使当前 App 失效（擦除 Metadata 扇区，清掉 magic）
* @param  None
* @retval 0:成功 1:失败
*/
uint8_t Metadata_Invalidate(void)
{
    return MY_FLASH_Erase_Sector(FLASH_SECTOR_2);
}

/**
 * @brief  读取APP的大小和版本号
 * @param  size: APP大小
 * @param  version: APP版本号
 * @retval None
 */
void Metadata_Read(uint32_t *size,uint32_t *version)
{
    *size = *(volatile uint32_t*)(METADATA_BASE + 4);
    *version = *(volatile uint32_t*)(METADATA_BASE + 8);
}

/**
 * @brief  写入APP的大小和版本号
 * @param  size: APP大小
 * @param  version: APP版本号
 * @retval 0:成功 1:擦除失败 2:写入失败
 */
uint8_t Metadata_Write(uint32_t size,uint32_t version)
{
    Metadata_t meta;
    meta.magic = APP_VALID_MAGIC;
    meta.size = size;
    meta.version = version;
    if(MY_FLASH_Erase_Sector(FLASH_SECTOR_2) == 0)
    {
        if(MY_Flash_Write(METADATA_BASE,(uint8_t *)&meta,sizeof(meta)) == 0)
        { 
            return 0;
        }
        else
        {
            return 2;
        }
    }
    else
    {
        return 1;
    }
}

void Metadata_test(void)
{
    uint32_t size,version;
    uint8_t  Met_Read_ret = 0;
    uint8_t  Met_Write_ret = 0;
    Met_Write_ret = Metadata_Write(0x00040000,0x0101);
    SEGGER_RTT_printf(0,"Met_Write_ret=%d\r\n",Met_Write_ret);
    Met_Read_ret = Metadata_IsValid();
    SEGGER_RTT_printf(0,"Met_Read_ret=%d\r\n",Met_Read_ret);
    Metadata_Read(&size,&version);SEGGER_RTT_printf(0,"size=0x%02X,version=0x%02X\r\n",size,version);
    MY_FLASH_Erase_Sector(FLASH_SECTOR_2);
    Met_Read_ret = Metadata_IsValid();
    SEGGER_RTT_printf(0,"Met_Read_ret=%d\r\n",Met_Read_ret);
}
