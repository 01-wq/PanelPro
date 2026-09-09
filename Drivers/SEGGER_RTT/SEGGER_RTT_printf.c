/*********************************************************************
 *                    SEGGER Microcontroller GmbH                     *
 *                        The Embedded Experts                        *
 **********************************************************************
 *                                                                    *
 *            (c) 1995 - 2021 SEGGER Microcontroller GmbH             *
 **********************************************************************
 *
 *       www.segger.com     Support: support@segger.com               *
 *
 **********************************************************************
 *
 *       SEGGER RTT * Real Time Transfer for embedded targets         *
 *
 **********************************************************************
 *
 * Minimal printf implementation over RTT.
 * Provides SEGGER_RTT_printf() and SEGGER_RTT_vprintf().
 */

#include "SEGGER_RTT.h"
#include <stdarg.h>
#include <string.h>

#define PRINTF_BUFFER_SIZE  SEGGER_RTT_PRINTF_BUFFER_SIZE

/*********************************************************************
 *
 *       Static helper: reverse a string in place
 *
 **********************************************************************
 */
static void _Reverse(char *s, unsigned int len)
{
  unsigned int i;
  char tmp;
  for (i = 0; i < len / 2; i++) {
    tmp = s[i];
    s[i] = s[len - 1 - i];
    s[len - 1 - i] = tmp;
  }
}

/*********************************************************************
 *
 *       Static helper: unsigned integer → string
 *
 **********************************************************************
 */
static unsigned int _Utoa(unsigned long value, char *buf, unsigned int base, int upper)
{
  unsigned int len = 0;
  unsigned long v = value;
  const char *digits_upper = "0123456789ABCDEF";
  const char *digits_lower = "0123456789abcdef";
  const char *digits = upper ? digits_upper : digits_lower;

  if (v == 0) {
    buf[len++] = '0';
  } else {
    while (v > 0) {
      buf[len++] = digits[v % base];
      v /= base;
    }
  }
  _Reverse(buf, len);
  return len;
}

/*********************************************************************
 *
 *       SEGGER_RTT_vprintf()
 *
 **********************************************************************
 */
int SEGGER_RTT_vprintf(unsigned int BufferIndex, const char *sFormat, va_list *pParamList)
{
  char  buf[PRINTF_BUFFER_SIZE];
  char *pBuf = buf;
  const char *pFmt;
  int   bufLen = 0;
  int   totalLen = 0;
  char  c;
  int   long_flag;
  int   width;
  char  pad_char;

  pFmt = sFormat;

  while ((c = *pFmt++) != '\0') {

    if (c != '%') {
      // Plain character
      pBuf[bufLen++] = c;
      if (bufLen >= (int)(PRINTF_BUFFER_SIZE - 20)) {
        // Flush buffer
        SEGGER_RTT_Write(BufferIndex, buf, bufLen);
        totalLen += bufLen;
        bufLen = 0;
      }
      continue;
    }

    // Got '%' — parse format specifier
    long_flag = 0;
    width = 0;
    pad_char = ' ';

    // Check for zero-pad
    if (*pFmt == '0') {
      pad_char = '0';
      pFmt++;
    }

    // Parse width
    while (*pFmt >= '0' && *pFmt <= '9') {
      width = width * 10 + (*pFmt - '0');
      pFmt++;
    }

    // Check for 'l' (long)
    if (*pFmt == 'l') {
      long_flag = 1;
      pFmt++;
    }

    c = *pFmt++;

    if (c == '\0') break;

    // ---- Flush any pending buffer content ----
    if (bufLen > 0) {
      SEGGER_RTT_Write(BufferIndex, buf, bufLen);
      totalLen += bufLen;
      bufLen = 0;
    }

    switch (c) {
      case '%':
        pBuf[bufLen++] = '%';
        break;

      case 'c': {
        int ch = va_arg(*pParamList, int);
        pBuf[bufLen++] = (char)ch;
        break;
      }

      case 's': {
        const char *s = va_arg(*pParamList, const char *);
        if (s == NULL) s = "(null)";
        SEGGER_RTT_WriteString(BufferIndex, s);
        totalLen += (int)strlen(s);
        pBuf[0] = '\0';
        bufLen = 0;
        break;
      }

      case 'd':
      case 'i': {
        char intbuf[24];
        long val;
        unsigned int len;

        if (long_flag) {
          val = va_arg(*pParamList, long);
        } else {
          val = (long)va_arg(*pParamList, int);
        }

        if (val < 0) {
          SEGGER_RTT_Write(BufferIndex, "-", 1);
          totalLen++;
          val = -val;
        }
        len = _Utoa((unsigned long)val, intbuf, 10, 0);
        // Padding
        while (width > 0 && (int)len < width) {
          char pad[2] = { pad_char, '\0' };
          SEGGER_RTT_WriteString(BufferIndex, pad);
          totalLen++;
          width--;
        }
        SEGGER_RTT_Write(BufferIndex, intbuf, len);
        totalLen += len;
        break;
      }

      case 'u': {
        unsigned long val;
        char intbuf[24];
        unsigned int len;

        if (long_flag) {
          val = va_arg(*pParamList, unsigned long);
        } else {
          val = (unsigned long)va_arg(*pParamList, unsigned int);
        }
        len = _Utoa(val, intbuf, 10, 0);
        SEGGER_RTT_Write(BufferIndex, intbuf, len);
        totalLen += len;
        break;
      }

      case 'x': {
        unsigned long val;
        char intbuf[24];
        unsigned int len;

        if (long_flag) {
          val = va_arg(*pParamList, unsigned long);
        } else {
          val = (unsigned long)va_arg(*pParamList, unsigned int);
        }
        len = _Utoa(val, intbuf, 16, 0);
        SEGGER_RTT_Write(BufferIndex, intbuf, len);
        totalLen += len;
        break;
      }

      case 'X': {
        unsigned long val;
        char intbuf[24];
        unsigned int len;

        if (long_flag) {
          val = va_arg(*pParamList, unsigned long);
        } else {
          val = (unsigned long)va_arg(*pParamList, unsigned int);
        }
        len = _Utoa(val, intbuf, 16, 1);
        SEGGER_RTT_Write(BufferIndex, intbuf, len);
        totalLen += len;
        break;
      }

      case 'p': {
        void *ptr = va_arg(*pParamList, void *);
        char intbuf[24];
        unsigned int len;
        SEGGER_RTT_WriteString(BufferIndex, "0x");
        totalLen += 2;
        len = _Utoa((unsigned long)ptr, intbuf, 16, 0);
        SEGGER_RTT_Write(BufferIndex, intbuf, len);
        totalLen += len;
        break;
      }

      default:
        // Unknown format char — output literally
        pBuf[bufLen++] = '%';
        if (bufLen + 2 < (int)PRINTF_BUFFER_SIZE) {
          pBuf[bufLen++] = c;
        }
        break;
    }
  }

  // Flush remaining buffer
  if (bufLen > 0) {
    SEGGER_RTT_Write(BufferIndex, buf, bufLen);
    totalLen += bufLen;
  }

  return totalLen;
}

/*********************************************************************
 *
 *       SEGGER_RTT_printf()
 *
 **********************************************************************
 */
int SEGGER_RTT_printf(unsigned int BufferIndex, const char *sFormat, ...)
{
  int r;
  va_list ParamList;

  va_start(ParamList, sFormat);
  r = SEGGER_RTT_vprintf(BufferIndex, sFormat, &ParamList);
  va_end(ParamList);

  return r;
}
