#ifndef __RTT_LOG_H__
#define __RTT_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void RTT_LogInit(void);
void RTT_LogWrite(const char *str);
void RTT_LogPrintf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __RTT_LOG_H__ */
