// tv.c

#include "tv.h"

// Colors from socha.h
#define COLOR_HEADER "\x1b[1;97;104m"
#define COLOR_TEXT "\x1b[1;96;104m"
#define COLOR_RESET "\x1b[30;40m"
#define COLOR_LIGHT_BLUE "\x1b[104m"
#define COLOR_WHITE "\x1b[1;37m" // Bright white for text
#define COLOR_PINK_BG "\x1b[48;2;255;105;180m"

#define TAB_WIDTH  4

// Key codes from socha.h
#define KEY_TAB    9
#define KEY_ESC    1000
#define KEY_UP     1001
#define KEY_DOWN   1002
#define KEY_RIGHT  1003
#define KEY_LEFT   1004
#define KEY_PGUP   1005
#define KEY_PGDOWN 1006
#define KEY_HOME   1007
#define KEY_END    1008
#define KEY_ENTER  1021
#define KEY_BACKSPACE 1024
#define KEY_DELETE 1010
#define KEY_F1     1011
#define KEY_F2     1012
#define KEY_F3     1013
#define KEY_F4     1014
#define KEY_F5     1015
#define KEY_F6     1016
#define KEY_F7     1017
#define KEY_F8     1018
#define KEY_F9     1019
#define KEY_F10    1020
#define KEY_CTRL_LEFT  1025
#define KEY_CTRL_RIGHT 1026
#define KEY_INSERT 1009

#define MAX_LINE_SIZE (1ULL << 32)  // 4GB max line size
#define CHUNK_SIZE 64000             // Buffer read chunk size

// Global state
struct termios orig_termios;
int rows, cols;
volatile sig_atomic_t resize_flag = 0;
char filename[1024];
int fd = -1;
off_t file_size = 0;
int view_mode = 0;  // 0 = edit (default), 1 = view
int modified = 0;
int insert_mode = 1;  // 1 = insert, 0 = replace
int show_blanks = 1;  // Toggle for blank space display (F5)

// Line structure
typedef struct Line {
    char *data;         // Line content (UTF-8)
    size_t len;         // Byte length of line (excluding \n)
    size_t capacity;    // Allocated size
    size_t disp_len;    // Display length (number of columns), computed on demand
} Line;

// Line buffer
typedef struct {
    Line *lines;        // Array of lines
    size_t count;       // Number of lines
    size_t capacity;    // Allocated size
} LineBuffer;

LineBuffer buffer = {0};
size_t cursor_x = 0, cursor_y = 0;  // Cursor position (in byte offset)
int scroll_x = 0, scroll_y = 0;     // Scroll offsets (in display columns)
size_t last_cursor_x = -1;          // Last cursor byte offset
int last_cursor_y = -1;             // Last cursor line

// Prototypes
void enable_raw_mode();
void disable_raw_mode();
int get_window_size(int *r, int *c);
void handle_resize(int sig);
int get_input();
void load_file();
void draw_header();
void draw_footer();
void draw_menu(int selected);
int handle_menu();
void draw_text();
void update_line(int line);
void insert_char(char c);
void delete_char();
void save_file();
void move_cursor_word(int direction);
void free_buffer();

// Line buffer functions
void init_buffer() {
    buffer.lines = malloc(sizeof(Line) * 1024);
    buffer.count = 0;
    buffer.capacity = 1024;
}

void grow_buffer() {
    buffer.capacity *= 2;
    buffer.lines = realloc(buffer.lines, sizeof(Line) * buffer.capacity);
}

void add_line(char *data, size_t len) {
    if (buffer.count >= buffer.capacity) grow_buffer();
    Line *line = &buffer.lines[buffer.count++];
    line->data = malloc(len + 1);
    line->capacity = len + 1;
    memcpy(line->data, data, len);
    line->data[len] = '\0';  // Null-terminate for safety
    line->len = len;
    line->disp_len = 0;  // Compute on demand
}

