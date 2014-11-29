#include <stdlib/stdlib.h>
#include "streamlib.h"

#define MAX_SNAKE_LENGTH 1000

void die(TKStreamPointer *stdout, int *snake_coords, int snake_length) {
    int j;
    for (j = 0; j < snake_length*2; j+=2) {
        printCharAt(stdout, snake_coords[j], snake_coords[j+1], '*', 12, 0);
        flushStreams();
        sleep(100);
    }
    printCharAt(stdout, TK_SCREEN_COLS/2-2, TK_SCREEN_ROWS/2, 'D', 12, 4);
    printCharAt(stdout, TK_SCREEN_COLS/2-1, TK_SCREEN_ROWS/2, 'i', 12, 4);
    printCharAt(stdout, TK_SCREEN_COLS/2-0, TK_SCREEN_ROWS/2, 'e', 12, 4);
    printCharAt(stdout, TK_SCREEN_COLS/2+1, TK_SCREEN_ROWS/2, 'd', 12, 4);
    printCharAt(stdout, TK_SCREEN_COLS/2+2, TK_SCREEN_ROWS/2, '!', 12, 4);
    flushStreams();
}

int collidesWithSnake(int x, int y, int *snake_coords, int snake_length) {
    int j;
    for (j = 0; j <= snake_length*2; j+=2) {
        if (x == snake_coords[j] && y == snake_coords[j+1]) {
            return 1;
        }
    }
    return 0;
}

void createGoodie(TKStreamPointer *stdout, int *goodie_pos, int *snake_coords, int snake_length) {
    int color;
    unsigned int r;
    // Keep generating positions until there is no collision
    while (1) {
        r = (unsigned int)rand();
        goodie_pos[0] = 1 + (r % (TK_SCREEN_COLS-2));
        goodie_pos[1] = 6 + ((r / (TK_SCREEN_COLS-2)) % (TK_SCREEN_ROWS-7));
        if (!collidesWithSnake(goodie_pos[0], goodie_pos[1], snake_coords, snake_length)) {
            break;
        }
    }
    color = (r % 8) + 7;
    printCharAt(stdout, goodie_pos[0], goodie_pos[1], '@', color, 1);
}

void clearScreen(TKStreamPointer *stdout) {
    int x, y;
    // Clear screen
    for (x = 1; x < TK_SCREEN_COLS - 1; x++) {
        for (y = 6; y < TK_SCREEN_ROWS - 1; y++) {
            printCharAt(stdout, x, y, ' ', 0, 0);
        }
    }
    for (x = 0; x < TK_SCREEN_COLS; x++) {
        printCharAt(stdout, x, 5, '#', 1, 0);
        printCharAt(stdout, x, TK_SCREEN_ROWS-1, '#', 1, 0);
    }
    for (y = 6; y < TK_SCREEN_ROWS - 1; y++) {
        printCharAt(stdout, 0, y, '#', 1, 0);
        printCharAt(stdout, TK_SCREEN_COLS-1, y, '#', 1, 0);
    }
    flushStreams();

}

void gameLoop(TKStreamPointer *stdin, TKStreamPointer *stdout) {
    int i;
    TKMsgHeader *msg;
    int snake_coords[MAX_SNAKE_LENGTH*2];
    int snake_length = 2;
    int snake_vx = 1;
    int snake_vy = 0;
    int goodie_pos[2];

    // Place snake in center of screen
    snake_coords[0] = TK_SCREEN_COLS/2;
    snake_coords[1] = TK_SCREEN_ROWS/2;
    snake_coords[2] = TK_SCREEN_COLS/2;
    snake_coords[3] = TK_SCREEN_ROWS/2;

    clearScreen(stdout);

    srand(1234567);
    createGoodie(stdout, goodie_pos, snake_coords, snake_length);

    while (1) {
        int next_x = snake_coords[0] + snake_vx;
        int next_y = snake_coords[1] + snake_vy;

        // Check if the snake hit the wall
        if (next_x < 1 ||
            next_x >= TK_SCREEN_COLS-1 ||
            next_y < 6 ||
            next_y >= TK_SCREEN_ROWS-1) {
            // Died!
            die(stdout, snake_coords, snake_length);
            return;
        }
        // Check if the snake hit itself
        if (collidesWithSnake(next_x, next_y, snake_coords, snake_length)) {
            // Died!
            die(stdout, snake_coords, snake_length);
            return;
        }

        // Shift snake coordinates by one
        for (i = snake_length * 2; i > 0; i -= 2) {
            snake_coords[i] = snake_coords[i-2];
            snake_coords[i+1] = snake_coords[i-1];
        }
        // Update snake head position
        snake_coords[0] = next_x;
        snake_coords[1] = next_y;

        // Did snake eat the goodie?
        if (snake_coords[0] == goodie_pos[0] && snake_coords[1] == goodie_pos[1]) {
            // Grow snake
            for (i = 0; i < 3; i++) {
                if (snake_length < MAX_SNAKE_LENGTH-1) {
                    snake_length++;
                    snake_coords[snake_length*2] = snake_coords[snake_length*2-2];
                    snake_coords[snake_length*2+1] = snake_coords[snake_length*2-1];
                }
            }
            // Generate a new goodie
            createGoodie(stdout, goodie_pos, snake_coords, snake_length);
        }
        // Draw snake head
        printCharAt(stdout, snake_coords[0], snake_coords[1], '*', 11, 0);
        // Erase snake tail
        printCharAt(stdout, snake_coords[snake_length*2], snake_coords[snake_length*2+1], ' ', 0, 0);

        // Update screen
        flushStreams();

        // Handle input
        while (msg = streamReadMsg(stdin)) {
            switch (msg->identifier) {
                case ID_KEY_CODE: {
                    TKMsgKeyCode *key_msg = (TKMsgKeyCode*)msg;
                    if (key_msg->type == TKB_KEY_TYPE_ARROW &&
                            key_msg->action == TKB_KEY_ACTION_DOWN) {
                        switch (key_msg->dir) {
                        case TKB_ARROW_UP:    snake_vx = 0; snake_vy = -1; break;
                        case TKB_ARROW_DOWN:  snake_vx = 0;  snake_vy = 1; break;
                        case TKB_ARROW_LEFT: snake_vx = -1;  snake_vy = 0; break;
                        case TKB_ARROW_RIGHT: snake_vx = 1;  snake_vy = 0; break;
                        }
                    }
                    break;
                }
            }
        }
        flushStreams();
        sleep(100);
    }
}

void main(TKStreamPointer *stdin, TKStreamPointer *stdout) {
    // TODO: Take this out
    fprintf(stdout, "Snake!\n");
    return;
    while (1) {
        TKMsgHeader *msg;
        int waiting;
        gameLoop(stdin, stdout);
        waiting = 1;
        while (waiting) {
            msg = streamReadMsg(stdin);
            if (msg) {
                switch (msg->identifier) {
                    case ID_KEY_CODE: {
                        TKMsgKeyCode *key_msg = (TKMsgKeyCode*)msg;
                        if (key_msg->type == TKB_KEY_TYPE_ASCII &&
                                key_msg->action == TKB_KEY_ACTION_DOWN &&
                                key_msg->ascii == '\n') {
                            waiting = 0;
                        }
                        break;
                    }
                }
            }
        }
    }
}
