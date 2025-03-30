// input.c
#include "sc.h"

/*
int history_start = 0; // Індекс початку кільцевого буфера
int history_scroll_pos = 0; // Позиція прокручування історії
int history_display_offset = 0; // Зміщення для відображення історії
*/

int get_input() {
    int c = getchar();
    if (c == 27) { // Esc або стрілки
        int c2 = getchar();
        if (c2 == '[') {
            int c3 = getchar();
            if (c3 == 'A') return KEY_UP;    // Up
            if (c3 == 'B') return KEY_DOWN;  // Down
            if (c3 == 'C') return KEY_RIGHT; // Right
            if (c3 == 'D') return KEY_LEFT;  // Left
            if (c3 == 'H') return KEY_HOME;  // Home
            if (c3 == 'F') return KEY_END;   // End
            if (c3 == '5') { // PgUp
                getchar(); // ~
                return KEY_PGUP;
            }
            if (c3 == '6') { // PgDown
                getchar(); // ~
                return KEY_PGDOWN;
            }
            // Обробка F1-F10
            if (c3 >= '1' && c3 <= '2') {
                int c4 = getchar();
                if (c3 == '1') {
                    if (c4 == '1') { getchar(); return KEY_F1; }  // F1: \033[11~
                    if (c4 == '2') { getchar(); return KEY_F2; }  // F2: \033[12~
                    if (c4 == '3') { getchar(); return KEY_F3; }  // F3: \033[13~
                    if (c4 == '4') { getchar(); return KEY_F4; }  // F4: \033[14~
                    if (c4 == '5') { getchar(); return KEY_F5; }  // F5: \033[15~
                    if (c4 == '7') { getchar(); return KEY_F6; }  // F6: \033[17~
                    if (c4 == '8') { getchar(); return KEY_F7; }  // F7: \033[18~
                    if (c4 == '9') { getchar(); return KEY_F8; }  // F8: \033[19~
                } else if (c3 == '2') {
                    if (c4 == '0') { getchar(); return KEY_F9; }  // F9: \033[20~
                    if (c4 == '1') { getchar(); return KEY_F10; } // F10: \033[21~
                }
            }
        } else if (c2 == 27) {
            return 27; // Esc
        } else {
            ungetc(c2, stdin); // Повернути символ назад
            return 27;
        }
    }
    return c;
}

void execute_command(const char *cmd) {
    // Додаємо перенаправлення stderr у stdout
    char cmd_with_redirect[2048];
    snprintf(cmd_with_redirect, sizeof(cmd_with_redirect), "%s 2>&1", cmd);

    FILE *pipe = popen(cmd_with_redirect, "r");
    if (!pipe) {
        snprintf(history[history_start].output, sizeof(history[history_start].output), "Failed to execute command\n");
        return;
    }

    // Читаємо весь вивід після завершення команди
    char buffer[16384] = {0};
    size_t pos = 0;
    int ch;
    while ((ch = fgetc(pipe)) != EOF && pos < sizeof(buffer) - 1) {
        buffer[pos++] = ch;
    }
    buffer[pos] = 0;

    pclose(pipe);

    // Скидаємо стан терміналу після виконання команди
    disable_raw_mode();
    system("reset"); // Повне скидання терміналу
    enable_raw_mode();

    // Зберігаємо команду та її вивід у кільцевому буфері
    strncpy(history[history_start].command, cmd, sizeof(history[history_start].command) - 1);
    history[history_start].command[sizeof(history[history_start].command) - 1] = 0;
    strncpy(history[history_start].output, buffer, sizeof(history[history_start].output) - 1);
    history[history_start].output[sizeof(history[history_start].output) - 1] = 0;

    // Оновлюємо індекси для кільцевого буфера
    history_start = (history_start + 1) % MAX_HISTORY;
    if (history_count < MAX_HISTORY) {
        history_count++;
    }

    // Скидаємо позицію прокручування історії
    history_scroll_pos = history_count;
    history_display_offset = 0; // Скидаємо зміщення відображення
}

void handle_command_input() {
    disable_raw_mode();
    printf("\x1b[2J\x1b[H"); // Очистити екран

    // Відображаємо історію команд
    for (int i = 0; i < history_count; i++) {
        int idx = (history_start - history_count + i + MAX_HISTORY) % MAX_HISTORY;
        printf("%s\n", history[idx].command);
        printf("%s\n", history[idx].output);
    }
    printf("\nPress Ctrl+O to return...");

    while (getchar() != 15); // Чекаємо Ctrl+O

    enable_raw_mode();
}
