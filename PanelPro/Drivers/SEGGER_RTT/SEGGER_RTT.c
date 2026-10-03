/*********************************************************************
 *                    SEGGER Microcontroller GmbH                     *
 *                        The Embedded Experts                        *
 **********************************************************************
 *                                                                    *
 *            (c) 1995 - 2021 SEGGER Microcontroller GmbH             *
 **********************************************************************
 *                                                                    *
 *       www.segger.com     Support: support@segger.com               *
 *                                                                    *
 **********************************************************************
 *                                                                    *
 *       SEGGER RTT * Real Time Transfer for embedded targets         *
 *                                                                    *
 **********************************************************************
 *
 * All rights reserved.
 *
 * -------------------------------------------------------------------
 *
 * This is a minimal-but-functional implementation of SEGGER RTT
 * for STM32F4 + Keil MDK.
 *
 * How it works:
 *   - A control block (SEGGER_RTT_CB) with magic ID "SEGGER RTT" is
 *     placed in RAM. J-Link scans RAM looking for this ID.
 *   - Data is written to a ring buffer in RAM. J-Link periodically
 *     reads this buffer over the SWD connection and sends it to the
 *     RTT Viewer on the PC.
 *   - No UART pins, no extra wiring, no interrupts needed.
 *   - Works through the same SWD/JTAG connection used for debugging.
 */

#include "SEGGER_RTT.h"
#include <string.h>

/*********************************************************************
 *
 *       Static data
 *
 **********************************************************************
 */

// Up-buffer 0 data: target → host (printf output)
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
  // Keil: place in regular SRAM (0x20000000), not CCM (0x10000000)
  // so J-Link can reliably find it via AHB bus
  static char _acUpBuffer0[BUFFER_SIZE_UP] __attribute__((section(".bss.RTT")));
#else
  static char _acUpBuffer0[BUFFER_SIZE_UP];
#endif

// Down-buffer 0 data: host → target (keyboard input)
static char _acDownBuffer0[BUFFER_SIZE_DOWN];

//
// The RTT Control Block — J-Link finds this by scanning RAM for "SEGGER RTT"
//
// Important: must NOT be placed in CCM RAM (0x10000000 on STM32F4).
// J-Link accesses via AHB matrix which may not reach CCM.
// We place it in the main .bss section (0x20000000 region).
//
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
  static SEGGER_RTT_CB _SEGGER_RTT __attribute__((section(".bss.RTT")));
#else
  static SEGGER_RTT_CB _SEGGER_RTT;
#endif

/*********************************************************************
 *
 *       Static functions
 *
 **********************************************************************
 */

static unsigned int _WriteBlocking(unsigned int BufferIndex, const void *pBuffer, unsigned int NumBytes)
{
  unsigned int NumBytesToWrite;
  unsigned int NumBytesWritten;
  unsigned int RdOff;
  unsigned int WrOff;
  unsigned int NumBytesInBuffer;
  unsigned int RemainingBuffer;
  char *pRing;
  SEGGER_RTT_BUFFER_UP *pRingInfo;

  if (BufferIndex >= (unsigned int)_SEGGER_RTT.MaxNumUpBuffers) {
    return 0;
  }

  pRingInfo = &_SEGGER_RTT.aUp[BufferIndex];
  pRing     = pRingInfo->pBuffer;

  NumBytesToWrite = NumBytes;
  NumBytesWritten = 0;

  while (NumBytesToWrite > 0) {
    WrOff = pRingInfo->WrOff;
    RdOff = pRingInfo->RdOff;

    if (WrOff >= pRingInfo->SizeOfBuffer) {
      WrOff = 0;
    }

    if (WrOff == RdOff) {
      // Buffer full — wait for host to read some data
      // In NO_BLOCK_SKIP mode, we'd just return what we've written
    }

    // Calculate space available
    if (WrOff < RdOff) {
      RemainingBuffer = RdOff - WrOff - 1;
    } else {
      RemainingBuffer = pRingInfo->SizeOfBuffer - WrOff;
      if (RdOff == 0) {
        RemainingBuffer -= 1;
      }
    }

    if (RemainingBuffer > NumBytesToWrite) {
      RemainingBuffer = NumBytesToWrite;
    }

    if (RemainingBuffer == 0) {
      break;  // Buffer full
    }

    // Copy to ring buffer
    memcpy(&pRing[WrOff], &((const char *)pBuffer)[NumBytesWritten], RemainingBuffer);

    NumBytesWritten  += RemainingBuffer;
    NumBytesToWrite  -= RemainingBuffer;
    WrOff            += RemainingBuffer;

    if (WrOff >= pRingInfo->SizeOfBuffer) {
      WrOff -= pRingInfo->SizeOfBuffer;
    }

    pRingInfo->WrOff = WrOff;
  }

  return NumBytesWritten;
}