void load_file() {
    init_buffer();
    char chunk[CHUNK_SIZE];
    off_t offset = 0;
    char *line_start = chunk;
    size_t line_len = 0;

    while (offset < file_size) {
        ssize_t bytes = pread(fd, chunk, CHUNK_SIZE, offset);
        if (bytes <= 0) break;

        for (size_t i = 0; i < (size_t)bytes; i++) {
            if (chunk[i] == '\n' || line_len >= MAX_LINE_SIZE) {
                add_line(line_start, line_len);
                line_start = chunk + i + 1;  // Point to the next byte after newline or split
                line_len = 0;
                // If we're at the end of the chunk, don't process further here
                if (i == (size_t)bytes - 1) {
                    line_start = chunk;  // Reset to start of next chunk
                    break;
                }
            } else {
                line_len++;
                // If this is the last byte and no newline, carry over to next chunk
                if (i == (size_t)bytes - 1 && offset + bytes < file_size) {
                    // Copy partial line to a temporary buffer if it spans chunks
                    char *temp = malloc(line_len);
                    memcpy(temp, line_start, line_len);
                    offset += bytes;
                    ssize_t next_bytes = pread(fd, chunk, CHUNK_SIZE, offset);
                    if (next_bytes > 0) {
                        size_t temp_len = line_len;
                        line_start = chunk;
                        line_len = 0;
                        memcpy(chunk, temp, temp_len);  // Prepend previous data
                        line_start = chunk + temp_len;  // Adjust start
                        bytes = next_bytes;
                        i = temp_len - 1;  // Continue from where we left off
                    }
                    free(temp);
                }
            }
        }
        // Handle any remaining data at the end of the chunk
        if (line_len > 0 && offset + bytes >= file_size) {
            add_line(line_start, line_len);
            line_len = 0;
        }
        offset += bytes;
    }
    if (buffer.count == 0) {
        add_line("", 0);  // Empty file
    }
}

void free_buffer() {
    for (size_t i = 0; i < buffer.count; i++) {
        free(buffer.lines[i].data);
    }
    free(buffer.lines);
    buffer.lines = NULL;
    buffer.count = buffer.capacity = 0;
}

// Terminal handling
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO | IEXTEN);
    raw.c_iflag &= ~(IXON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

int get_window_size(int *r, int *c) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) return -1;
    *r = ws.ws_row;
    *c = ws.ws_col;
    return 0;
}

void handle_resize(int sig) {
    resize_flag = 1;
}

int get_input() {
    int c = getchar();
    if (c == 27) {
        int c2 = getchar();
        if (c2 == '[') {
            int c3 = getchar();
            if (c3 == 'A') return KEY_UP;
            if (c3 == 'B') return KEY_DOWN;
            if (c3 == 'C') return KEY_RIGHT;
            if (c3 == 'D') return KEY_LEFT;
            if (c3 == 'H') return KEY_HOME;
            if (c3 == 'F') return KEY_END;
            if (c3 == '3' && getchar() == '~') return KEY_DELETE;
            if (c3 == '5' && getchar() == '~') return KEY_PGUP;
            if (c3 == '6' && getchar() == '~') return KEY_PGDOWN;
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
                    if (c4 == ';') {
                        int c5 = getchar();
                        if (c5 == '5') {
                           int c6 = getchar();
                           if (c6 == 'D') return KEY_CTRL_LEFT;
                           if (c6 == 'C') return KEY_CTRL_RIGHT;
                        }
                    }
                } else if (c3 == '2') {
                    if (c4 == '0') { getchar(); return KEY_F9; }  // F9: \033[20~
                    if (c4 == '1') { getchar(); return KEY_F10; } // F10: \033[21~
                    if (c4 == '~') { return KEY_INSERT; } // F10: \033[21~
                }
            }
        } else if (c2 == 'O') {
            int c3 = getchar();
            if (c3 == 'P') return KEY_F1;
            if (c3 == 'R') return KEY_F3;
            if (c3 == 'S') return KEY_F4;
            if (c3 == 'T') return KEY_F5;
            if (c3 == 'U') return KEY_F10;
        } else if (c2 == '2' && getchar() == '~') return KEY_INSERT;
        return KEY_ESC;
    } else if (c == '\n') return KEY_ENTER;
    else if (c == 127) return KEY_BACKSPACE;
    return c;
}

