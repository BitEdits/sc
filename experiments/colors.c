#include <stdio.h>
#include <termios.h>
#include <unistd.h>

void setup_terminal(struct termios *oldt, struct termios *newt) {
    tcgetattr(STDIN_FILENO, oldt);
    *newt = *oldt;
    newt->c_lflag &= ~(ICANON | ECHO); // Вимкнути канонічний режим і ехо
    tcsetattr(STDIN_FILENO, TCSANOW, newt);
}

void reset_terminal(struct termios *oldt) {
    tcsetattr(STDIN_FILENO, TCSANOW, oldt);
}

void print_color_table() {
    // Очищення екрана і переміщення курсору вгору
    printf("\x1b[2J\x1b[1;1H");

    // Заголовок
    printf("\x1b[1mANSI Color and Style Combinations\x1b[0m\n\n");

    // Основні стилі
    printf("Styles:\n");
    printf("\x1b[1m1: Bold\x1b[0m | ");
    printf("\x1b[4m4: Underline\x1b[0m | ");
    printf("\x1b[7m7: Inverse\x1b[0m\n\n");

    // Основні кольори тексту і фону
    printf("Basic Colors (Foreground 30-37, Background 40-47):\n");
    for (int fg = 30; fg <= 37; fg++) {
        printf("FG %d: ", fg);
        for (int bg = 40; bg <= 47; bg++) {
            printf("\x1b[%d;%dmSample\x1b[0m ", fg, bg);
        }
        printf("\n");
    }

    // Яскраві кольори тексту і фону
    printf("\nBright Colors (Foreground 90-97, Background 100-107):\n");
    for (int fg = 90; fg <= 97; fg++) {
        printf("FG %d: ", fg);
        for (int bg = 100; bg <= 107; bg++) {
            printf("\x1b[%d;%dmSample\x1b[0m ", fg, bg);
        }
        printf("\n");
    }

    // Комбінації зі стилями
    printf("\nStyles + Colors (FG 31 + Styles):\n");
    printf("\x1b[31m31: Red\x1b[0m | ");
    printf("\x1b[1;31m1;31: Bold Red\x1b[0m | ");
    printf("\x1b[4;31m4;31: Underline Red\x1b[0m | ");
    printf("\x1b[7;31m7;31: Inverse Red\x1b[0m\n");

    // Повні приклади комбінацій
    printf("Full Examples:\n");
    printf(" [1;32;44] \x1b[1;32;44mBold Green on Blue\x1b[0m | ");
    printf(" [4;93;105] \x1b[4;93;105mUnderline Bright Yellow on Bright Purple\x1b[0m | ");
    printf(" [7;36;41] \x1b[7;36;41mInverse Cyan on Red\x1b[0m\n");

}

int main() {
    struct termios oldt, newt;

    // Налаштування термінала
    setup_terminal(&oldt, &newt);

    // Виведення таблиці кольорів
    print_color_table();

    // Відновлення налаштувань термінала
    reset_terminal(&oldt);
//    printf("\x1b[2J\x1b[1;1H"); // Очистити екран перед виходом

    return 0;
}