/*********************************************************************
 *
 *       Public API
 *
 **********************************************************************
 */

/*********************************************************************
 *       SEGGER_RTT_WriteString()
 *
 *  Function description
 *    Stores a null-terminated string in the RTT up-buffer.
 *    This is the most commonly used RTT function.
 */
unsigned int SEGGER_RTT_WriteString(unsigned int BufferIndex, const char *s)
{
  unsigned int Len;
  unsigned int r;

  Len = 0;
  if (s != NULL) {
    while (s[Len] != '\0') {
      Len++;
    }
  }

  if (Len == 0) {
    return 0;
  }

  SEGGER_RTT_LOCK();
  r = _WriteBlocking(BufferIndex, s, Len);
  SEGGER_RTT_UNLOCK();

  return r;
}

/*********************************************************************
 *       SEGGER_RTT_WriteStringLocked()
 *
 *  Function description
 *    Same as WriteString but caller must handle locking.
 */
void SEGGER_RTT_WriteStringLocked(unsigned int BufferIndex, const char *s)
{
  unsigned int Len = 0;
  if (s != NULL) {
    while (s[Len] != '\0') Len++;
    if (Len) _WriteBlocking(BufferIndex, s, Len);
  }
}

/*********************************************************************
 *       SEGGER_RTT_PutChar()
 *
 *  Function description
 *    Stores a single character in the RTT up-buffer.
 *    Ideal for fputc() redirection.
 */
unsigned int SEGGER_RTT_PutChar(unsigned int BufferIndex, char c)
{
  unsigned int r;

  SEGGER_RTT_LOCK();
  r = _WriteBlocking(BufferIndex, &c, 1);
  SEGGER_RTT_UNLOCK();

  return r;
}

/*********************************************************************
 *       SEGGER_RTT_Write()
 */
unsigned int SEGGER_RTT_Write(unsigned int BufferIndex, const void *pBuffer, unsigned int NumBytes)
{
  unsigned int r;

  SEGGER_RTT_LOCK();
  r = _WriteBlocking(BufferIndex, pBuffer, NumBytes);
  SEGGER_RTT_UNLOCK();

  return r;
}

/*********************************************************************
 *       SEGGER_RTT_WriteNoLock()
 */
unsigned int SEGGER_RTT_WriteNoLock(unsigned int BufferIndex, const void *pBuffer, unsigned int NumBytes)
{
  return _WriteBlocking(BufferIndex, pBuffer, NumBytes);
}

/*********************************************************************
 *       SEGGER_RTT_WriteSkipNoLock()
 */
unsigned int SEGGER_RTT_WriteSkipNoLock(unsigned int BufferIndex, const void *pBuffer, unsigned int NumBytes)
{
  // Same as WriteNoLock in this minimal implementation
  return _WriteBlocking(BufferIndex, pBuffer, NumBytes);
}

/*********************************************************************
 *       SEGGER_RTT_HasKey() / SEGGER_RTT_GetKey() / SEGGER_RTT_WaitKey()
 */
int SEGGER_RTT_HasKey(void)
{
  SEGGER_RTT_BUFFER_DOWN *pRing = &_SEGGER_RTT.aDown[0];
  return pRing->WrOff != pRing->RdOff;
}

int SEGGER_RTT_GetKey(void)
{
  SEGGER_RTT_BUFFER_DOWN *pRing = &_SEGGER_RTT.aDown[0];
  int c = -1;

  if (pRing->WrOff != pRing->RdOff) {
    c = (unsigned char)pRing->pBuffer[pRing->RdOff];
    pRing->RdOff++;
    if (pRing->RdOff >= (int)pRing->SizeOfBuffer) {
      pRing->RdOff = 0;
    }
  }
  return c;
}

int SEGGER_RTT_WaitKey(void)
{
  int c;
  do {
    c = SEGGER_RTT_GetKey();
  } while (c == -1);
  return c;
}

/*********************************************************************
 *       SEGGER_RTT_HasData()
 */
unsigned int SEGGER_RTT_HasData(unsigned int BufferIndex)
{
  SEGGER_RTT_BUFFER_DOWN *pRing;
  if (BufferIndex >= (unsigned int)_SEGGER_RTT.MaxNumDownBuffers) return 0;
  pRing = &_SEGGER_RTT.aDown[BufferIndex];
  return pRing->WrOff != pRing->RdOff;
}

/*********************************************************************
 *       SEGGER_RTT_Read()
 */
