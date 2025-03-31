#include <stdio.h>
#include <unistd.h>
#include <termios.h>

int main() {
    struct termios old, raw;
    int ch;
    tcgetattr(STDIN_FILENO, &old);
    raw = old;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN); // Enable CTRL-O for Sokhatsky Commander
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    printf("Press Ctrl+C to exit...\n");
    while (1) {
        ch = getchar();
        printf("Char: %d (0x%x)\n", ch, ch);
        if (ch == 3) {  // Ctrl+C
            printf("\nCaught Ctrl+C! Exiting.\n");
            break;
        } else {
            printf("You pressed: %d (0x%02x)\n", ch, ch);
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    return 0;
}
