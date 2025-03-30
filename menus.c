// menus.c

#include "sc.h"

void draw_panel_border(int start_col, int width, int height) {
    printf("%s", COLOR_TEXT);
    // Верхня рамка
    printf("\x1b[2;%dH┌", start_col); // Start at row 2 to leave space for top menu
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("┐");

    // Бокові рамки
    for (int i = 0; i < height - 2; i++) {
        printf("\x1b[%d;%dH│", 3 + i, start_col);
        printf("\x1b[%d;%dH│", 3 + i, start_col + width - 1);
    }

    // Нижня рамка
    printf("\x1b[%d;%dH└", 1 + height, start_col);
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("┘");
}

void draw_panel(Panel *panel, int start_col, int width, int is_active) {
    int panel_height = rows - 4; // 1 row for top menu, 1 for command bar, 1 for bottom menu, 1 for padding
    int visible_files = panel_height - 5; // 2 rows for borders, 1 for path, 1 for column headers, 1 for status bar

    // Оновлення прокручування
    if (panel->cursor < panel->scroll_offset) {
        panel->scroll_offset = panel->cursor;
    } else if (panel->cursor >= panel->scroll_offset + visible_files) {
        panel->scroll_offset = panel->cursor - visible_files + 1;
    }

    // Малювання рамки
    draw_panel_border(start_col, width, panel_height);

    // Заголовок панелі (шлях)
    printf("\x1b[2;%dH%s %s ", start_col + 1, COLOR_HEADER, panel->path);

    // Заголовки колонок
    int name_width = width - 4 - 23; // 65% для імені
    int size_width = 9 ; // 15% для розміру/типу
    int date_width = 14 ; // 20% для дати
    int sep1 = start_col + 1 + name_width;
    int sep2 = sep1 + size_width;

    printf("\x1b[3;%dH%-*s│%-*s│%-*s", start_col + 1, name_width, "Name", size_width, "Size", date_width, "Date Time");

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
    printf("\x1b[%d;%dH├", status_row, start_col);
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
    printf("\x1b[%d;%dH Total: %s, files: %d, directories: %d. ", status_row + 1, start_col + 1, size_display, total_files, total_directories);
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
    struct timespec start, end;
    double elapsed;

    // Start timing
    clock_gettime(CLOCK_MONOTONIC, &start);

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
        printf("\x1b[%d;1H%-*s", i + 2, cols, history_lines[i]);
    }

    // Очищаємо залишок екрана
    for (int i = line_count; i < max_display; i++) {
        printf("\x1b[%d;1H%-*s", i + 2, cols, "");
    }

    // Оновлюємо командний рядок
    printf("\x1b[%d;1H%-*s", rows - 2, cols, "");
    printf("\x1b[%d;1H%s%s>%s", rows - 2, COLOR_TEXT, active_panel->path, command_buffer);

    // Оновлюємо нижнє меню
    printf("\x1b[%d;1H\x1b[1;37m\x1b[42m%-*s", rows - 1, cols, "");
    printf("\x1b[%d;1H\x1b[1;37m\x1b[42m↑1Help 2UserMn 3View 4Edit 5Copy 6RenMov 7MkFold 8Delete 9ConfMn 10Quit 11Plugin 12Screen%s", rows - 1, COLOR_RESET);

    // Звільняємо пам’ять
    for (int i = 0; i < line_count; i++) {
        free(history_lines[i]);
    }

    // Time after history display
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    fprintf(stderr, "Time to display history (optimized): %.3f seconds\n", elapsed);
}

void draw_interface() {
    // Скидаємо стан терміналу перед малюванням
    printf("\x1b[0m\x1b[2J\x1b[H"); // Скидаємо кольори, очищаємо екран, переміщаємо курсор у верхній лівий кут

    // Малюємо меню верхнього рівня
    draw_menu();

    if (show_command_buffer) {
        // Відображаємо історію команд і їх виводи
        int max_display = rows - 4; // Залишаємо місце для меню, командного рядка і нижнього меню

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
            printf("\x1b[%d;1H%-*s", i + 2, cols, "");
        }

        // Звільняємо пам’ять
        for (int i = 0; i < line_count; i++) {
            free(history_lines[i]);
        }
    } else {
        // Панелі
        int panel_width = (cols - 1) / 2; // 1 для роздільника
        draw_panel(&left_panel, 1, panel_width, active_panel == &left_panel);
        draw_panel(&right_panel, panel_width + 2, panel_width, active_panel == &right_panel);
    }

    // Командний рядок
    printf("\x1b[%d;1H%-*s", rows - 2, cols, "");
    printf("\x1b[%d;1H%s%s>%s", rows - 2, COLOR_TEXT, active_panel->path, command_buffer);

    // Нижнє меню
    printf("\x1b[%d;1H\x1b[1;37m\x1b[42m%-*s", rows - 1, cols, "");
    printf("\x1b[%d;1H\x1b[1;37m\x1b[42m↑1Help 2UserMn 3View 4Edit 5Copy 6RenMov 7MkFold 8Delete 9ConfMn 10Quit 11Plugin 12Screen%s", rows - 1, COLOR_RESET);
}

void draw_menu() {
    const char *menu_tabs[] = {"Left", "File", "Command", "Options", "Right"};
    int tab_count = 5;
    int start_col = 10;
    // Малюємо меню у верхній частині з зеленим фоном (як у MC)
    printf("\x1b[1;1H\x1b[1;45m\x1b[37m%-*s", cols, "▄ SC"); // Білий текст, зелений фон

    // Малюємо вкладки з фіксованим відступом
    for (int i = 0; i < tab_count; i++) {
        printf("\x1b[1;%dH%s", start_col, menu_tabs[i]);
        start_col += strlen(menu_tabs[i]) + 3; // Фіксований відступ 3 символи
    }
}

