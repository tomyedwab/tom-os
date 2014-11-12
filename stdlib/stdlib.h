#include "tk-user.h"

int fprintf(TKStreamPointer *out, const char *fmt, ...);
// colors for fg/bg are:
// 0 - black
// 1 - dark blue
// 2 - dark green
// 3 - dark cyan
// 4 - dark red
// 5 - dark magenta
// 6 - orange
// 7 - light gray
// 8 - dark gray ** 8-15 are only available for fg
// 9 - blue
// 10 - green
// 11 - cyan
// 12 - red
// 13 - magenta
// 14 - yellow
// 15 - white
void printCharAt(TKStreamPointer *out, int x, int y, char c, char fg, char bg);

int sprintf(char *buf, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list args);

void memcpy(void *dest, void *src, int bytes);

void flushStreams();

void sleep(int milliseconds);

void srand(unsigned long seed);
unsigned long rand();
