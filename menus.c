// interface.c
#include "sc.h"

void draw_panel_border(int start_col, int width, int height) {
    printf("%s", COLOR_TEXT);
    // Верхня рамка
    printf("\x1b[2;%dH┌", start_col);
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("┐");

    // Бокові рамки
    for (int i = 0; i < height - 2; i++) {
        printf("\x1b[%d;%dH│", 3 + i, start_col);
        printf("\x1b[%d;%dH│", 3 + i, start_col + width - 1);
    }

    // Нижня рамка
    printf("\x1b[%d;%dH└", 2 + height, start_col);
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("┘");
}

void draw_panel(Panel *panel, int start_col, int width, int is_active) {
    int panel_height = rows - 4; // 2 рядки на заголовок, 1 на командний рядок, 1 на меню
    int visible_files = panel_height - 2; // 2 рядки на рамку

    // Оновлення прокручування
    if (panel->cursor < panel->scroll_offset) {
        panel->scroll_offset = panel->cursor;
    } else if (panel->cursor >= panel->scroll_offset + visible_files) {
        panel->scroll_offset = panel->cursor - visible_files + 1;
    }

    // Малювання рамки
    draw_panel_border(start_col, width, panel_height);

    // Заголовок панелі
    printf("\x1b[2;%dH%s %s ", start_col + 1, COLOR_HEADER, panel->path);
    for (int i = strlen(panel->path) + start_col + 2; i < start_col + width - 1; i++) printf(" ");

    // Сепаратори для регіонів (65%-15%-20%)
    int name_width = (width - 4) * 0.65; // 65% для імені
    int size_width = (width - 4) * 0.15; // 15% для розміру
    int date_width = (width - 4) * 0.20; // 20% для дати
    int sep1 = start_col + 1 + name_width;
    int sep2 = sep1 + size_width;

    // Малюємо сепаратори
    for (int i = 0; i < panel_height - 2; i++) {
        printf("\x1b[%d;%dH│", 3 + i, sep1);
        printf("\x1b[%d;%dH│", 3 + i, sep2);
    }

    // Файли
    for (int i = 0; i < visible_files; i++) {
        int file_idx = i + panel->scroll_offset;
        if (file_idx >= panel->file_count) {
            // Заповнення порожнього простору
            printf("\x1b[%d;%dH%s", i + 3, start_col + 1, COLOR_TEXT);
            for (int j = 0; j < width - 2; j++) printf(" ");
            continue;
        }

        if (is_active && file_idx == panel->cursor) {
            printf("\x1b[%d;%dH%s", i + 3, start_col + 1, COLOR_HIGHLIGHT);
        } else {
            printf("\x1b[%d;%dH%s", i + 3, start_col + 1, panel->files[file_idx].is_dir ? COLOR_DIR : COLOR_TEXT);
        }

        char *name = panel->files[file_idx].name;
        off_t size = panel->files[file_idx].size;
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%b %d %H:%M", localtime(&panel->files[file_idx].mtime));

        // Форматування з сепараторами
        printf("%-*.*s %*lld %*s", name_width, name_width, name, size_width, size, date_width, time_str);
    }
}

void append_to_history_display(const char *command, const char *output) {
    int max_display = rows - 2; // Залишаємо місце для нижнього меню

    // Збираємо всі рядки історії в один масив
    char *history_lines[16384];
    int line_count = 0;

    for (int i = 0; i < history_count && line_count < 16384; i++) {
        int idx = (history_start - history_count + i + MAX_HISTORY) % MAX_HISTORY;
        if (i == history_count - 1) {
            // Для останньої команди використовуємо передані command і output
            char command_line[1024];
            snprintf(command_line, sizeof(command_line), "> %s", command);
            history_lines[line_count] = strdup(command_line);
            line_count++;

            char output_copy[16384];
            strncpy(output_copy, output, sizeof(output_copy) - 1);
            output_copy[sizeof(output_copy) - 1] = 0;
            char *line = strtok(output_copy, "\n");
            while (line && line_count < 16384) {
                history_lines[line_count] = strdup(line);
                line_count++;
                line = strtok(NULL, "\n");
            }
        } else {
            // Для попередніх команд використовуємо збережені дані
            char command_line[1024];
            snprintf(command_line, sizeof(command_line), "> %s", history[idx].command);
            history_lines[line_count] = strdup(command_line);
            line_count++;

            char output_copy[16384];
            strncpy(output_copy, history[idx].output, sizeof(output_copy) - 1);
            output_copy[sizeof(output_copy) - 1] = 0;
            char *line = strtok(output_copy, "\n");
            while (line && line_count < 16384) {
                history_lines[line_count] = strdup(line);
                line_count++;
                line = strtok(NULL, "\n");
            }
        }
    }

    // Визначаємо, з якого рядка починати відображення
    int start_line = line_count - max_display - history_display_offset;
    if (start_line < 0) start_line = 0;

    // Оновлюємо лише видимі рядки
    for (int i = 0; i < max_display && (start_line + i) < line_count; i++) {
        printf("\x1b[%d;1H%-*s", i + 1, cols, history_lines[start_line + i]);
    }

    // Очищаємо залишок екрана
    for (int i = line_count - start_line; i < max_display; i++) {
        printf("\x1b[%d;1H%-*s", i + 1, cols, "");
    }

    // Оновлюємо командний рядок
    printf("\x1b[%d;1H%-*s", rows - 1, cols, "");
    printf("\x1b[%d;1H%s> %s%s", rows - 1, COLOR_TEXT, command_buffer, COLOR_RESET);

    // Оновлюємо нижнє меню
    printf("\x1b[%d;1H\x1b[1;37m\x1b[42m%-*s", rows, cols, "");
    printf("\x1b[%d;1H\x1b[1;37m\x1b[42m1Help 2Menu 3View 4Edit 5Copy 6RenMov 7Mkdir 8Delete 9Sort 10Quit%s", rows, COLOR_RESET);

    // Звільняємо пам’ять
    for (int i = 0; i < line_count; i++) {
        free(history_lines[i]);
    }
}

