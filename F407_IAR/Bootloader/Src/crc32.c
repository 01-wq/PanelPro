#include "crc32.h"

static uint32_t crc32_table[256];

void CRC32_Init(void)
{
    for(uint32_t i = 0;i < 256;i++)
    {
        uint32_t c = i;
        for(int j = 0;j < 8;j++)
        {
            if(c & 1)
            {
                c = (c >> 1) ^ 0xEDB88320; //0xEDB88320 是 0x04C11DB7 的反射形式
            }
            else
            {
                c = c >> 1;
            }
        }
        crc32_table[i] = c;
    }
}

uint32_t CRC32_Calc(const uint8_t *data,uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF; //初值

    for(uint32_t i = 0;i < len;i++)
    {
        //右移8位+查表+异或
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }

    return crc ^ 0xFFFFFFFF; //末异或
}
