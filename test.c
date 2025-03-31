#include <stdio.h>
#include <unistd.h>
#include <termios.h>

int main() {
    struct termios oldt, newt;
    int ch;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    // Disable canonical mode, echo, and signal generation
    newt.c_lflag &= ~(ICANON | ECHO | ISIG);
    newt.c_iflag &= ~(IXON | ICRNL);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    printf("Press Ctrl+C to exit...\n");

    while (1) {
        ch = getchar();
        printf("Char: %d (0x%x)\n", ch, ch);

        if (ch == 0x03) {  // Ctrl+C
            printf("\nCaught Ctrl+C! Exiting.\n");
            break;
        } else {
            printf("You pressed: %d (0x%02x)\n", ch, ch);
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return 0;
}