void draw_submenu(const char *items[], int item_count, int start_row, int start_col, int selected) {
    int width = 36; // Фіксована ширина підменю
    int height = item_count + 2;

    // Малюємо рамку
    printf("%s", COLOR_TEXT);
    printf("\x1b[%d;%dH┌", start_row, start_col);
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("┐");

    for (int i = 0; i < height - 2; i++) {
        printf("\x1b[%d;%dH│", start_row + 1 + i, start_col);
        printf("\x1b[%d;%dH│", start_row + 1 + i, start_col + width - 1);
    }

    printf("\x1b[%d;%dH└", start_row + height - 1, start_col);
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("┘");

    // Відображаємо пункти підменю
    for (int i = 0; i < item_count; i++) {
        if (i == selected) {
            printf("\x1b[%d;%dH\x1b[1;30m\x1b[47m%-*s\x1b[0m", start_row + 1 + i, start_col + 1, width - 2, items[i]);
        } else {
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
        "View           F3",
        "Edit           F4",
        "Copy           F5",
        "Chmod          C-x c",
        "Link           C-x l",
        "Edit symlink   C-x s",
        "Chown          C-x o",
        "Advanced chown ",
        "Chattr         C-x e",
        "Rename/Move    F6",
        "Mkdir          F7",
        "Delete         F8",
        "Select group   +",
        "Unselect group -",
        "Invert selection *",
        "Exit           F10"
    };
    const char *command_items[] = {
        "User menu      F2",
        "<dir> tree ",
        "Find file      M-?",
        "Swap panels    C-u",
        "Switch panels on/off C-o",
        "Compare directories C-x d",
        "External panelize C-x !",
        "Command history M-h",
        "Viewed/edited files history M-E",
        "Directory hotlist C-\\",
        "Background jobs C-x j",
        "Screen list    M-'"
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

    int item_counts[] = {6, 16, 12, 6, 6};

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
        printf("\x1b[1;%dH\x1b[1;30m\x1b[47m%s\x1b[1;37m\x1b[42m", tab_start_col, menu_tabs[selected_tab]);

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
                    if (selected_item == 0) { // View (F3)
                        // Реалізувати
                    } else if (selected_item == 3) { // Edit (F4)
                        char cmd[1024*5];
                        snprintf(cmd, sizeof(cmd), "./be %s", active_panel->path);
                        disable_raw_mode();
                        system(cmd);
                        enable_raw_mode();
                        return 0; // Виходимо з меню після виконання дії
                    } else if (selected_item == 20) { // Exit (F10)
                        return 1; // Сигналізуємо вихід
                    }
                } else if (selected_tab == 2) { // Command
                    if (selected_item == 0) { // User menu (F2)
                        // Реалізувати
                    } else if (selected_item == 8) { // Command history (M-h)
                        show_command_buffer = 1;
                        history_scroll_pos = history_count;
                        history_display_offset = 0;
                        return 0; // Виходимо з меню
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
                if (c == KEY_LEFT && selected_tab > 0) {
                    selected_tab--;
                    selected_item = 0; // Скидаємо вибір на початок нового підменю
                    // Перемальовуємо панелі, щоб прибрати попереднє підменю
                    int panel_width = (cols - 1) / 2;
                    draw_panel(&left_panel, 1, panel_width, active_panel == &left_panel);
                    draw_panel(&right_panel, panel_width + 2, panel_width, active_panel == &right_panel);
                    for (int i = 2; i < rows - 2; i++) {
                        printf("\x1b[%d;%dH│", i, panel_width + 1);
                    }
                }
                if (c == KEY_RIGHT && selected_tab < tab_count - 1) {
                    selected_tab++;
                    selected_item = 0; // Скидаємо вибір на початок нового підменю
                    // Перемальовуємо панелі, щоб прибрати попереднє підменю
                    int panel_width = (cols - 1) / 2;
                    draw_panel(&left_panel, 1, panel_width, active_panel == &left_panel);
                    draw_panel(&right_panel, panel_width + 2, panel_width, active_panel == &right_panel);
                    for (int i = 2; i < rows - 2; i++) {
                        printf("\x1b[%d;%dH│", i, panel_width + 1);
                    }
                }
                if (c == 27) {
                    submenu_active = 0;
                    break; // Esc
                }
            }
        } else {
            // Обробка введення в головному меню
            if (c == KEY_LEFT && selected_tab > 0) {
                selected_tab--;
                // Перемальовуємо панелі, щоб прибрати попереднє підменю
                int panel_width = (cols - 1) / 2;
                draw_panel(&left_panel, 1, panel_width, active_panel == &left_panel);
                draw_panel(&right_panel, panel_width + 2, panel_width, active_panel == &right_panel);
                for (int i = 2; i < rows - 2; i++) {
                    printf("\x1b[%d;%dH│", i, panel_width + 1);
                }
            } else if (c == KEY_RIGHT && selected_tab < tab_count - 1) {
                selected_tab++;
                // Перемальовуємо панелі, щоб прибрати попереднє підменю
                int panel_width = (cols - 1) / 2;
                draw_panel(&left_panel, 1, panel_width, active_panel == &left_panel);
                draw_panel(&right_panel, panel_width + 2, panel_width, active_panel == &right_panel);
                for (int i = 2; i < rows - 2; i++) {
                    printf("\x1b[%d;%dH│", i, panel_width + 1);
                }
            } else if (c == '\n' || c == KEY_DOWN) {
                submenu_active = 1;
                selected_item = 0;
            } else if (c == 27) { // Esc
                break;
            }
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
    printf("%s", COLOR_MAGENTA);
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
