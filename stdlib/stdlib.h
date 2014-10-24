#include "tk-user.h"

extern TKStreamPointer stdin_ptr;
extern TKStreamPointer stdout_ptr;

typedef unsigned char *va_list;
#define va_start(list, param) (list = (((va_list)&param) + sizeof(param)))
#define va_arg(list, type)    (*(type *)((list += sizeof(type)) - sizeof(type)))
#define va_end(list)

int printf(const char *fmt, ...);
int printf_old(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list args);

void memcpy(void *dest, void *src, int bytes);
