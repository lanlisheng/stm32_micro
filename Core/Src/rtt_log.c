#include "rtt_log.h"

#include "SEGGER_RTT.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define RTT_LOG_BUFFER_INDEX 0U
#define RTT_LOG_LOCAL_BUFFER_SIZE 256U

void RTT_LogInit(void)
{
    SEGGER_RTT_Init();
    SEGGER_RTT_SetFlagsUpBuffer(RTT_LOG_BUFFER_INDEX, SEGGER_RTT_MODE_NO_BLOCK_TRIM);
}

void RTT_LogWrite(const char *str)
{
    if (str == NULL)
    {
        return;
    }

    SEGGER_RTT_WriteString(RTT_LOG_BUFFER_INDEX, str);
}

void RTT_LogPrintf(const char *fmt, ...)
{
    va_list args;
    char buffer[RTT_LOG_LOCAL_BUFFER_SIZE];
    int length;

    if (fmt == NULL)
    {
        return;
    }

    va_start(args, fmt);
    length = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (length <= 0)
    {
        return;
    }

    if (length >= (int)sizeof(buffer))
    {
        length = (int)sizeof(buffer) - 1;
    }

    SEGGER_RTT_Write(RTT_LOG_BUFFER_INDEX, buffer, (unsigned)length);
}

int fputc(int ch, FILE *f)
{
    (void)f;
    SEGGER_RTT_PutChar(RTT_LOG_BUFFER_INDEX, (char)ch);
    return ch;
}
