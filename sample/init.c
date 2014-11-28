#include <stdlib/stdlib.h>
#include "streamlib.h"

unsigned short hour;
unsigned short minute;
unsigned short second;

void displayClock(TKStreamPointer *stdout) {
    int x, y;
    // Clear screen
    for (x = 0; x < TK_SCREEN_COLS; x++) {
        for (y = 0; y < 5; y++) {
            printCharAt(stdout, x, y, ' ', 0, 0);
        }
    }
    
    printCharAt(stdout, TK_SCREEN_COLS - 10, 1, (hour / 10) + '0', 15, 8);
    printCharAt(stdout, TK_SCREEN_COLS - 9, 1, (hour % 10) + '0', 15, 8);
    printCharAt(stdout, TK_SCREEN_COLS - 8, 1, ':', 15, 8);
    printCharAt(stdout, TK_SCREEN_COLS - 7, 1, (minute / 10) + '0', 15, 8);
    printCharAt(stdout, TK_SCREEN_COLS - 6, 1, (minute % 10) + '0', 15, 8);
    printCharAt(stdout, TK_SCREEN_COLS - 5, 1, ':', 15, 8);
    printCharAt(stdout, TK_SCREEN_COLS - 4, 1, (second / 10) + '0', 15, 8);
    printCharAt(stdout, TK_SCREEN_COLS - 3, 1, (second % 10) + '0', 15, 8);
    flushStreams();
}

void main(TKStreamPointer *stdin, TKStreamPointer *stdout) {
    fprintf(stdout, "Starting snake.elf...\n");
    spawnProcess(stdout, "/bin", "snake.elf");
    flushStreams();
    
    hour = 0;
    minute = 0;
    second = 0;
    while (1) {
        /*
        displayClock(stdout);
        sleep(1024);
        second++;
        if (second == 60) {
            second = 0;
            minute++;
            if (minute == 60) {
                minute = 0;
                hour++;
            }
        }
        */
        // TODO: Take this out
        fprintf(stdout, "Here!\n");
        sleep(1024);
    }
}
