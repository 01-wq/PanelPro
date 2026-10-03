/*********************************************************************
 *                    SEGGER Microcontroller GmbH                     *
 *                        The Embedded Experts                        *
 **********************************************************************
 *                                                                    *
 *            (c) 1995 - 2021 SEGGER Microcontroller GmbH             *
 *                                                                    *
 *       www.segger.com     Support: support@segger.com               *
 *                                                                    *
 **********************************************************************
 *                                                                    *
 *       SEGGER RTT * Real Time Transfer for embedded targets         *
 *                                                                    *
 **********************************************************************
 *                                                                    *
 * All rights reserved.                                               *
 *                                                                    *
 * SEGGER strongly recommends to not make any changes                 *
 * to or modify the source code of this software in order to stay     *
 * compatible with the RTT protocol and J-Link.                       *
 *                                                                    *
 * Redistribution and use in source and binary forms, with or         *
 * without modification, are permitted provided that the following    *
 * condition is met:                                                  *
 *                                                                    *
 * o Redistributions of source code must retain the above copyright   *
 *   notice, this condition and the following disclaimer.             *
 *                                                                    *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND             *
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,        *
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF           *
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE           *
 * DISCLAIMED. IN NO EVENT SHALL SEGGER Microcontroller BE LIABLE FOR *
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR           *
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT  *
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;    *
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF      *
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT          *
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE  *
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF        *
 * SUCH DAMAGE.                                                       *
 *                                                                    *
 **********************************************************************
 */

#ifndef SEGGER_RTT_H
#define SEGGER_RTT_H

#include "SEGGER_RTT_Conf.h"

/*********************************************************************
 *
 *       Defines, defaults
 *
 **********************************************************************
 */
#ifndef BUFFER_SIZE_UP
  #define BUFFER_SIZE_UP                              (1024)
#endif
#ifndef BUFFER_SIZE_DOWN
  #define BUFFER_SIZE_DOWN                            (16)
#endif
#ifndef SEGGER_RTT_MAX_NUM_UP_BUFFERS
  #define SEGGER_RTT_MAX_NUM_UP_BUFFERS               (3)
#endif
#ifndef SEGGER_RTT_MAX_NUM_DOWN_BUFFERS
  #define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS             (3)
#endif
#ifndef SEGGER_RTT_MODE_DEFAULT
  #define SEGGER_RTT_MODE_DEFAULT                     SEGGER_RTT_MODE_NO_BLOCK_SKIP
#endif
#ifndef SEGGER_RTT_PRINTF_BUFFER_SIZE
  #define SEGGER_RTT_PRINTF_BUFFER_SIZE               (64u)
#endif

/*********************************************************************
 *
 *       RTT Control Block
 *
 **********************************************************************
 */
typedef struct {
  const char        *sName;                                           // Optional name. Standard names: "Terminal", "SysView", "J-Scope_t4i4"
  char              *pBuffer;                                         // Pointer to start of buffer
  unsigned int       SizeOfBuffer;                                    // Buffer size in bytes. Note that one byte is lost, as this implementation does not fill up the buffer in order to avoid the problem of being unable to distinguish between full and empty.
  unsigned int       WrOff;                                           // Position of next item to be written by either target.
  volatile unsigned int RdOff;                                        // Position of next item to be read by host. Must be volatile since it may be modified by host.
  unsigned int       Flags;                                           // Contains configuration flags
} SEGGER_RTT_BUFFER_UP;

typedef struct {
  const char        *sName;                                           // Optional name. Standard names: "Terminal", "SysView", "J-Scope_t4i4"
  char              *pBuffer;                                         // Pointer to start of buffer
  unsigned int       SizeOfBuffer;                                    // Buffer size in bytes.
  volatile unsigned int WrOff;                                        // Position of next item to be written by host. Must be volatile since it may be modified by host.
  unsigned int       RdOff;                                           // Position of next item to be read by target (down-buffer).
  unsigned int       Flags;                                           // Contains configuration flags
} SEGGER_RTT_BUFFER_DOWN;

