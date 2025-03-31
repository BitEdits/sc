// menus.c

#include "sc.h"

void draw_panel_border(int start_col, int start_row, int width, int height, char *color) {
    if (color) printf("%s",color);

    // Верхня рамка
    printf("\x1b[%d;%dH┌", start_row, start_col); // Start at row 2 to leave space for top menu
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("┐");

    // Бокові рамки
    for (int i = start_row + 1; i < start_row + height - 1; i++) {
        printf("\x1b[%d;%dH│", i, start_col);
        printf("\x1b[%d;%dH│", i, start_col + width - 1);
    }

    // Нижня рамка
    printf("\x1b[%d;%dH└", start_row + height - 1, start_col);
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("┘");
}

void draw_panel(Panel *panel, int start_col, int width, int is_active) {
    int panel_height = rows - 3; // 1 row for top menu, 1 for command bar, 1 for bottom menu, 1 for padding
    int visible_files = panel_height - 5; // 2 rows for borders, 1 for path, 1 for column headers, 1 for status bar
    // Оновлення прокручування
    if (panel->cursor < panel->scroll_offset) {
        panel->scroll_offset = panel->cursor;
    } else if (panel->cursor >= panel->scroll_offset + visible_files) {
        panel->scroll_offset = panel->cursor - visible_files + 1;
    }

    // Малювання рамки
    draw_panel_border(start_col, 2, width, panel_height, COLOR_TEXT);

    // Заголовок панелі (шлях)
    printf("\x1b[2;%dH%s %s ", start_col + 1, COLOR_HEADER, panel->path);

    // Заголовки колонок
    int name_width = width - 4 - 23; // 65% для імені
    int size_width = 9 ; // 15% для розміру/типу
    int date_width = 14 ; // 20% для дати
    int sep1 = start_col + 1 + name_width;
    int sep2 = sep1 + size_width;

    printf("%s\x1b[3;%dH%-*s│%-*s│%-*s%s", COLOR_TEXT, start_col + 1, name_width, "Name", size_width, "Size", date_width, "Date Time", COLOR_RESET);

    // Сепаратори для колонок
    for (int i = 0; i < visible_files; i++) { // Reduced by 1 to avoid extra line
        printf("\x1b[%d;%dH│", 4 + i, sep1);
        printf("\x1b[%d;%dH│", 4 + i, sep2);
    }

    // Файли
    for (int i = 0; i < visible_files; i++) {
        int file_idx = i + panel->scroll_offset;

          if (file_idx >= panel->file_count) {
             // Заповнення порожнього простору
             printf("\x1b[%d;%dH%s", i + 4, start_col + 1, COLOR_TEXT);
             printf("%-*.*s│%*s│%*s", name_width, name_width, "", size_width, "", date_width, "");
             continue;
          }

        if (is_active && file_idx == panel->cursor) {
            printf("\x1b[%d;%dH%s", i + 4, start_col + 1, COLOR_HIGHLIGHT);
        } else {
            printf("\x1b[%d;%dH%s", i + 4, start_col + 1, panel->files[file_idx].is_dir ? COLOR_DIR : COLOR_TEXT);
        }

        char *name = panel->files[file_idx].name;
        char size_str[16];
        if (strcmp(name, "..") == 0) {
            snprintf(size_str, sizeof(size_str), "Up");
        } else if (panel->files[file_idx].is_dir) {
            snprintf(size_str, sizeof(size_str), "<dir>");
        } else {
            snprintf(size_str, sizeof(size_str), "%ld", panel->files[file_idx].size);
        }
        char datetime_str[20];
        strftime(datetime_str, sizeof(datetime_str), "%m-%d-%y|%H:%M", localtime(&panel->files[file_idx].mtime));

        // Форматування з сепараторами
        printf("%-*.*s│%*s│%*s", name_width, name_width, name, size_width, size_str, date_width, datetime_str);
    }

    // Горизонтальна лінія перед статусною строкою
    int status_row = 4 + visible_files;
    printf("%s\x1b[%d;%dH├", COLOR_TEXT, status_row, start_col);
    for (int i = 0; i < name_width; i++) printf("─");
    printf("┴");
    for (int i = 0; i < size_width; i++) printf("─");
    printf("┴");
    for (int i = 0; i < date_width; i++) printf("─");
    printf("┤");

    // Статусна строка панелі
    int total_files = 0, total_directories = 0;
    long long total_size = 0;
    for (int i = 0; i < panel->file_count; i++) {
        if (panel->files[i].is_dir) {
            if (strcmp(panel->files[i].name, "..") != 0) total_directories++;
        } else {
            total_files++;
            total_size += panel->files[i].size;
        }
    }
    char size_display[16];
    if (total_size < 1024) {
        snprintf(size_display, sizeof(size_display), "%lld B", total_size);
    } else if (total_size < 1024 * 1024) {
        snprintf(size_display, sizeof(size_display), "%.1f K", total_size / 1024.0);
    } else {
        snprintf(size_display, sizeof(size_display), "%.1f M", total_size / (1024.0 * 1024.0));
    }
    printf("\x1b[37m\x1b[%d;%dH %-*s ", status_row + 1, start_col + 1, cols / 2 - 4, " ");
    printf("\x1b[37m\x1b[%d;%dH Total: %s, files: %d, directories: %d.", status_row + 1, start_col + 1, size_display, total_files, total_directories);
}

