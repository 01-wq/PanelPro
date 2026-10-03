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

#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

/*********************************************************************
 *
 *       Defines, configurable
 *
 **********************************************************************
 */

//
// Take in and set to correct values for Cortex-A systems with CPU cache
//
//#define SEGGER_RTT_CPU_CACHE_LINE_SIZE            (32)          // Largest cache line size (in bytes) in the current system
//#define SEGGER_RTT_UNCACHED_OFF                   (0xFB000000)  // Address alias where RTT CB and buffers can be accessed uncached
//
// Most common case:
// Up-channel 0: RTT
// Up-channel 1: SystemView
//
#ifndef   BUFFER_SIZE_UP
  #define BUFFER_SIZE_UP                          (1024)        // Size of the buffer for terminal output of target, up to host (Default: 1k)
#endif
#ifndef   BUFFER_SIZE_DOWN
  #define BUFFER_SIZE_DOWN                        (16)          // Size of the buffer for terminal input to target from host (Usually keyboard input) (Default: 16)
#endif

//
// Target is not allowed to perform other RTT operations while string still has not been stored completely.
// Otherwise we would probably end up with a mixed string in the buffer.
// If using  RTT from within interrupts, multiple tasks or if you have performance limitations, there is a risk of an incomplete string.
// Configure "LOCK/UNLOCK" to a way of protecting against concurrent access to RTT.
// Default is a pair of Global interruption disable/enable.
// Mask/unmask interrupts for Lock/Unlock:
//#define SEGGER_RTT_LOCK()                        __set_BASEPRI(0x50)   // Disable interrupts of priority >= 5
//#define SEGGER_RTT_UNLOCK()                      __set_BASEPRI(0)      // Enable all interrupts
// ARM v7 style (Cortex-M4):
#ifndef   SEGGER_RTT_LOCK
  #define SEGGER_RTT_LOCK()                        { unsigned int _LockState = __get_PRIMASK(); __disable_irq();
#endif
#ifndef   SEGGER_RTT_UNLOCK
  #define SEGGER_RTT_UNLOCK()                      __set_PRIMASK(_LockState); }
#endif

#define SEGGER_RTT_MAX_NUM_UP_BUFFERS             (3)           // Max. number of up-buffers (T->H) available on this target    (Default: 3)
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS           (3)           // Max. number of down-buffers (H->T) available on this target  (Default: 3)

#define SEGGER_RTT_PRINTF_BUFFER_SIZE             (64u)         // Size of buffer for RTT printf to bulk-send chars via RTT     (Default: 64)

//
// Define SEGGER_RTT_IN_RAM to place the RTT Control Block into RAM
// for faster and direct access. Also helps with some linkers.
//
#define SEGGER_RTT_IN_RAM                         (1)

/*********************************************************************
 *
 *       RTT memcpy configuration (boring but necessary)
 *
 **********************************************************************
 */

#if (!defined(SEGGER_RTT_MEMCPY_USE_BYTELOOP) && !defined(SEGGER_RTT_MEMCPY_USE_MEMMOVE))
  #if defined(__CC_ARM) || defined(__ARMCC_VERSION) || defined(__ICCARM__)
    #define SEGGER_RTT_MEMCPY_USE_BYTELOOP
  #elif defined(__GNUC__)
    #define SEGGER_RTT_MEMCPY_USE_MEMMOVE
  #endif
#endif

#endif  // SEGGER_RTT_CONF_H
