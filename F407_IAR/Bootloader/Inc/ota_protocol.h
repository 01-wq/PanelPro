#ifndef __OTA_PROTOCOL_H__
#define __OTA_PROTOCOL_H__

#include <stdint.h>

//业务层常量 (协议约定，PC 端必须一致)
#define OTA_BLOCK_SIZE     240
#define CMD_DATA_BLK_LEN   4

//帧常量 (传输层)
#define FRAME_HEADER    0xAA
#define FRAME_TRAILER   0x55
#define FRAME_MAX_SIZE  (OTA_BLOCK_SIZE + CMD_DATA_BLK_LEN)

//命令码
#define CMD_START       0x01
#define CMD_DATA        0x02
#define CMD_END         0x03
#define CMD_VERSION     0x10

//响应码
#define RSP_ACK         0x80
#define RSP_NACK        0x81
#define RSP_VERSION     0x82

enum FrameState
{
    FRAME_STATE_WAIT_HEADER,
    FRAME_STATE_WAIT_CMD,
    FRAME_STATE_WAIT_LEN,
    FRAME_STATE_WAIT_DATA,
    FRAME_STATE_WAIT_XOR,
    FRAME_STATE_WAIT_TRAILER
};

// OTA 协议帧结构体
typedef struct 
{
    uint8_t state;
    uint8_t cmd;
    uint16_t data_len;
    uint8_t len_idx;
    uint8_t data[FRAME_MAX_SIZE];
    uint8_t data_idx;
    uint8_t received_xor;
    uint8_t calculated_xor;
}FrameParser_t;

//解析结果(完整帧输出)
typedef struct 
{
    uint8_t cmd;
    uint16_t data_len;
    uint8_t data[FRAME_MAX_SIZE];
}OtaFrame_t;

void FrameParser_Init(FrameParser_t *fp);
int FrameParser_Feed(FrameParser_t *fp,uint8_t byte,OtaFrame_t *out);

#endif
