#include "ota_manager.h"                           
#include "usart.h"
#include "metadata.h"
#include "ota_protocol.h"
#include "boot_config.h"
#include "flash_ops.h"
#include "crc32.h"

static uint8_t ota_response_buffer[256]; // OTA响应数据缓冲区

static OtaSession_t ota;

/**
 * @brief 计算校验和
 * @param data 数据指针
 * @param len 数据长度
 * @return 校验和
 */
static uint8_t calc_xor8(uint8_t *data,uint16_t len)
{
    uint8_t xor = 0;
    for(uint16_t i=0;i<len;i++)
    {
        xor ^= data[i];    
    }
    return xor;
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

void OTA_Send_Response_Data(uint8_t rsp,const uint8_t *data,uint16_t len)
{
    uint8_t *frame = ota_response_buffer; // 响应帧缓冲区，包含头部和数据部分
    
    // 处理OTA响应数据的逻辑
    frame[0] = 0xAA;
    frame[1] = rsp;
    frame[2] = len & 0xFF;
    frame[3] = len >> 8;
    if(len > 0)
    {
        for(uint16_t i = 0; i < len; i++)
        {
            frame[4 + i] = data[i]; // 将数据部分复制到响应帧中
        }
    }
    
    uint8_t XOR = calc_xor8(&frame[4],len); // 计算校验和，从第4个字节开始计算

    frame[4 + len] = XOR; // 将校验和放入响应帧中
    frame[5 + len] = 0x55; // 响应帧尾部

    HAL_UART_Transmit(&huart2, frame, 6 + len, 100); // 通过UART发送响应帧
}

void OTA_Send_Response(uint8_t rsp)
{
    OTA_Send_Response_Data(rsp, NULL, 0); // 调用OTA响应数据函数，发送无数据的响应
}

void OTA_HandleFrame(const OtaFrame_t *frame)
{
    // 合法性检查：只有 CMD_START / CMD_VERSION 与状态无关；
    // 其余命令（CMD_DATA / CMD_END）必须在 RECEIVING 状态才合法。
    // 默认拒绝——以后新增的命令若不在这里显式放行，会被这道检查挡住。
    if(frame->cmd != CMD_START && frame->cmd != CMD_VERSION
       && ota.state != OTA_STATE_RECEIVING)
    {
        OTA_Send_Response(RSP_NACK);
        return;
    }

    switch (frame->cmd)
    {
        case CMD_VERSION:
        {
            if(Metadata_IsValid() == 1)
            {
                uint32_t size,version;
                Metadata_Read(&size,&version);
                  
                uint8_t ver[4];
                wr_u32(ver,version);
                OTA_Send_Response_Data(RSP_VERSION,ver,4);
            }
            else
            {
                OTA_Send_Response(RSP_NACK);
            }
            break;
        }    
        case CMD_START:
        {
            if(frame->data_len != 12)
            {
                OTA_Send_Response(RSP_NACK);
                break;
            }

            //使用小端模式
            uint32_t fw_size = rd_u32(frame->data);
            uint32_t fw_crc = rd_u32(frame->data + 4);
            uint32_t fw_ver = rd_u32(frame->data + 8);
            if(fw_size == 0 || fw_size > APP_SIZE)
            {
                OTA_Send_Response(RSP_NACK);
                break;
            }
            
            ota.file_size = fw_size;
            ota.crc32 = fw_crc;
            ota.version = fw_ver;
            ota.received = 0;
            ota.expected_blk = 1;
            ota.state = OTA_STATE_RECEIVING;

            Metadata_Invalidate();
            Flash_Erase_App_Area();
            OTA_Send_Response(RSP_ACK);
            break;
        }
        case CMD_DATA:
        {
            uint32_t blk = rd_u32(frame->data);
            if(ota.received + (frame->data_len - CMD_DATA_BLK_LEN) > ota.file_size)
            {
                OTA_Send_Response(RSP_NACK);
                break;
            }
            
            if(blk == ota.expected_blk)
            {
                if(frame->data_len <= CMD_DATA_BLK_LEN)
                {
                    OTA_Send_Response(RSP_NACK);
                    break;      
                }
                else
                {
                    MY_Flash_Write(APP_BASE + (blk - 1) * OTA_BLOCK_SIZE,frame->data + CMD_DATA_BLK_LEN,frame->data_len - CMD_DATA_BLK_LEN);
                    ota.expected_blk++;
                    ota.received += frame->data_len - CMD_DATA_BLK_LEN;
                    OTA_Send_Response(RSP_ACK);
                    break;
                }
            }
            else if(blk == ota.expected_blk - 1)
            {
                if(frame->data_len <= CMD_DATA_BLK_LEN)
                {
                    OTA_Send_Response(RSP_NACK);
                    break;
                }
                else
                {
                    OTA_Send_Response(RSP_ACK);
                    break;
                }
            }
            else
            {
                OTA_Send_Response(RSP_NACK);
                break;
            }
        }
        case CMD_END:
        {
            if(ota.received != ota.file_size)
            {
                OTA_Send_Response(RSP_NACK);
                break;
            }

            uint32_t actual_crc = CRC32_Calc((const uint8_t *)APP_BASE,ota.file_size);
            if(actual_crc != ota.crc32)
            {
                OTA_Send_Response(RSP_NACK);
                break;
            }
            if(Metadata_Write(ota.file_size,ota.version) == 0)
            {
                OTA_Send_Response(RSP_ACK);

                NVIC_SystemReset();
                break;
            }
            else
            {
                OTA_Send_Response(RSP_NACK);
                break;
            }
        }
        default:
        {
            OTA_Send_Response(RSP_NACK);
            break;
        }
    }
}
