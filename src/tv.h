// tv.h

#ifndef TV_H
#define TV_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

size_t utf8_char_bytes(const char *data, size_t pos, size_t len);
size_t find_last_utf8_boundary(const char *buf, size_t len);
uint32_t utf8_to_codepoint(const char *data, size_t pos, size_t len, size_t *bytes);
int utf8_char_width(uint32_t cp);
size_t utf8_display_length(const char *data, size_t len);
size_t byte_to_display(const char *data, size_t byte_pos, size_t len);
size_t display_to_byte(const char *data, size_t disp_pos, size_t len);
uint32_t get_utf8_char_at(const char *data, size_t byte_pos, size_t len, size_t *bytes, int *width);
void print_utf8_char(uint32_t cp);

#endif