unsigned int SEGGER_RTT_Read(unsigned int BufferIndex, void *pData, unsigned int BufferSize)
{
  unsigned int NumBytesRem;
  unsigned int NumBytesRead;
  unsigned int RdOff;
  unsigned int WrOff;
  char *pBuffer;
  SEGGER_RTT_BUFFER_DOWN *pRing;

  if (BufferIndex >= (unsigned int)_SEGGER_RTT.MaxNumDownBuffers) return 0;
  pRing   = &_SEGGER_RTT.aDown[BufferIndex];
  pBuffer = pRing->pBuffer;

  NumBytesRead = 0;
  RdOff = pRing->RdOff;
  WrOff = pRing->WrOff;

  while (NumBytesRead < BufferSize) {
    if (RdOff == WrOff) break;
    ((char *)pData)[NumBytesRead++] = pBuffer[RdOff];
    RdOff++;
    if (RdOff >= (int)pRing->SizeOfBuffer) RdOff = 0;
  }

  pRing->RdOff = RdOff;
  return NumBytesRead;
}

/*********************************************************************
 *       SEGGER_RTT_ReadNoLock()
 */
unsigned int SEGGER_RTT_ReadNoLock(unsigned int BufferIndex, void *pData, unsigned int BufferSize)
{
  return SEGGER_RTT_Read(BufferIndex, pData, BufferSize);
}

/*********************************************************************
 *       Buffer management stubs (not needed for basic printf use)
 */
int SEGGER_RTT_ConfigUpBuffer(unsigned int BufferIndex, const char *sName, void *pBuffer, unsigned int BufferSize, unsigned int Flags)
{
  (void)BufferIndex; (void)sName; (void)pBuffer; (void)BufferSize; (void)Flags;
  return 0;
}
int SEGGER_RTT_ConfigDownBuffer(unsigned int BufferIndex, const char *sName, void *pBuffer, unsigned int BufferSize, unsigned int Flags)
{
  (void)BufferIndex; (void)sName; (void)pBuffer; (void)BufferSize; (void)Flags;
  return 0;
}
int SEGGER_RTT_SetNameUpBuffer(unsigned int BufferIndex, const char *sName)
{
  (void)BufferIndex; (void)sName;
  return 0;
}
int SEGGER_RTT_SetNameDownBuffer(unsigned int BufferIndex, const char *sName)
{
  (void)BufferIndex; (void)sName;
  return 0;
}
int SEGGER_RTT_SetFlagsUpBuffer(unsigned int BufferIndex, unsigned int Flags)
{
  (void)BufferIndex; (void)Flags;
  return 0;
}
int SEGGER_RTT_SetFlagsDownBuffer(unsigned int BufferIndex, unsigned int Flags)
{
  (void)BufferIndex; (void)Flags;
  return 0;
}

/*********************************************************************
 *
 *       Control Block initialization
 *
 **********************************************************************
 */

/*********************************************************************
 *       SEGGER_RTT_Init()
 *
 *  Called automatically before main() via the .init_array mechanism.
 *  Initializes the RTT control block that J-Link searches for.
 */
void SEGGER_RTT_Init(void)
{
  // Only init once
  if (_SEGGER_RTT.acID[0] == 'S') {
    return;
  }

  // Set magic ID — J-Link scans RAM for this exact string
  memcpy(&_SEGGER_RTT.acID[0], "SEGGER RTT", 16);

  _SEGGER_RTT.MaxNumUpBuffers   = SEGGER_RTT_MAX_NUM_UP_BUFFERS;
  _SEGGER_RTT.MaxNumDownBuffers = SEGGER_RTT_MAX_NUM_DOWN_BUFFERS;

  // ---- Up-buffer 0: Target → Host (printf output) ----
  _SEGGER_RTT.aUp[0].sName        = "Terminal";
  _SEGGER_RTT.aUp[0].pBuffer      = _acUpBuffer0;
  _SEGGER_RTT.aUp[0].SizeOfBuffer = BUFFER_SIZE_UP;
  _SEGGER_RTT.aUp[0].RdOff        = 0;
  _SEGGER_RTT.aUp[0].WrOff        = 0;
  _SEGGER_RTT.aUp[0].Flags        = SEGGER_RTT_MODE_NO_BLOCK_SKIP;

  // ---- Down-buffer 0: Host → Target (keyboard input) ----
  _SEGGER_RTT.aDown[0].sName        = "Terminal";
  _SEGGER_RTT.aDown[0].pBuffer      = _acDownBuffer0;
  _SEGGER_RTT.aDown[0].SizeOfBuffer = BUFFER_SIZE_DOWN;
  _SEGGER_RTT.aDown[0].RdOff        = 0;
  _SEGGER_RTT.aDown[0].WrOff        = 0;
  _SEGGER_RTT.aDown[0].Flags        = 0;

  // Remaining buffers stay default (empty, unused)
}