void update_cursor(Panel *panel, int start_col, int width, int is_active, int prev_cursor) {
    int panel_height = rows - 4; // 1 row for top menu, 1 for command bar, 1 for bottom menu, 1 for padding
    int visible_files = panel_height - 5; // 2 rows for borders, 1 for path, 1 for column headers, 1 for status bar

    // Оновлення прокручування
    if (panel->cursor < panel->scroll_offset) {
        panel->scroll_offset = panel->cursor;
    } else if (panel->cursor >= panel->scroll_offset + visible_files) {
        panel->scroll_offset = panel->cursor - visible_files + 1;
    }

    // Сепаратори для регіонів
    int name_width = width - 4 - 23; // 65% для імені
    int size_width = 9 ; // 15% для розміру/типу
    int date_width = 14 ; // 20% для дати

    // Оновлюємо лише попередню і нову позиції курсора
    int prev_idx = prev_cursor - panel->scroll_offset;
    int curr_idx = panel->cursor - panel->scroll_offset;

    // Перемальовуємо попередню позицію (знімаємо підсвітку)
    if (prev_idx >= 0 && prev_idx < visible_files) {
        printf("\x1b[%d;%dH%s", prev_idx + 4, start_col + 1, panel->files[prev_cursor].is_dir ? COLOR_DIR : COLOR_TEXT);
        char *name = panel->files[prev_cursor].name;
        char size_str[16];
        if (strcmp(name, "..") == 0) {
            snprintf(size_str, sizeof(size_str), "Up");
        } else if (panel->files[prev_cursor].is_dir) {
            snprintf(size_str, sizeof(size_str), "<dir>");
        } else {
            snprintf(size_str, sizeof(size_str), "%ld", panel->files[prev_cursor].size);
        }
        char datetime_str[20];
        strftime(datetime_str, sizeof(datetime_str), "%m-%d-%y|%H:%M", localtime(&panel->files[prev_cursor].mtime));
        printf("%-*.*s│%*s│%*s", name_width, name_width, name, size_width, size_str, date_width, datetime_str);
    }

    // Перемальовуємо нову позицію (додаємо підсвітку)
    if (curr_idx >= 0 && curr_idx < visible_files) {
        printf("\x1b[%d;%dH%s", curr_idx + 4, start_col + 1, is_active ? COLOR_HIGHLIGHT : (panel->files[panel->cursor].is_dir ? COLOR_DIR : COLOR_TEXT));
        char *name = panel->files[panel->cursor].name;
        char size_str[16];
        if (strcmp(name, "..") == 0) {
            snprintf(size_str, sizeof(size_str), "Up");
        } else if (panel->files[panel->cursor].is_dir) {
            snprintf(size_str, sizeof(size_str), "<dir>");
        } else {
            snprintf(size_str, sizeof(size_str), "%ld", panel->files[panel->cursor].size);
        }
        char datetime_str[20];
        strftime(datetime_str, sizeof(datetime_str), "%m-%d-%y|%H:%M", localtime(&panel->files[panel->cursor].mtime));
        printf("%-*.*s│%*s│%*s", name_width, name_width, name, size_width, size_str, date_width, datetime_str);
    }
}

