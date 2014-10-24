#include "tk-user.h"

extern TKStreamPointer stdin_ptr;
extern TKStreamPointer stdout_ptr;

int printf(const char *fmt, ...);
int printf_old(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list args);

void memcpy(void *dest, void *src, int bytes);
