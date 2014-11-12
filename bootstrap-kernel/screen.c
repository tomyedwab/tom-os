char *video_memory;
int x, y;
#define ROWS 25
#define COLS 80

void clearScreen() {
    int i;
    for (i = 0; i < 2 * ROWS * COLS; i++) {
        *(video_memory + i) = 0;
    }
}

void initScreen() {
    video_memory = (char *)0xb8000;
    x = 0;
    y = 0;
    clearScreen();
}

void scrollScreen() {
    int addr;
    for (addr = 1 * COLS * 2; addr < ROWS * COLS * 2; addr++) {
        *(video_memory + addr - COLS * 2) = *(video_memory + addr);
    }
    for (addr = (ROWS - 1) * COLS * 2; addr < ROWS * COLS * 2; addr++) {
        *(video_memory + addr) = 0;
    }
}

void printCharAt(int x, int y, const char c, char color) {
    if (x >= 0 && y >= 0 && x < COLS && y < ROWS) {
        int pos = x * 2 + y * COLS * 2;
        *(video_memory + pos) = c;
        *(video_memory + pos + 1) = color;
    }
}

void printChar(const char c) {
    if (c == '\n' || x > (COLS-1)) {
        x = 0;
        y++;
        if (y >= ROWS) {
            scrollScreen();
            y = ROWS - 1;
        }
        if (c == '\n') {
            return;
        }
    }
    int pos = x * 2 + y * COLS * 2;
    *(video_memory + pos) = c;
    *(video_memory + pos + 1) = 0x0f;
    x++;
}

void printStr(const char *str) {
    const char *p = str;
    while (*p) {
        printChar(*p);
        p++;
    }
}

void printByte(unsigned char byte) {
    char bytes[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    printChar(bytes[(byte >> 4) % 16]);
    printChar(bytes[byte % 16]);
}

void printShort(unsigned short num) {
    printByte((num >> 8) & 0xff);
    printByte(num & 0xff);
}

void printInt(unsigned int num) {
    printShort((num >> 16) & 0xffff);
    printShort(num & 0xffff);
}
