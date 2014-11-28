#include <stdlib/stdlib.h>
#include "streamlib.h"

void main(TKStreamPointer *stdin, TKStreamPointer *stdout) {
    spawnProcess(stdout, "/bin", "snake.elf");
    flushStreams();
    while (1) { }
}
