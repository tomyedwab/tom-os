#include "tk-user.h"

int fprintf(TKStreamPointer *out, const char *fmt, ...);

int sprintf(char *buf, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list args);

void memcpy(void *dest, void *src, int bytes);