// UI drawing
void draw_header() {
    printf("\x1b[1;1H\x1b[33;44m▄%s%s TV \x1b[90;106m    [%s]    \x1b[37;46m    %s%s%s%-*s", COLOR_PINK_BG, COLOR_WHITE, filename,
        view_mode ? "[VIEW]" : "[EDIT]",
        view_mode ? "" : (insert_mode ? "[INSERTING]"
                                      : "[REPLACING]"), 
        modified ? "[+]" : "", cols - ((int)strlen(filename)), "");
    printf("\x1b[K");
}

void draw_footer() {
    printf("\x1b[%d;1H\x1b[37m\x1b[44m "
           "1\x1b[90;106m Help "
           "3\x1b[90;106m View "
           "4\x1b[90;106m Edit "
           "5\x1b[90;106m Blanks "
           "10\x1b[90;106m Exit %s", rows, COLOR_RESET);
    printf("\x1b[K");
}

void draw_menu(int selected) {
    const char *items[] = {"Edit Mode", "Save", "Exit"};
    int item_count = 3;
    int width = 20;
    int start_row = 2;
    int start_col = (cols - width) / 2;

    printf("\x1b[46m");
    for (int i = 0; i < item_count; i++) {
        if (i == selected) {
            printf("\x1b[%d;%dH\x1b[90;47m%-*s", start_row + i, start_col, width - 2, items[i]);
        } else {
            printf("\x1b[%d;%dH\x1b[97;46m%-*s", start_row + i, start_col, width - 2, items[i]);
        }
    }
    printf(COLOR_RESET);
}

void update_line(int line) {
    int buf_idx = scroll_y + line;
    int screen_row = line + 2;
    printf("\x1b[%d;1H\x1b[K", screen_row);  // Clear the entire line
    if (buf_idx >= buffer.count) return;

    Line *l = &buffer.lines[buf_idx];
    size_t byte_start = display_to_byte(l->data, scroll_x, l->len);
    size_t disp_len = 0;
    size_t byte_end = byte_start;
    while (byte_end < l->len && disp_len < cols) {
        size_t bytes;
        uint32_t cp = utf8_to_codepoint(l->data, byte_end, l->len, &bytes);
        disp_len += utf8_char_width(cp);
        byte_end += bytes;
    }
    if (byte_start < l->len) {
        printf("\x1b[%d;1H%s%.*s", screen_row, COLOR_TEXT, (int)(byte_end - byte_start), l->data + byte_start);
    }
    if (show_blanks && disp_len < cols) {
        printf("%s%*s%s", COLOR_LIGHT_BLUE, (int)(cols - disp_len), "", COLOR_RESET);
    } else {
        printf("%s", COLOR_RESET);
    }
}

void draw_text() {
    printf("\x1b[2;1H\x1b[J");  // Clear from cursor to end of screen
    for (int i = 0; i < rows - 2; i++) {
        update_line(i);
        if (i < rows - 3) printf("\n");  // Avoid extra newline on last line
    }
    // Clear cursor trail
    if (last_cursor_x != (size_t)-1 && last_cursor_y >= 0 && !view_mode) {
        int old_y = last_cursor_y - scroll_y;
        if (old_y >= 0 && old_y < rows - 2) {
            Line *l = &buffer.lines[last_cursor_y];
            size_t disp_x = byte_to_display(l->data, last_cursor_x, l->len);
            int x = disp_x - scroll_x;
            char c = (last_cursor_x < l->len) ? l->data[last_cursor_x] : ' ';
            if (x >= 0 && x < cols) {
                printf("\x1b[%d;%dH%s%c%s", old_y + 2, x + 1, COLOR_TEXT, c, COLOR_RESET);
            }
        }
    }

    // Draw new cursor
    if (!view_mode) {
        Line *l = &buffer.lines[cursor_y];
        size_t disp_x = byte_to_display(l->data, cursor_x, l->len);
        int x = disp_x - scroll_x;
        int cursor_row = cursor_y - scroll_y + 2;
        if (x >= 0 && x < cols && cursor_row >= 2 && cursor_row <= rows - 1) {
            size_t bytes;
            int width;
            uint32_t cp = get_utf8_char_at(l->data, cursor_x, l->len, &bytes, &width);
            printf("\x1b[%d;%dH\x1b[7m", cursor_row, x + 1);
            print_utf8_char(cp);
            printf("\x1b[0m");
            if (width > 1) printf("\x1b[%d;%dH", cursor_row, x + width + 1);
            printf("\x1b[%d;1H", cursor_row);
        }
    }

    last_cursor_x = cursor_x;
    last_cursor_y = cursor_y;
    // Ensure cursor is at a safe position after drawing
    printf("\x1b[%d;1H", rows);
}

