#ifndef __OTA_MANAGER_H__
#define __OTA_MANAGER_H__

#include <stdint.h>
#include "main.h"
#include "ota_protocol.h"

typedef enum
{
    OTA_STATE_IDLE = 0,
    OTA_STATE_RECEIVING,
}OtaState_t;

typedef struct
{
    OtaState_t state;
    uint32_t file_size;
    uint32_t crc32;
    uint32_t version;
    uint32_t received;
    uint32_t expected_blk;
}OtaSession_t;

void OTA_Send_Response_Data(uint8_t rsp,const uint8_t *data,uint16_t len);
void OTA_Send_Response(uint8_t rsp);
void OTA_HandleFrame(const OtaFrame_t *frame);

#endif
