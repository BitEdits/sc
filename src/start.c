// start.c

#include "socha.h"

// Глобальні змінні
Panel left_panel, right_panel;
Panel *active_panel;
int rows, cols;
CommandEntry history[MAX_HISTORY];
int history_count = 0, history_pos = 0;
int history_start = 0; // Індекс початку кільцевого буфера
char command_buffer[1024*4];
volatile sig_atomic_t resize_flag = 0;
int show_command_buffer = 0;
int history_scroll_pos = 0; // Позиція прокручування історії команд
int history_display_offset = 0; // Зміщення для відображення історії

#ifdef _WIN32

HANDLE hStdin;
DWORD orig_mode;

void enable_raw_mode() {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &orig_mode);
    DWORD raw_mode = orig_mode;
    raw_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    SetConsoleMode(hStdin, raw_mode);
}

void disable_raw_mode() {
    SetConsoleMode(hStdin, orig_mode);
}

int get_window_size(int *rows, int *cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleScreenBufferInfo(hStdOut, &csbi)) {
        return -1;
    }
    *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return 0;
}

#else

struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int get_window_size(int *r, int *c) {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    *r = ws.ws_row;
    *c = ws.ws_col;
}

#endif

void handle_resize(int sig) {
    resize_flag = 1;
}

int main() {
    // Ініціалізація
    enable_raw_mode();
    atexit(disable_raw_mode);

    // Увімкнути альтернативний буфер екрана
    printf("\x1b[?1049h");

    // Встановлення обробника SIGWINCH
  //  signal(SIGWINCH, handle_resize);

    get_window_size(&rows, &cols);

    // Ініціалізація панелей
    strcpy(left_panel.path, "/");
    strcpy(right_panel.path, "/");
    left_panel.sort_type = 0;
    right_panel.sort_type = 0;
    left_panel.scroll_offset = 0;
    right_panel.scroll_offset = 0;
    left_panel.dir_history_count = 0;
    right_panel.dir_history_count = 0;
    active_panel = &left_panel;

    load_files(&left_panel);
    load_files(&right_panel);

    command_buffer[0] = 0;

    // Початкове малювання інтерфейсу
    draw_interface();

    // Основний цикл
    int cmd_pos = 0;
    int prev_cursor = active_panel->cursor;
    while (1) {
        // Перевірка зміни розміру
        if (resize_flag) {
            get_window_size(&rows, &cols);
            resize_flag = 0;
            draw_interface();
        }

        int c = get_input();

        if (c == 15) { // Ctrl+O
            show_command_buffer = !show_command_buffer;
            if (show_command_buffer) {
                history_scroll_pos = history_count; // Починаємо з останньої команди
                history_display_offset = 0; // Скидаємо зміщення відображення
            }
            command_buffer[0] = 0;
            draw_interface();
        } else if (c == '\t') { // Перемикання панелей (Tab)
            int prev_active = (active_panel == &left_panel) ? 1 : 0;
            active_panel = (active_panel == &left_panel) ? &right_panel : &left_panel;
            chdir(active_panel->path);
            int panel_width = (cols - 1) / 2;
            if (prev_active) {
                update_cursor(&left_panel, 1, panel_width, 0, prev_cursor);
                update_cursor(&right_panel, panel_width + 1, panel_width, 1, active_panel->cursor);
            } else {
                update_cursor(&right_panel, panel_width + 1, panel_width, 0, prev_cursor);
                update_cursor(&left_panel, 1, panel_width, 1, active_panel->cursor);
            }
            prev_cursor = active_panel->cursor;
            cmd_pos = 0;
            command_buffer[0] = 0;
            draw_interface();
        } else if (c == KEY_UP) {
            if (!show_command_buffer) { // Панелі видимі: навігація по файлах
                if (active_panel->cursor > 0) {
                    prev_cursor = active_panel->cursor;
                    active_panel->cursor--;
                    int panel_width = (cols - 1) / 2;
                    int start_col = (active_panel == &left_panel) ? 1 : panel_width + 1;
                    if (active_panel->cursor % rows < rows){
                        draw_panel(active_panel, start_col, panel_width, 1);
                    } else {
                       update_cursor(active_panel, start_col, panel_width, 1, prev_cursor);
                    }
                    cmd_pos = 0;
                    command_buffer[0] = 0;
                }
            } else { // Режим Ctrl+O: прокручування історії команд
                if (history_scroll_pos > 0) {
                    history_scroll_pos--;
                    int idx = (history_start - history_count + history_scroll_pos + MAX_HISTORY) % MAX_HISTORY;
                    strncpy(command_buffer, history[idx].command, sizeof(command_buffer) - 1);
                    command_buffer[sizeof(command_buffer) - 1] = 0;
                    draw_interface();
                }
            }
        } else if (c == KEY_DOWN) {
            if (!show_command_buffer) { // Панелі видимі: навігація по файлах
                if (active_panel->cursor < active_panel->file_count - 1) {
                    prev_cursor = active_panel->cursor;
                    active_panel->cursor++;
                    int panel_width = (cols - 1) / 2;
                    int start_col = (active_panel == &left_panel) ? 1 : panel_width + 1;
                    if (active_panel->cursor > rows - 4 - 2) {
                        draw_panel(active_panel, start_col, panel_width, 1);
                    } else {
                        update_cursor(active_panel, start_col, panel_width, 1, prev_cursor);
                    }
                    cmd_pos = 0;
                    command_buffer[0] = 0;
                }
            } else { // Режим Ctrl+O: прокручування історії команд
                if (history_scroll_pos < history_count) {
                    history_scroll_pos++;
                    if (history_scroll_pos == history_count) {
                        command_buffer[0] = 0; // Очищаємо буфер, якщо досягли кінця
                    } else {
                        int idx = (history_start - history_count + history_scroll_pos + MAX_HISTORY) % MAX_HISTORY;
                        strncpy(command_buffer, history[idx].command, sizeof(command_buffer) - 1);
                        command_buffer[sizeof(command_buffer) - 1] = 0;
                    }
                    draw_interface();
                }
            }
        } else if (c == KEY_PGUP) {
            if (show_command_buffer) { // Режим Ctrl+O: скролінг історії
                int max_display = rows - 4;
                history_display_offset += max_display / 2; // Скролимо на пів екрана
                // Перевіряємо межі
                int total_lines = 0;
                for (int i = 0; i < history_count; i++) {
                    int idx = (history_start - history_count + i + MAX_HISTORY) % MAX_HISTORY;
                    total_lines++; // Рядок для команди
                    char *output = history[idx].output;
                    for (int j = 0; output[j]; j++) {
                        if (output[j] == '\n') total_lines++;
                    }
                }
                if (history_display_offset > total_lines - max_display) {
                    history_display_offset = total_lines - max_display;
                }
                if (history_display_offset < 0) history_display_offset = 0;
                draw_interface();
            } else { // Панелі видимі: Page Up
                prev_cursor = active_panel->cursor;
                active_panel->cursor -= (rows - 4 - 2);
                if (active_panel->cursor < 0) active_panel->cursor = 0;
                int panel_width = (cols - 1) / 2;
                int start_col = (active_panel == &left_panel) ? 1 : panel_width + 1;
//                update_cursor(active_panel, start_col, panel_width, 1, prev_cursor);
                cmd_pos = 0;
                command_buffer[0] = 0;
                draw_interface();
            }
        } else if (c == KEY_PGDOWN) {
            if (show_command_buffer) { // Режим Ctrl+O: скролінг історії
                history_display_offset -= (rows - 4) / 2; // Скролимо на пів екрана
                if (history_display_offset < 0) history_display_offset = 0;
                draw_interface();
            } else { // Панелі видимі: Page Down
                prev_cursor = active_panel->cursor;
                active_panel->cursor += (rows - 4 - 2);
                if (active_panel->cursor > active_panel->file_count) active_panel->cursor = active_panel->file_count - 1;
                int panel_width = (cols - 1) / 2;
                int start_col = (active_panel == &left_panel) ? 1 : panel_width + 1;
//                update_cursor(active_panel, start_col, panel_width, 1, prev_cursor);
                cmd_pos = 0;
                command_buffer[0] = 0;
                draw_interface();
            }
        } else if (c == KEY_RIGHT && !show_command_buffer) { // Вхід у директорію (Lynx-подібна навігація)
            if (active_panel->files[active_panel->cursor].is_dir) {
                // Зберігаємо позицію перед входом у директорію
                if (active_panel->dir_history_count < MAX_FILES) {
                    DirHistory *dh = &active_panel->dir_history[active_panel->dir_history_count];
                    strcpy(dh->parent_path, active_panel->path);
                    strcpy(dh->dir_name, active_panel->files[active_panel->cursor].name);
                    dh->cursor_pos = active_panel->cursor;
                    active_panel->dir_history_count++;
                }

                char new_path[1024*8];
                snprintf(new_path, sizeof(new_path), "%s/%s", active_panel->path, active_panel->files[active_panel->cursor].name);
                strcpy(active_panel->path, new_path);
                active_panel->cursor = 0;
                active_panel->scroll_offset = 0;
                load_files(active_panel);
                chdir(active_panel->path);
                draw_interface();
            }
            cmd_pos = 0;
            command_buffer[0] = 0;
        } else if (c == KEY_LEFT && !show_command_buffer) { // Вихід із директорії (Lynx-подібна навігація)
            if (strcmp(active_panel->path, "/") != 0) { // Не корінь
                // Зберігаємо поточний шлях для порівняння
                char current_path[1024*3];
                strcpy(current_path, active_panel->path);

                char *last_slash = strrchr(active_panel->path, '/');
                char dir_name[256];
                if (last_slash != active_panel->path) { // Не кореневий слеш
                    strcpy(dir_name, last_slash + 1);
                    *last_slash = 0; // Обрізаємо шлях до батьківської директорії
                } else {
                    strcpy(dir_name, last_slash + 1);
                    active_panel->path[1] = 0; // Залишаємо тільки "/"
                }

                // Завантажуємо файли в батьківській директорії
                load_files(active_panel);

                // Відновлюємо позицію курсору з історії
                for (int i = active_panel->dir_history_count - 1; i >= 0; i--) {
                    DirHistory *dh = &active_panel->dir_history[i];
                    if (strcmp(dh->parent_path, active_panel->path) == 0 && strcmp(dh->dir_name, dir_name) == 0) {
                        active_panel->cursor = dh->cursor_pos;
                        active_panel->scroll_offset = active_panel->cursor - (rows - 4 - 2) / 2;
                        if (active_panel->scroll_offset < 0) active_panel->scroll_offset = 0;
                        active_panel->dir_history_count--;
                        break;
                    }
                }
                chdir(active_panel->path);
                draw_interface();
            }
            cmd_pos = 0;
            command_buffer[0] = 0;
        } else if (c == KEY_HOME && !show_command_buffer) { // Home
            prev_cursor = active_panel->cursor;
            active_panel->cursor = 0;
            active_panel->scroll_offset = 0;
            int panel_width = (cols - 1) / 2;
            int start_col = (active_panel == &left_panel) ? 1 : panel_width + 1;
//          update_cursor(active_panel, start_col, panel_width, 1, prev_cursor);
            cmd_pos = 0;
            command_buffer[0] = 0;
            draw_interface();
        } else if (c == KEY_END && !show_command_buffer) { // End
            prev_cursor = active_panel->cursor;
            active_panel->cursor = active_panel->file_count - 1;
            active_panel->scroll_offset = active_panel->cursor - (rows - 4 - 2) + 1;
            if (active_panel->scroll_offset < 0) active_panel->scroll_offset = 0;
            int panel_width = (cols - 1) / 2;
            int start_col = (active_panel == &left_panel) ? 1 : panel_width + 1;
//          update_cursor(active_panel, start_col, panel_width, 1, prev_cursor);
            cmd_pos = 0;
            command_buffer[0] = 0;
            draw_interface();
        } else if (c == '\n') { // Enter
            if (command_buffer[0] != 0) { // Виконання команди
                show_command_buffer = 1; // Переходимо в режим Ctrl+O
                char cmd_copy[1024*3];
                strcpy(cmd_copy, command_buffer);
                execute_command(command_buffer);
                append_to_history_display(cmd_copy, history[(history_start - 1 + MAX_HISTORY) % MAX_HISTORY].output);
                history_scroll_pos = history_count; // Скидаємо позицію прокручування
                command_buffer[0] = 0;
                draw_interface();
            } else if (!show_command_buffer) { // Відкриття теки
                if (active_panel->files[active_panel->cursor].is_dir) {
                    // Перевірка, чи це ".."
                    if (strcmp(active_panel->files[active_panel->cursor].name, "..") == 0) {
                        if (strcmp(active_panel->path, "/") != 0) { // Не корінь
                            // Зберігаємо поточний шлях для порівняння
                            char current_path[1024*3];
                            strcpy(current_path, active_panel->path);

                            char *last_slash = strrchr(active_panel->path, '/');
                            char dir_name[256];
                            if (last_slash != active_panel->path) { // Не кореневий слеш
                                strcpy(dir_name, last_slash + 1);
                                *last_slash = 0; // Обрізаємо шлях до батьківської директорії
                            } else {
                                strcpy(dir_name, last_slash + 1);
                                active_panel->path[1] = 0; // Залишаємо тільки "/"
                            }

                            // Завантажуємо файли в батьківській директорії
                            load_files(active_panel);

                            // Відновлюємо позицію курсору з історії
                            for (int i = active_panel->dir_history_count - 1; i >= 0; i--) {
                                DirHistory *dh = &active_panel->dir_history[i];
                                if (strcmp(dh->parent_path, active_panel->path) == 0 && strcmp(dh->dir_name, dir_name) == 0) {
                                    active_panel->cursor = dh->cursor_pos;
                                    active_panel->scroll_offset = active_panel->cursor - (rows - 4 - 2) / 2;
                                    if (active_panel->scroll_offset < 0) active_panel->scroll_offset = 0;
                                    active_panel->dir_history_count--;
                                    break;
                                }
                            }
                        }
                    } else {
                        // Зберігаємо позицію перед входом у директорію
                        if (active_panel->dir_history_count < MAX_FILES) {
                            DirHistory *dh = &active_panel->dir_history[active_panel->dir_history_count];
                            strcpy(dh->parent_path, active_panel->path);
                            strcpy(dh->dir_name, active_panel->files[active_panel->cursor].name);
                            dh->cursor_pos = active_panel->cursor;
                            active_panel->dir_history_count++;
                        }

                        char new_path[1024*8];
                        snprintf(new_path, sizeof(new_path), "%s/%s", active_panel->path, active_panel->files[active_panel->cursor].name);
                        strcpy(active_panel->path, new_path);
                        active_panel->cursor = 0;
                        active_panel->scroll_offset = 0;
                        chdir(active_panel->path);
                        load_files(active_panel);
                    }
                    draw_interface();
                }
            }
        } else if (c == 127) { // Backspace
            if (strlen(command_buffer) > 0) {
                command_buffer[strlen(command_buffer) - 1] = 0;
                draw_interface();
            }
        } else if (c >= 32 && c <= 126) { // Друковані символи
            int len = strlen(command_buffer);
            if (len < sizeof(command_buffer) - 1) {
                command_buffer[len] = c;
                command_buffer[len + 1] = 0;
                draw_interface();
            }
        } else if (c == KEY_F4) { // F4 (Edit)
            if (!show_command_buffer && active_panel->file_count > 0 && !active_panel->files[active_panel->cursor].is_dir) {
                char cmd[1024*8];
                snprintf(cmd, sizeof(cmd), "mcedit %s/%s", active_panel->path, active_panel->files[active_panel->cursor].name);
                disable_raw_mode();
                int ret = system(cmd);
                if (ret == -1) {
                    printf("\x1b[2J\x1b[HFailed to execute editor. Ensure it exists and is executable.\n");
                    getchar();
                }
                enable_raw_mode();
                atexit(disable_raw_mode);
//              printf("\x1b[?1049h");
//                signal(SIGWINCH, handle_resize);
                get_window_size(&rows, &cols);
                draw_interface();
            }
        } else if (c == KEY_F9) { // F9 (Menu)
            if (!show_command_buffer) {
                int result = handle_menu();
                if (result == 1) break; // Сигнал для виходу
                draw_interface();
            }
        } else if (c == KEY_F10) { // F10 (Quit)
            if (!show_command_buffer) {
                if (handle_exit_dialog()) break;
                draw_interface();
            }
        }
    }

    // Вимкнути альтернативний буфер екрана
    printf("\x1b[?1049l");
    printf("\x1b[2J\x1b[H"); // Очистити екран
    return 0;
}