void draw_interface() {
    // Скидаємо стан терміналу перед малюванням
    printf("\x1b[0m\x1b[2J\x1b[H"); // Скидаємо кольори, очищаємо екран, переміщаємо курсор у верхній лівий кут

    if (show_command_buffer) {
        // Відображаємо історію команд і їх виводи
        int max_display = rows - 2; // Залишаємо місце для нижнього меню

        // Збираємо всі рядки історії в один масив
        char *history_lines[16384];
        int line_count = 0;

        for (int i = 0; i < history_count && line_count < 16384; i++) {
            int idx = (history_start - history_count + i + MAX_HISTORY) % MAX_HISTORY;

            // Додаємо рядок команди
            char command_line[1024];
            snprintf(command_line, sizeof(command_line), "> %s", history[idx].command);
            history_lines[line_count] = strdup(command_line);
            line_count++;

            // Додаємо вивід команди
            char output_copy[16384];
            strncpy(output_copy, history[idx].output, sizeof(output_copy) - 1);
            output_copy[sizeof(output_copy) - 1] = 0;
            char *line = strtok(output_copy, "\n");
            while (line && line_count < 16384) {
                history_lines[line_count] = strdup(line);
                line_count++;
                line = strtok(NULL, "\n");
            }
        }

        // Визначаємо, з якого рядка починати відображення
        int start_line = line_count - max_display - history_display_offset;
        if (start_line < 0) start_line = 0;

        // Відображаємо рядки
        for (int i = 0; i < max_display && (start_line + i) < line_count; i++) {
            printf("\x1b[%d;1H%-*s", i + 1, cols, history_lines[start_line + i]);
        }

        // Очищаємо залишок екрана
        for (int i = line_count - start_line; i < max_display; i++) {
            printf("\x1b[%d;1H%-*s", i + 1, cols, "");
        }

        // Звільняємо пам’ять
        for (int i = 0; i < line_count; i++) {
            free(history_lines[i]);
        }
    } else {
        // Заголовок
        printf("%sSokhatsky Commander%s", COLOR_HEADER, COLOR_RESET);
        printf("\x1b[1;%dH", cols - 20);
        printf("%sSize: %dB%s", COLOR_HEADER, 0, COLOR_RESET);

        // Панелі
        int panel_width = (cols - 2) / 2; // 2 для роздільника
        draw_panel(&left_panel, 1, panel_width, active_panel == &left_panel);
        draw_panel(&right_panel, panel_width + 3, panel_width, active_panel == &right_panel);
    }

    // Командний рядок
    printf("\x1b[%d;1H%s", rows - 1, COLOR_TEXT);
    for (int i = 0; i < cols; i++) printf(" "); // Очистити рядок
    printf("\x1b[%d;1H%s> %s%s", rows - 1, COLOR_TEXT, command_buffer, COLOR_RESET);

    // Нижнє меню
    printf("\x1b[%d;1H\x1b[1;37m\x1b[42m", rows); // Білий текст, зелений фон
    for (int i = 0; i < cols; i++) printf(" "); // Очистити рядок
    printf("\x1b[%d;1H\x1b[1;37m\x1b[42m1Help 2Menu 3View 4Edit 5Copy 6RenMov 7Mkdir 8Delete 9Sort 10Quit%s", rows, COLOR_RESET);
}