typedef struct {
  char                    acID[16];                                   // Initialized to "SEGGER RTT"
  int                     MaxNumUpBuffers;                            // Initialized to SEGGER_RTT_MAX_NUM_UP_BUFFERS
  int                     MaxNumDownBuffers;                          // Initialized to SEGGER_RTT_MAX_NUM_DOWN_BUFFERS
  SEGGER_RTT_BUFFER_UP    aUp[SEGGER_RTT_MAX_NUM_UP_BUFFERS];       // Up buffers, transferring information up from target via debug probe to host
  SEGGER_RTT_BUFFER_DOWN  aDown[SEGGER_RTT_MAX_NUM_DOWN_BUFFERS];   // Down buffers, transferring information down from host via debug probe to target
} SEGGER_RTT_CB;

/*********************************************************************
 *
 *       RTT API functions
 *
 **********************************************************************
 */
#ifdef __cplusplus
  extern "C" {
#endif

int          SEGGER_RTT_AllocDownBuffer         (const char *sName, void *pBuffer, unsigned int BufferSize, unsigned int Flags);
int          SEGGER_RTT_AllocUpBuffer           (const char *sName, void *pBuffer, unsigned int BufferSize, unsigned int Flags);
int          SEGGER_RTT_ConfigUpBuffer          (unsigned int BufferIndex, const char *sName, void *pBuffer, unsigned int BufferSize, unsigned int Flags);
int          SEGGER_RTT_ConfigDownBuffer        (unsigned int BufferIndex, const char *sName, void *pBuffer, unsigned int BufferSize, unsigned int Flags);
int          SEGGER_RTT_GetKey                  (void);
unsigned int SEGGER_RTT_HasData                 (unsigned int BufferIndex);
int          SEGGER_RTT_HasKey                  (void);
unsigned int SEGGER_RTT_Read                    (unsigned int BufferIndex, void *pData, unsigned int BufferSize);
unsigned int SEGGER_RTT_ReadNoLock              (unsigned int BufferIndex, void *pData, unsigned int BufferSize);
int          SEGGER_RTT_SetNameDownBuffer       (unsigned int BufferIndex, const char *sName);
int          SEGGER_RTT_SetNameUpBuffer         (unsigned int BufferIndex, const char *sName);
int          SEGGER_RTT_SetFlagsDownBuffer      (unsigned int BufferIndex, unsigned int Flags);
int          SEGGER_RTT_SetFlagsUpBuffer        (unsigned int BufferIndex, unsigned int Flags);
int          SEGGER_RTT_WaitKey                 (void);
unsigned int SEGGER_RTT_Write                   (unsigned int BufferIndex, const void *pBuffer, unsigned int NumBytes);
unsigned int SEGGER_RTT_WriteNoLock             (unsigned int BufferIndex, const void *pBuffer, unsigned int NumBytes);
unsigned int SEGGER_RTT_WriteSkipNoLock         (unsigned int BufferIndex, const void *pBuffer, unsigned int NumBytes);
unsigned int SEGGER_RTT_ASM_WriteSkipNoLock     (unsigned int BufferIndex, const void *pBuffer, unsigned int NumBytes);
unsigned int SEGGER_RTT_WriteString             (unsigned int BufferIndex, const char *s);
void         SEGGER_RTT_WriteStringLocked       (unsigned int BufferIndex, const char *s);
int          SEGGER_RTT_printf                  (unsigned int BufferIndex, const char *sFormat, ...);
int          SEGGER_RTT_vprintf                 (unsigned int BufferIndex, const char * sFormat, va_list *pParamList);
unsigned int SEGGER_RTT_PutChar                 (unsigned int BufferIndex, char c);

#define      SEGGER_RTT_MODE_NO_BLOCK_SKIP      (0U)
#define      SEGGER_RTT_MODE_NO_BLOCK_TRIM      (1U)
#define      SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL (2U)

#ifdef __cplusplus
  }
#endif

#endif  // SEGGER_RTT_H
