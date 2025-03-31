// sc.h

#ifndef SC_H
#define SC_H

#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>

// Кольори
#define COLOR_HEADER "\x1b[1;97;104m"
#define COLOR_TEXT "\x1b[1;96;104m"
#define COLOR_HIGHLIGHT "\x1b[1;30m\x1b[47m"
#define COLOR_DIR "\x1b[1;97;104m"
#define COLOR_RESET "\x1b[30;40m"

#define COLOR_PINK_BG "\x1b[48;2;255;105;180m"
#define COLOR_BUTTON_HIGHLIGHT "\x1b[1;37m\x1b[44m"
#define COLOR_MAGENTA "\x1b[35m" // Magenta for border
#define COLOR_WHITE "\x1b[1;37m" // Bright white for text

// Коди для стрілок та інших клавіш
#define KEY_UP     1000
#define KEY_DOWN   1001
#define KEY_RIGHT  1002
#define KEY_LEFT   1003
#define KEY_PGUP   1004
#define KEY_PGDOWN 1005
#define KEY_HOME   1006
#define KEY_END    1007
#define KEY_F1     1008
#define KEY_F2     1009
#define KEY_F3     1010
#define KEY_F4     1011
#define KEY_F5     1012
#define KEY_F6     1013
#define KEY_F7     1014
#define KEY_F8     1015
#define KEY_F9     1016
#define KEY_F10    1017

#define MAX_FILES   1000 // Максимальна кількість файлів у директорії
#define MAX_HISTORY 1000 // Максимальна кількість команд в історії

// Структура для файлу
typedef struct {
    char name[256];
    off_t size;
    time_t mtime;
    int is_dir;
    mode_t mode; // Права доступу
} File;

// Структура для збереження позиції при вході в директорію
typedef struct {
    char parent_path[1024*2];
    char dir_name[2560];
    int cursor_pos;
} DirHistory;

// Структура для панелі
typedef struct {
    char path[1024*4];
    File files[MAX_FILES];
    int file_count;
    int cursor;
    int scroll_offset;
    int selected[MAX_FILES];
    int sort_type; // 0 - за ім'ям, 1 - за розміром, 2 - за датою
    DirHistory dir_history[MAX_FILES]; // Історія входу в директорії
    int dir_history_count;
} Panel;

// Структура для історії команд і їх виводів
typedef struct {
    char command[1024*2];
    char output[16384*4]; // Буфер для виводів
} CommandEntry;

// Глобальні змінні
extern struct termios orig_termios;
extern Panel left_panel, right_panel;
extern Panel *active_panel;
extern int rows, cols;
extern CommandEntry history[MAX_HISTORY];
extern int history_count, history_pos;
extern int history_start; // Індекс початку кільцевого буфера
extern char command_buffer[1024*4];
extern volatile sig_atomic_t resize_flag;
extern int show_command_buffer;
extern int history_scroll_pos; // Позиція прокручування історії команд
extern int history_display_offset; // Зміщення для відображення історії (для скролінгу)

// Функції з input.c
int get_input();
void execute_command(const char *cmd);

// Функції з menus.c
void draw_interface();
void update_cursor(Panel *panel, int start_col, int width, int is_active, int prev_cursor);
void draw_panel(Panel *panel, int start_col, int width, int is_active);
void draw_panel_border(int start_col, int width, int height);
void draw_exit_dialog();
int handle_exit_dialog();
void draw_menu();
int handle_menu();
void append_to_history_display(const char *command, const char *output);

// Функції з files.c
void load_files(Panel *panel);
int compare_files(const void *a, const void *b);

// Функції з sc.c
void enable_raw_mode();
void disable_raw_mode();
void get_window_size(int *r, int *c);
void handle_resize(int sig);

#endif