void draw_exit_dialog() {
    int dialog_width = 40;
    int dialog_height = 5;
    int start_row = (rows - dialog_height) / 2;
    int start_col = (cols - dialog_width) / 2;

    // Малюємо рамку
    printf("%s", COLOR_TEXT);
    printf("\x1b[%d;%dH┌", start_row, start_col);
    for (int i = 0; i < dialog_width - 2; i++) printf("─");
    printf("┐");

    for (int i = 0; i < dialog_height - 2; i++) {
        printf("\x1b[%d;%dH│", start_row + 1 + i, start_col);
        printf("\x1b[%d;%dH│", start_row + 1 + i, start_col + dialog_width - 1);
    }

    printf("\x1b[%d;%dH└", start_row + dialog_height - 1, start_col);
    for (int i = 0; i < dialog_width - 2; i++) printf("─");
    printf("┘");

    // Текст
    printf("\x1b[%d;%dHDo you want to exit Sokhatsky Commander?", start_row + 1, start_col + 2);
    printf("\x1b[%d;%dHY/N?", start_row + 3, start_col + dialog_width / 2 - 2);
}

int handle_exit_dialog() {
    draw_exit_dialog();
    while (1) {
        int c = get_input();
        if (c == 'y' || c == 'Y') return 1;
        if (c == 'n' || c == 'N') return 0;
    }
}

void draw_menu() {
    const char *menu_tabs[] = {"Left", "File", "Command", "Options", "Right"};
    int tab_count = 5;
    int tab_width = cols / tab_count;
    int start_row = 1; // Верхній рядок

    // Малюємо меню у верхній частині з зеленим фоном (як у MC)
    printf("\x1b[%d;1H\x1b[1;37m\x1b[42m", start_row); // Білий текст, зелений фон
    for (int i = 0; i < cols; i++) printf(" "); // Очистити рядок

    // Малюємо вкладки
    for (int i = 0; i < tab_count; i++) {
        printf("\x1b[%d;%dH%s", start_row, 2 + i * tab_width, menu_tabs[i]);
    }
}

int handle_menu() {
    const char *menu_tabs[] = {"Left", "File", "Command", "Options", "Right"};
    int tab_count = 5;
    int tab_width = cols / tab_count;
    int start_row = 1; // Верхній рядок
    int selected_tab = 0;

    while (1) {
        // Малюємо меню
        draw_menu();

        // Підсвічуємо вибрану вкладку
        printf("\x1b[%d;%dH\x1b[1;30m\x1b[47m%s\x1b[1;37m\x1b[42m", start_row, 2 + selected_tab * tab_width, menu_tabs[selected_tab]);

        int c = get_input();
        if (c == KEY_LEFT && selected_tab > 0) {
            selected_tab--;
        } else if (c == KEY_RIGHT && selected_tab < tab_count - 1) {
            selected_tab++;
        } else if (c == '\n') {
            if (selected_tab == 4) { // Right -> Sort (для прикладу)
                active_panel->sort_type = (active_panel->sort_type + 1) % 3;
                load_files(active_panel);
            }
            break;
        } else if (c == 27) { // Esc
            break;
        }
    }
    return 0;
}

void draw_search_dialog() {
    int dialog_width = 40;
    int dialog_height = 5;
    int start_row = (rows - dialog_height) / 2;
    int start_col = (cols - dialog_width) / 2;

    // Малюємо рамку
    printf("%s", COLOR_TEXT);
    printf("\x1b[%d;%dH┌", start_row, start_col);
    for (int i = 0; i < dialog_width - 2; i++) printf("─");
    printf("┐");

    for (int i = 0; i < dialog_height - 2; i++) {
        printf("\x1b[%d;%dH│", start_row + 1 + i, start_col);
        printf("\x1b[%d;%dH│", start_row + 1 + i, start_col + dialog_width - 1);
    }

    printf("\x1b[%d;%dH└", start_row + dialog_height - 1, start_col);
    for (int i = 0; i < dialog_width - 2; i++) printf("─");
    printf("┘");

    // Текст
    printf("\x1b[%d;%dHSearch file:", start_row + 1, start_col + 2);
    printf("\x1b[%d;%dH%s", start_row + 3, start_col + 2, search_buffer);
}

int handle_search_dialog(char *search_query) {
    search_buffer[0] = 0;
    while (1) {
        draw_search_dialog();
        int c = get_input();
        if (c == '\n') {
            strcpy(search_query, search_buffer);
            return 1;
        } else if (c == 27) { // Esc
            return 0;
        } else if (c == 127) { // Backspace
            if (strlen(search_buffer) > 0) {
                search_buffer[strlen(search_buffer) - 1] = 0;
            }
        } else if (c >= 32 && c <= 126) { // Друковані символи
            int len = strlen(search_buffer);
            if (len < sizeof(search_buffer) - 1) {
                search_buffer[len] = c;
                search_buffer[len + 1] = 0;
            }
        }
    }
}