// Editing functions
void insert_char(char c) {
    if (view_mode) return;
    Line *l = &buffer.lines[cursor_y];
    if (c == '\n') {
        if (buffer.count >= buffer.capacity) grow_buffer();
        memmove(&buffer.lines[cursor_y + 2], &buffer.lines[cursor_y + 1], 
                (buffer.count - cursor_y - 1) * sizeof(Line));
        buffer.count++;
        Line *new_line = &buffer.lines[cursor_y + 1];
        size_t tail_len = l->len - cursor_x;
        new_line->data = malloc(tail_len + 1);
        new_line->capacity = tail_len + 1;
        new_line->len = tail_len;
        if (tail_len > 0) memcpy(new_line->data, l->data + cursor_x, tail_len);
        new_line->data[tail_len] = '\0';
        l->len = cursor_x;
        l->data[l->len] = '\0';
        cursor_y++;
        cursor_x = 0;
        modified = 1;
        draw_text();
    } else {
        if (l->len >= l->capacity) {
            l->capacity = l->capacity ? l->capacity * 2 : 16;
            l->data = realloc(l->data, l->capacity);
        }
        if (insert_mode) {
            memmove(l->data + cursor_x + 1, l->data + cursor_x, l->len - cursor_x);
            l->data[cursor_x] = c;
            l->len++;
            cursor_x++;
        } else if (cursor_x < l->len) {
            l->data[cursor_x] = c;
            cursor_x += utf8_char_bytes(l->data, cursor_x, l->len);
        } else {
            l->data[cursor_x] = c;
            l->len = cursor_x + 1;
            cursor_x++;
        }
        l->data[l->len] = '\0';
        modified = 1;
        update_line(cursor_y - scroll_y);
    }
}

void delete_char() {
    if (view_mode) return;
    Line *l = &buffer.lines[cursor_y];
    if (cursor_x < l->len) {
        size_t bytes = utf8_char_bytes(l->data, cursor_x, l->len);
        memmove(l->data + cursor_x, l->data + cursor_x + bytes, l->len - cursor_x - bytes);
        l->len -= bytes;
        l->data[l->len] = '\0';
        modified = 1;
        update_line(cursor_y - scroll_y);
    } else if (cursor_y + 1 < buffer.count) {
        Line *next = &buffer.lines[cursor_y + 1];
        if (l->len + next->len >= l->capacity) {
            l->capacity = l->len + next->len + 1;
            l->data = realloc(l->data, l->capacity);
        }
        memcpy(l->data + l->len, next->data, next->len);
        l->len += next->len;
        l->data[l->len] = '\0';
        memmove(&buffer.lines[cursor_y + 1], &buffer.lines[cursor_y + 2], 
                (buffer.count - cursor_y - 2) * sizeof(Line));
        buffer.count--;
        free(next->data);
        modified = 1;
        draw_text();
    }
}

void save_file() {
    if (fd == -1 || view_mode) return;
    lseek(fd, 0, SEEK_SET);
    for (size_t i = 0; i < buffer.count; i++) {
        write(fd, buffer.lines[i].data, buffer.lines[i].len);
        if (i < buffer.count - 1) write(fd, "\n", 1);
    }
    ftruncate(fd, file_size);
    modified = 0;
}