void append_to_history_display(const char *command, const char *output) {
    int max_display = rows - 4; // Залишаємо місце для меню, командного рядка і нижнього меню

    // Збираємо лише видимі рядки історії
    char *history_lines[16384*4];
    int line_count = 0;

    // Визначаємо, з якого рядка історії починати відображення
    int start_idx = history_count - max_display - history_display_offset;
    if (start_idx < 0) start_idx = 0;

    for (int i = start_idx; i < history_count && line_count < 16384; i++) {
        int idx = (history_start - history_count + i + MAX_HISTORY) % MAX_HISTORY;
        if (i == history_count - 1) {
            // Для останньої команди використовуємо передані command і output
            char command_line[1024*3];
            snprintf(command_line, sizeof(command_line), "> %s", command);
            history_lines[line_count] = strdup(command_line);
            line_count++;

            char output_copy[16384*4];
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
            char command_line[1024*3];
            snprintf(command_line, sizeof(command_line), "> %s", history[idx].command);
            history_lines[line_count] = strdup(command_line);
            line_count++;

            char output_copy[16384*4];
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

    // Відображаємо лише видимі рядки
    for (int i = 0; i < max_display && i < line_count; i++) {
        printf("\x1b[37;40m\x1b[%d;1H%-*s", i + 2, cols, history_lines[i]);
    }

    // Очищаємо залишок екрана
    for (int i = line_count; i < max_display; i++) {
        printf("\x1b[37;40m\x1b[%d;1H%-*s", i + 2, cols, "");
    }

    draw_command_line();
    draw_bottom_bar();

    // Звільняємо пам’ять
    for (int i = 0; i < line_count; i++) {
        free(history_lines[i]);
    }
}

void draw_interface() {
    // Скидаємо стан терміналу перед малюванням
    printf("\x1b[0m\x1b[2J\x1b[H"); // Скидаємо кольори, очищаємо екран, переміщаємо курсор у верхній лівий кут

    // Малюємо меню верхнього рівня
    draw_menu();

    if (show_command_buffer) {
        // Відображаємо історію команд і їх виводи
        int max_display = rows - 3; // Залишаємо місце для меню, командного рядка і нижнього меню

        // Збираємо всі рядки історії в один масив
        char *history_lines[16384];
        int line_count = 0;

        for (int i = 0; i < history_count && line_count < 16384; i++) {
            int idx = (history_start - history_count + i + MAX_HISTORY) % MAX_HISTORY;

            // Додаємо рядок команди
            char command_line[1024*3];
            snprintf(command_line, sizeof(command_line), "> %s", history[idx].command);
            history_lines[line_count] = strdup(command_line);
            line_count++;

            // Додаємо вивід команди
            char output_copy[16384*4];
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
            printf("\x1b[%d;1H%-*s", i + 2, cols, history_lines[start_line + i]);
        }

        // Очищаємо залишок екрана
        for (int i = line_count - start_line; i < max_display; i++) {
            printf("\x1b[37;40m\x1b[%d;1H%-*s", i + 2, cols, "");
        }

        // Звільняємо пам’ять
        for (int i = 0; i < line_count; i++) {
            free(history_lines[i]);
        }
    } else {
        // Панелі
        int panel_width = (cols - 1) / 2; // 1 для роздільника
        draw_panel(&left_panel, 1, panel_width + 1, active_panel == &left_panel);
        draw_panel(&right_panel, panel_width + 2, panel_width + 1, active_panel == &right_panel);
    }

   draw_command_line();
   draw_bottom_bar();
}

void draw_command_line() {
    // Командний рядок
    printf("\x1b[37;40m\x1b[%d;1H%-*s", rows - 1, cols, "");
    printf("\x1b[37;40m\x1b[%d;1H%s>%s", rows - 1, active_panel->path, command_buffer);
}

void draw_bottom_bar() {
    // Нижнє меню
    printf("\x1b[%d;1H\x1b[37m\x1b[44m∀ "
           "\x1b[37;40m 1\x1b[90;106m Help "
           "\x1b[37;40m 2\x1b[90;106m User "
           "\x1b[37;40m 3\x1b[90;106m View "
           "\x1b[37;40m 4\x1b[90;106m Edit "
           "\x1b[37;40m 5\x1b[90;106m Copy "
           "\x1b[37;40m 6\x1b[90;106m Move "
           "\x1b[37;40m 7\x1b[90;106m Make "
           "\x1b[37;40m 8\x1b[90;106m Delete "
           "\x1b[37;40m 9\x1b[90;106m Menu "
           "\x1b[37;40m10\x1b[90;106m Exit %s", rows, COLOR_RESET);
}

void draw_menu() {
    const char *menu_tabs[] = {"Left", "File", "Command", "Options", "Right"};
    int tab_count = 5;
    int start_col = 10;
    // Малюємо меню у верхній частині з зеленим фоном (як у MC)
    printf("\x1b[1;1H\x1b[33;44m▄%s%s SC \x1b[90;106m%-*s", COLOR_PINK_BG, COLOR_WHITE, cols - 5, ""); // Білий текст, зелений фон

    // Малюємо вкладки з фіксованим відступом
    for (int i = 0; i < tab_count; i++) {
        printf("\x1b[90;106m\x1b[1;%dH%s", start_col, menu_tabs[i]);
        start_col += strlen(menu_tabs[i]) + 3; // Фіксований відступ 3 символи
    }

    printf(COLOR_RESET);
}

void draw_submenu(const char *items[], int item_count, int start_row, int start_col, int selected) {
    int width = 36; // Фіксована ширина підменю
    int height = item_count + 2;
    char *color = "\x1b[96;46m";

    draw_panel_border(start_col, 2, width, height, color);

    for (int i = 0; i < item_count; i++) {
        if (i == selected) {
            printf("\x1b[%d;%dH\x1b[90;47m%-*s", start_row + 1 + i, start_col + 1, width - 2, items[i]);
        } else {
            printf("\x1b[97;46m");
            printf("\x1b[%d;%dH%-*s", start_row + 1 + i, start_col + 1, width - 2, items[i]);
        }
    }
}

int handle_menu() {
    const char *menu_tabs[] = {"Left", "File", "Command", "Options", "Right"};
    int tab_count = 5;
    int selected_tab = 0;
    int submenu_active = 0;
    int selected_item = 0;

    // Підменю
    const char *left_items[] = {
        "Info           C-x i",
        "Listing format...",
        "Sort order...",
        "Filter...      M-!",
        "Encoding...    M-e",
        "Panelize       C-r"
    };
    const char *file_items[] = {
        "User Manual        F1",
        "User Applications  F2",
        "View               F3",
        "Edit               F4",
        "Copy               F5",
        "Rename/Move        F6",
        "Mkdir              F7",
        "Delete             F8",
        "Menu               F9",
        "Exit               F10",
        "Chmod              C-x c",
        "Link               C-x l",
        "Edit symlink       C-x s",
        "Chown              C-x o",
        "Chattr             C-x e",
        "Select group       +",
        "Unselect group     -",
        "Invert selection   *",
    };
    const char *command_items[] = {
        "Command history   C-o",
        "Find file         C-s",
        "Swap panels       C-u",
    };
    const char *options_items[] = {
        "Layout...",
        "Panel options...",
        "Appearance...",
        "Confirmation...",
        "Display bits...",
        "Virtual FS..."
    };
    const char *right_items[] = {
        "Info           C-x i",
        "Listing format...",
        "Sort order...",
        "Filter...      M-!",
        "Encoding...    M-e",
        "Panelize       C-r"
    };

    int item_counts[] = {6, 18, 3, 6, 6};

    const char **submenus[] = {left_items, file_items, command_items, options_items, right_items};

    // Малюємо основний інтерфейс перед входом у модальність
    draw_interface();

    while (1) {
        // Малюємо меню
        draw_menu();

        // Підсвічуємо вибрану вкладку
        int tab_start_col = 10;
        for (int i = 0; i < selected_tab; i++) {
            tab_start_col += strlen(menu_tabs[i]) + 3;
        }
        printf("\x1b[1;%dH\x1b[90;47m%s", tab_start_col, menu_tabs[selected_tab]);

        // Якщо підменю активне, малюємо його
        if (submenu_active) {
            draw_submenu(submenus[selected_tab], item_counts[selected_tab], 2, tab_start_col, selected_item);
        }

        int c = get_input();
        if (submenu_active) {
            // Обробка введення в підменю
            if (c == KEY_UP && selected_item > 0) {
                selected_item--;
            } else if (c == KEY_DOWN && selected_item < item_counts[selected_tab] - 1) {
                selected_item++;
            } else if (c == '\n') {
                // Виконуємо дію
                if (selected_tab == 0) { // Left
                    if (selected_item == 0) { // Listing
                        // Реалізувати
                    }
                } else if (selected_tab == 1) { // File
                    if (selected_item == 3) { // Edit (F4)
                        char cmd[1024*5];
                        snprintf(cmd, sizeof(cmd), "mcedit %s/%s", active_panel->path, active_panel->files[active_panel->cursor].name);
                        disable_raw_mode();
                        system(cmd);
                        enable_raw_mode();
                        return 0;
                    } else if (selected_item == 9) { // Exit (F10)
                        if (handle_exit_dialog()) {
                           printf("\x1b[?1049l");
                           printf("\x1b[2J\x1b[H");
                           exit(0);
                        } else return 0;
                    }
                } else if (selected_tab == 2) { // Command
                    if (selected_item == 0) { // Command history (C-o)
                        show_command_buffer = 1;
                        history_scroll_pos = history_count;
                        history_display_offset = 0;
                        draw_interface();
                        return 0;
                    }
                } else if (selected_tab == 3) { // Options
                    // Реалізувати
                } else if (selected_tab == 4) { // Right
                    if (selected_item == 0) { // Listing
                        // Реалізувати
                    }
                }
                submenu_active = 0; // Закриваємо підменю після виконання дії
            } else if (c == 27 || c == KEY_LEFT || c == KEY_RIGHT) { // Esc або стрілки вліво/вправо
                printf(COLOR_TEXT);
                if (c == KEY_LEFT && selected_tab > 0) {
                    selected_tab--;
                    selected_item = 0; // Скидаємо вибір на початок нового підменю
                } else if (c == KEY_RIGHT && selected_tab < tab_count - 1) {
                    selected_tab++;
                    selected_item = 0; // Скидаємо вибір на початок нового підменю
                } else if (c == 27) {
                    submenu_active = 0;
                    break; // Esc
                }
                draw_interface();
            }
        } else {
            printf(COLOR_TEXT);
            if (c == KEY_LEFT && selected_tab > 0) {
                selected_tab--;
            } else if (c == KEY_RIGHT && selected_tab < tab_count - 1) {
                selected_tab++;
            } else if (c == 13 || c == KEY_DOWN) {
                submenu_active = 1;
                selected_item = 0;
            } else if (c == 27) { // Esc
                break;
            }
            draw_interface();
        }
    }
    return 0;
}

void draw_exit_dialog(int selected_button) {
    int dialog_width = 50;
    int dialog_height = 5;
    int start_row = (rows - dialog_height) / 2;
    int start_col = (cols - dialog_width) / 2;

    // Малюємо заповнений фон рожевого кольору
    for (int i = 0; i < dialog_height; i++) {
        printf("\x1b[%d;%dH%s", start_row + i, start_col, COLOR_PINK_BG);
        for (int j = 0; j < dialog_width; j++) {
            printf(" ");
        }
    }

    // Малюємо тонку рамку магентового кольору
    draw_panel_border(start_col, start_row, dialog_width, dialog_height, COLOR_MAGENTA);

    // Текст (білий колір)
    printf("%s", COLOR_WHITE);
    const char *message = "Do you want to exit Sokhatsky Commander?";
    int message_col = start_col + (dialog_width - strlen(message)) / 2;
    printf("\x1b[%d;%dH%s", start_row + 1, message_col, message);

    // Кнопки [Yes] і [No]
    int button_row = start_row + 3;
    int yes_col = start_col + (dialog_width / 2) - 8; // Центруємо кнопки
    int no_col = start_col + (dialog_width / 2) + 2;

    // Кнопка [Yes]
    if (selected_button == 0) {
        printf("\x1b[%d;%dH%s[Yes]%s", button_row, yes_col, COLOR_BUTTON_HIGHLIGHT, COLOR_RESET);
    } else {
        printf("\x1b[%d;%dH%s[Yes]%s", button_row, yes_col, COLOR_WHITE, COLOR_RESET);
    }

    // Кнопка [No]
    if (selected_button == 1) {
        printf("\x1b[%d;%dH%s[No]%s", button_row, no_col, COLOR_BUTTON_HIGHLIGHT, COLOR_RESET);
    } else {
        printf("\x1b[%d;%dH%s[No]%s", button_row, no_col, COLOR_WHITE, COLOR_RESET);
    }

    printf("%s", COLOR_RESET); // Reset colors after drawing
}

int handle_exit_dialog() {
    int selected_button = 1; // 0 = Yes, 1 = No
    draw_exit_dialog(selected_button);

    while (1) {
        int c = get_input();
        if (c == 'y' || c == 'Y') {
            return 1; // Yes
        } else if (c == 'n' || c == 'N' || c == 27) { // Esc
            return 0; // No
        } else if (c == '\t' || c == KEY_RIGHT || c == KEY_LEFT) { // Tab
            selected_button = (selected_button + 1) % 2; // Перемикаємо між Yes і No
            draw_exit_dialog(selected_button);
        } else if (c == '\n') { // Enter
            return (selected_button == 0) ? 1 : 0; // Yes if selected_button is 0, No if 1
        }
    }
}
