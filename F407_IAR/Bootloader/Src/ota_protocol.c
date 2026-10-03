#include "ota_protocol.h"

/**
 * @brief 帧解析器初始化
 * @param fp 帧解析器结构体指针
 * @return 无
 */
void FrameParser_Init(FrameParser_t *fp)
{
    fp->state = FRAME_STATE_WAIT_HEADER;
    fp->cmd = 0;
    fp->data_len = 0;
    fp->len_idx = 0;
    fp->data_idx = 0;
    fp->received_xor = 0;
    fp->calculated_xor = 0;
}

/**
 * @brief 计算校验和
 * @param data 数据指针
 * @param len 数据长度
 * @return 校验和
 */
static uint8_t calc_xor8(uint8_t *data,uint8_t len)
{
    uint8_t xor = 0;
    for(uint8_t i=0;i<len;i++)
    {
        xor ^= data[i];    
    }
    return xor;
}

/**
 * @brief 帧解析器喂入字节
 * @param fp 帧解析器结构体指针
 * @param byte 输入字节
 * @param out 输出帧结构体指针
 * @return 1:解析完成一帧,0:未完成,-1:帧错误
 */
int FrameParser_Feed(FrameParser_t *fp,uint8_t byte,OtaFrame_t *out)
{
    switch(fp->state)
    {
        case FRAME_STATE_WAIT_HEADER:
        {
            if(byte == FRAME_HEADER)
            {
                fp->state = FRAME_STATE_WAIT_CMD;
            }
            return 0;
            break;
        }
        case FRAME_STATE_WAIT_CMD:
        {
            if(byte == CMD_START || byte == CMD_DATA || byte == CMD_END || byte == CMD_VERSION)
            {
                fp->cmd = byte;
                fp->data_len = 0;
                fp->data_idx = 0;
                fp->state = FRAME_STATE_WAIT_LEN;
            }
            else
            {
                fp->state = FRAME_STATE_WAIT_HEADER;
            }
            return 0;
            break;
        }
        case FRAME_STATE_WAIT_LEN:
        {
            if(fp->len_idx == 0)
            {
                fp->data_len = byte;
                fp->len_idx++;
            }
            else if(fp->len_idx == 1)
            {
                fp->data_len |= byte<<8;
                fp->len_idx = 0;
                if(fp->data_len == 0)
                {
                    fp->state = FRAME_STATE_WAIT_XOR;
                }
                else if(fp->data_len > FRAME_MAX_SIZE)
                {
                    fp->state = FRAME_STATE_WAIT_HEADER;
                    return -1;
                }
                else
                { 
                    fp->state = FRAME_STATE_WAIT_DATA;
                }
            }
            return 0; 
            break;
        }
        case FRAME_STATE_WAIT_DATA:
        {
            fp->data[fp->data_idx++] = byte;
            if(fp->data_idx >= fp->data_len)
            {
                fp->state = FRAME_STATE_WAIT_XOR;
            }
            return 0;
            break;
        }
        case FRAME_STATE_WAIT_XOR:
        {
            fp->received_xor = byte;
            fp->calculated_xor = calc_xor8(fp->data,fp->data_len);
            if(fp->received_xor == fp->calculated_xor)
            {
                fp->state =     FRAME_STATE_WAIT_TRAILER;
                return 0;
            }
            else
            {
                fp->state = FRAME_STATE_WAIT_HEADER;
                return -1;
            }
            break;
        }
        case FRAME_STATE_WAIT_TRAILER:
        {
            if(byte == FRAME_TRAILER)
            {
                out->cmd = fp->cmd;
                out->data_len = fp->data_len;
                for(uint16_t i=0;i<fp->data_len;i++)
                {
                    out->data[i] = fp->data[i];
                }
                fp->state = FRAME_STATE_WAIT_HEADER;
                return 1;
            }
            else
            {
                fp->state = FRAME_STATE_WAIT_HEADER;
                return 0;
                
            }
            break;
        }
        default:
        {
            fp->state = FRAME_STATE_WAIT_HEADER;
            return 0;
            break;
        }
    }
}