void move_cursor_word(int direction) {
    Line *l = &buffer.lines[cursor_y];
    size_t x = cursor_x;
    if (direction < 0) {
        while (x > 0 && isspace(l->data[x - 1])) x--;
        while (x > 0 && !isspace(l->data[x - 1])) x--;
    } else {
        while (x < l->len && !isspace(l->data[x])) x += utf8_char_bytes(l->data, x, l->len);
        while (x < l->len && isspace(l->data[x])) x += utf8_char_bytes(l->data, x, l->len);
    }
    cursor_x = x;
    size_t disp_x = byte_to_display(l->data, cursor_x, l->len);
    if (disp_x < scroll_x) {
        scroll_x = disp_x;
        draw_text();
    } else if (disp_x >= scroll_x + cols) {
        scroll_x = disp_x - cols + 1;
        draw_text();
    }
}

// Menu handling
int handle_menu() {
    int selected = 0;
    while (1) {
        draw_header();
        draw_text();
        draw_menu(selected);
        int c = get_input();
        if (c == KEY_UP && selected > 0) selected--;
        else if (c == KEY_DOWN && selected < 2) selected++;
        else if (c == KEY_ENTER) {
            if (selected == 0) view_mode = 0;
            else if (selected == 1) save_file();
            else if (selected == 2) return 1;
            break;
        } else if (c == KEY_ESC) break;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: tv <filename>\n");
        return 1;
    }

    strncpy(filename, argv[1], sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = 0;

    struct stat st;
    if (stat(filename, &st) == 0) {
        file_size = st.st_size;
        fd = open(filename, O_RDWR);
    } else {
        fd = open(filename, O_RDWR | O_CREAT, 0644);
        file_size = 0;
    }
    if (fd == -1) {
        perror("Failed to open file");
        return 1;
    }

    enable_raw_mode();
    atexit(disable_raw_mode);
    signal(SIGWINCH, handle_resize);
    get_window_size(&rows, &cols);

    load_file();

    printf("\x1b[?1049h");

    while (1) {
        if (resize_flag) {
            get_window_size(&rows, &cols);
            if (cursor_y >= (size_t)(rows - 2)) cursor_y = rows - 3;
            if (scroll_y > buffer.count) scroll_y = buffer.count > 0 ? buffer.count - 1 : 0;
            resize_flag = 0;
            draw_text();
        }

        draw_header();
        draw_text();
        draw_footer();

        int c = get_input();
        if (c == KEY_F1) {
            // Help (placeholder)
        } else if (c == KEY_F3) {
            if (view_mode) {
                if (!modified || handle_menu()) break;
            } else {
                view_mode = 1;
                draw_header();
            }
        } else if (c == KEY_F4) {
            if (!view_mode) {
                if (!modified || handle_menu()) break;
            } else {
                view_mode = 0;
                draw_header();
            }
        } else if (c == KEY_F5) {
            show_blanks = !show_blanks;
            draw_text();
        } else if (c == KEY_F10) {
            if (!modified || handle_menu()) break;
        } else if (c == KEY_UP) {
            if (cursor_y > 0) {
                cursor_y--;
                size_t disp_x = byte_to_display(buffer.lines[cursor_y].data, cursor_x, buffer.lines[cursor_y].len);
                if (disp_x > utf8_display_length(buffer.lines[cursor_y].data, buffer.lines[cursor_y].len)) {
                    cursor_x = buffer.lines[cursor_y].len;
                }
                if (cursor_y < (size_t)scroll_y) {
                    scroll_y--;
                    draw_text();
                }
            }
        } else if (c == KEY_DOWN) {
            if (cursor_y + 1 < buffer.count) {
                cursor_y++;
                size_t disp_x = byte_to_display(buffer.lines[cursor_y].data, cursor_x, buffer.lines[cursor_y].len);
                if (disp_x > utf8_display_length(buffer.lines[cursor_y].data, buffer.lines[cursor_y].len)) {
                    cursor_x = buffer.lines[cursor_y].len;
                }
                if (cursor_y >= (size_t)(scroll_y + rows - 2)) {
                    scroll_y++;
                    if (scroll_y + rows - 2 > buffer.count) {
                        scroll_y = buffer.count > (size_t)(rows - 2) ? buffer.count - (rows - 2) : 0;
                    }
                    draw_text();
                }
            }
        } else if (c == KEY_LEFT) {
            if (cursor_x > 0) {
                size_t i = cursor_x;
                do {
                    i--;
                } while (i > 0 && (buffer.lines[cursor_y].data[i] & 0xC0) == 0x80);
                cursor_x = i;
                size_t disp_x = byte_to_display(buffer.lines[cursor_y].data, cursor_x, buffer.lines[cursor_y].len);
                if (disp_x < scroll_x) {
                    scroll_x--;
                    draw_text();
                }
            }
        } else if (c == KEY_RIGHT) {
            Line *l = &buffer.lines[cursor_y];
            if (cursor_x < l->len) {
                cursor_x += utf8_char_bytes(l->data, cursor_x, l->len);
                size_t disp_x = byte_to_display(l->data, cursor_x, l->len);
                if (disp_x >= scroll_x + cols) {
                    scroll_x++;
                    draw_text();
                }
            }
        } else if (c == KEY_CTRL_LEFT && !view_mode) {
            move_cursor_word(-1);
        } else if (c == KEY_CTRL_RIGHT && !view_mode) {
            move_cursor_word(1);
        } else if (c == KEY_PGUP) {
            if (scroll_y > 0) {
                scroll_y -= rows - 2;
                cursor_y -= rows - 2;
                if (scroll_y < 0) scroll_y = 0;
                if (cursor_y < 0) cursor_y = 0;
                size_t disp_x = byte_to_display(buffer.lines[cursor_y].data, cursor_x, buffer.lines[cursor_y].len);
                if (disp_x > utf8_display_length(buffer.lines[cursor_y].data, buffer.lines[cursor_y].len)) {
                    cursor_x = buffer.lines[cursor_y].len;
                }
                draw_text();
            }
        } else if (c == KEY_PGDOWN) {
            if (scroll_y + rows - 2 < (int)buffer.count) {
                scroll_y += rows - 2;
                cursor_y += rows - 2;
                if (cursor_y >= buffer.count) cursor_y = buffer.count - 1;
                if (scroll_y + rows - 2 > buffer.count) {
                    scroll_y = buffer.count > (size_t)(rows - 2) ? buffer.count - (rows - 2) : 0;
                }
                size_t disp_x = byte_to_display(buffer.lines[cursor_y].data, cursor_x, buffer.lines[cursor_y].len);
                if (disp_x > utf8_display_length(buffer.lines[cursor_y].data, buffer.lines[cursor_y].len)) {
                    cursor_x = buffer.lines[cursor_y].len;
                }
                draw_text();
            }
        } else if (c == KEY_HOME) {
            cursor_x = 0;
            scroll_x = 0;
            draw_text();
        } else if (c == KEY_END) {
            Line *l = &buffer.lines[cursor_y];
            cursor_x = l->len;
            size_t disp_x = byte_to_display(l->data, cursor_x, l->len);
            if (disp_x >= (size_t)cols) scroll_x = disp_x - cols + 1;
            else scroll_x = 0;
            draw_text();
        } else if (!view_mode) {
            if (c == KEY_INSERT) {
                insert_mode = !insert_mode;
                draw_header();
            } else if (c == KEY_BACKSPACE) {
                if (cursor_x > 0) {
                    size_t i = cursor_x;
                    do {
                        i--;
                    } while (i > 0 && (buffer.lines[cursor_y].data[i] & 0xC0) == 0x80);
                    cursor_x = i;
                    delete_char();
                } else if (cursor_y > 0) {
                    Line *prev = &buffer.lines[cursor_y - 1];
                    cursor_x = prev->len;
                    delete_char();
                    cursor_y--;
                }
            } else if (c == KEY_DELETE) {
                delete_char();
            } else if (c == KEY_TAB) {
                insert_char('\t');
            } else if (c >= 32 && c <= 126) {
                insert_char(c);
            } else if (c == KEY_ENTER) {
                insert_char('\n');
            }
        }
    }

    if (modified) save_file();
    close(fd);
    free_buffer();
    printf("\x1b[?1049l\x1b[2J\x1b[H");
    return 0;

}
