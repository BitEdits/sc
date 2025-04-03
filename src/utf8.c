// utf8.c library by Sokhatsky (200 LOC)

#include <stdio.h>
#include <stdint.h>

extern size_t utf8_char_bytes(const char *data, size_t pos, size_t len);
extern size_t find_last_utf8_boundary(const char *buf, size_t len);
extern uint32_t utf8_to_codepoint(const char *data, size_t pos, size_t len, size_t *bytes);
extern int utf8_char_width(uint32_t cp);
extern size_t utf8_display_length(const char *data, size_t len);
extern size_t byte_to_display(const char *data, size_t byte_pos, size_t len);
extern size_t display_to_byte(const char *data, size_t disp_pos, size_t len);
extern uint32_t get_utf8_char_at(const char *data, size_t byte_pos, size_t len, size_t *bytes, int *width);
extern void print_utf8_char(uint32_t cp);

#define TAB_WIDTH 4

size_t utf8_char_bytes(const char *data, size_t pos, size_t len) {
    if (pos >= len) return 1;
    unsigned char c = data[pos];
    return (c & 0x80) ? ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : 4)) : 1;
}

// Helper function to find the last complete UTF-8 character in a buffer
size_t find_last_utf8_boundary(const char *buf, size_t len) {
    if (len == 0) return 0;

    size_t i = len;
    while (i > 0) {
        i--;
        unsigned char byte = (unsigned char)buf[i];
        if ((byte & 0x80) == 0) {  // 1-byte character (ASCII)
            return i + 1;
        } else if ((byte & 0xE0) == 0xC0) {  // 2-byte character
            if (i + 1 < len && (unsigned char)(buf[i + 1] & 0xC0) == 0x80) {
                return i + 2;
            }
        } else if ((byte & 0xF0) == 0xE0) {  // 3-byte character
            if (i + 2 < len && (unsigned char)(buf[i + 1] & 0xC0) == 0x80 && (unsigned char)(buf[i + 2] & 0xC0) == 0x80) {
                return i + 3;
            }
        } else if ((byte & 0xF8) == 0xF0) {  // 4-byte character
            if (i + 3 < len && (unsigned char)(buf[i + 1] & 0xC0) == 0x80 && (unsigned char)(buf[i + 2] & 0xC0) == 0x80 && (unsigned char)(buf[i + 3] & 0xC0) == 0x80) {
                return i + 4;
            }
        }
    }
    return 0;
}

uint32_t utf8_to_codepoint(const char *data, size_t pos, size_t len, size_t *bytes) {
    if (pos >= len) {
        *bytes = 1;
        return 0;
    }
    unsigned char c = data[pos];
    if (!(c & 0x80)) {
        *bytes = 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && pos + 1 < len) {
        *bytes = 2;
        return ((c & 0x1F) << 6) | (data[pos + 1] & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && pos + 2 < len) {
        *bytes = 3;
        return ((c & 0x0F) << 12) | ((data[pos + 1] & 0x3F) << 6) | (data[pos + 2] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && pos + 3 < len) {
        *bytes = 4;
        return ((c & 0x07) << 18) | ((data[pos + 1] & 0x3F) << 12) | ((data[pos + 2] & 0x3F) << 6) | (data[pos + 3] & 0x3F);
    }
    *bytes = 1;  // Invalid, treat as single byte
    return 0;
}

int utf8_char_width(uint32_t cp) {
    if (cp >= 0x00000 && cp <= 0x0007F) return 1; // Basic Latin (ASCII, e.g., a, 1, #)
    if (cp >= 0x00080 && cp <= 0x000FF) return 1; // Latin-1 Supplement (e.g., é, ñ)
    if (cp >= 0x00100 && cp <= 0x0017F) return 1; // Latin Extended-A (e.g., ě, ł)
    if (cp >= 0x00180 && cp <= 0x0024F) return 1; // Latin Extended-B (e.g., ƀ, ɏ)
    if (cp >= 0x00250 && cp <= 0x002AF) return 1; // IPA Extensions (e.g., ɐ, ʯ)
    if (cp >= 0x00300 && cp <= 0x0036F) return 0; // Combining Diacritical Marks (e.g., ◌́, ◌̈)
    if (cp >= 0x00370 && cp <= 0x003FF) return 1; // Greek and Coptic (e.g., π, Ω)
    if (cp >= 0x00400 && cp <= 0x004FF) return 1; // Cyrillic (e.g., б, я)
    if (cp >= 0x00500 && cp <= 0x0052F) return 1; // Cyrillic Supplement (e.g., ԯ, Ԯ)
    if (cp >= 0x00530 && cp <= 0x0058F) return 1; // Armenian (e.g., Ա, Ֆ)
    if (cp >= 0x00590 && cp <= 0x005FF) return 1; // Hebrew (e.g., א, ת)
    if (cp >= 0x00600 && cp <= 0x006FF) return 1; // Arabic (e.g., أ, ي)
    if (cp >= 0x00700 && cp <= 0x0074F) return 1; // Syriac (e.g., ܐ, ܯ)
    if (cp >= 0x00780 && cp <= 0x007BF) return 1; // Thaana (e.g., ހ,  )
    if (cp >= 0x0900 && cp <= 0x097F) { // Devanagari (e.g., अ, ह)
        if ((cp >= 0x093C && cp <= 0x094D) || (cp >= 0x0951 && cp <= 0x0957)) return 0;
        return 1;
    }
    if (cp >= 0x00980 && cp <= 0x009FF) return 1; // Bengali (e.g., অ, হ)
    if (cp >= 0x00A00 && cp <= 0x00A7F) return 1; // Gurmukhi (e.g., ਅ, ਹ)
    if (cp >= 0x00A80 && cp <= 0x00AFF) return 1; // Gujarati (e.g., અ, હ)
    if (cp >= 0x00B00 && cp <= 0x00B7F) return 1; // Oriya (e.g., ଅ, ହ)
    if (cp >= 0x00B80 && cp <= 0x00BFF) return 1; // Tamil (e.g., அ, ஹ)
    if (cp >= 0x00C00 && cp <= 0x00C7F) return 1; // Telugu (e.g., అ, హ)
    if (cp >= 0x00C80 && cp <= 0x00CFF) return 1; // Kannada (e.g., ಅ, ಹ)
    if (cp >= 0x00D00 && cp <= 0x00D7F) return 1; // Malayalam (e.g., അ, ഹ)
    if (cp >= 0x00E00 && cp <= 0x00E7F) return 1; // Thai (e.g., ก, ๏)
    if (cp >= 0x0E00 && cp <= 0x0E7F) {
        if (cp == 0x0E31 || (cp >= 0x0E34 && cp <= 0x0E3A) || (cp >= 0x0E47 && cp <= 0x0E4E) || cp == 0x0E46) return 0;
        return 1;
    }
    if (cp >= 0x00E80 && cp <= 0x00EFF) return 1; // Lao (e.g., ກ, ໝ)
    if (cp >= 0x010A0 && cp <= 0x010FF) return 1; // Georgian (e.g., Ⴀ, ჿ)
    if (cp >= 0x01100 && cp <= 0x011FF) return 2; // Hangul Jamo (e.g., ᄀ, ᇿ)
    if (cp >= 0x01E00 && cp <= 0x01EFF) return 1; // Latin Extended Additional (e.g., Ḁ, ỿ)
    if (cp >= 0x01F00 && cp <= 0x01FFF) return 1; // Greek Extended (e.g., ἀ, ῾)
    if (cp >= 0x02000 && cp <= 0x0206F) return 1; // General Punctuation (e.g., —,  )
    if (cp >= 0x02070 && cp <= 0x0209F) return 1; // Superscripts and Subscripts (e.g., ⁰, ₜ)
    if (cp >= 0x020A0 && cp <= 0x020CF) return 1; // Currency Symbols (e.g., €, ₿)
    if (cp >= 0x02100 && cp <= 0x0214F) return 1; // Letterlike Symbols (e.g., ℀, ℏ)
    if (cp >= 0x02150 && cp <= 0x0218F) return 1; // Number Forms (e.g., ⅐, ↉)
    if (cp >= 0x02190 && cp <= 0x021FF) return 1; // Arrows (e.g., →, ↻)
    if (cp >= 0x02200 && cp <= 0x022FF) return 1; // Mathematical Operators (e.g., ∀, ∮)
    if (cp >= 0x02300 && cp <= 0x023FF) return 1; // Miscellaneous Technical (e.g., ⌂, ⏿)
    if (cp >= 0x02460 && cp <= 0x024FF) return 1; // Enclosed Alphanumerics (e.g., ①, ⓿)
    if (cp >= 0x02500 && cp <= 0x0257F) return 1; // Box Drawing (e.g., ─, ╿)
    if (cp >= 0x02580 && cp <= 0x0259F) return 1; // Block Elements (e.g., ▀, ▟)
    if (cp >= 0x025A0 && cp <= 0x025FF) return 1; // Geometric Shapes (e.g., ■, ◿)
    if (cp >= 0x02600 && cp <= 0x026FF) return 1; // Miscellaneous Symbols (e.g., ☀, ⛿)
    if (cp >= 0x02700 && cp <= 0x027BF) return 1; // Dingbats (e.g., ✀, ➿)
    if (cp >= 0x02E80 && cp <= 0x02EFF) return 2; // CJK Radicals Supplement (e.g., ⺀,  )
    if (cp >= 0x02F00 && cp <= 0x02FDF) return 2; // Kangxi Radicals (e.g., ⼀,  )
    if (cp >= 0x03000 && cp <= 0x0303F) return 2; // CJK Symbols and Punctuation (e.g., 、, 〿)
    if (cp >= 0x03040 && cp <= 0x0309F) return 2; // Hiragana (e.g., ぁ, ゟ)
    if (cp >= 0x030A0 && cp <= 0x030FF) return 2; // Katakana (e.g., ァ, ヿ)
    if (cp >= 0x03100 && cp <= 0x0312F) return 2; // Bopomofo (e.g., ㄅ, ㄯ)
    if (cp >= 0x03130 && cp <= 0x0318F) return 2; // Hangul Compatibility Jamo (e.g., ㄱ, ㅿ)
    if (cp >= 0x031F0 && cp <= 0x031FF) return 2; // Katakana Phonetic Extensions (e.g., ㇰ, ㇿ)
    if (cp >= 0x03400 && cp <= 0x04DBF) return 2; // CJK Unified Ideographs Ext. A (e.g., 㐀, 䶿)
    if (cp >= 0x04E00 && cp <= 0x09FFF) return 2; // CJK Unified Ideographs (e.g., 一, 鿿)
    if (cp >= 0x0A000 && cp <= 0x0A48F) return 2; // Yi Syllables (e.g., ꀀ,  )
    if (cp >= 0x0A490 && cp <= 0x0A4CF) return 2; // Yi Radicals (e.g., ꒐,  )
    if (cp >= 0x0AC00 && cp <= 0x0D7AF) return 2; // Hangul Syllables (e.g., 가,  )
    if (cp >= 0x0D800 && cp <= 0x0DFFF) return 0; // Surrogate pairs (invalid in UTF-8)
    if (cp >= 0x0F900 && cp <= 0x0FAFF) return 2; // CJK Compatibility Ideographs (e.g., 豈,  )
    if (cp >= 0x0FB00 && cp <= 0x0FB4F) return 1; // Alphabetic Presentation Forms (e.g., ﬀ, ﭏ)
    if (cp >= 0x0FE00 && cp <= 0x0FE0F) return 0; // Variation Selectors (e.g., ︀, ️)
    if (cp >= 0x0FE10 && cp <= 0x0FE1F) return 2; // Vertical Forms (e.g., ︐,  )
    if (cp >= 0x0FE30 && cp <= 0x0FE4F) return 2; // CJK Compatibility Forms (e.g., ︰, ﹏)
    if (cp >= 0x0FF00 && cp <= 0x0FFEF) return 2; // Halfwidth and Fullwidth Forms (e.g., ！, ｯ)
    if (cp >= 0x1D400 && cp <= 0x1D7FF) return 1; // Mathematical Alphanumeric Symbols (e.g., 𝐀, 𝟿)
    if (cp >= 0x1F000 && cp <= 0x1F02F) return 2; // Mahjong Tiles (e.g., 🀀,  )
    if (cp >= 0x1F030 && cp <= 0x1F09F) return 2; // Domino Tiles (e.g., 🀰,  )
    if (cp >= 0x1F0A0 && cp <= 0x1F0FF) return 2; // Playing Cards (e.g., 🂠,  )
    if (cp >= 0x1F100 && cp <= 0x1F1FF) return 2; // Enclosed Alphanumeric Supplement (e.g., 🄀, 🇿)
    if (cp >= 0x1F300 && cp <= 0x1F5FF) return 2; // Miscellaneous Symbols and Pictographs (e.g., 🌀, 🗿)
    if (cp >= 0x1F600 && cp <= 0x1F64F) return 2; // Emoticons (e.g., 😀, 🙏)
    if (cp >= 0x1F650 && cp <= 0x1F67F) return 2; // Ascending Triangle (e.g., ▲)
    if (cp >= 0x1F680 && cp <= 0x1F6FF) return 2; // Transport and Map Symbols (e.g., 🚀, 🗺)
    if (cp >= 0x1F700 && cp <= 0x1F77F) return 2; // Alchemical Symbols (e.g., 🜀,  )
    if (cp >= 0x1F900 && cp <= 0x1F9FF) return 2; // Supplemental Symbols and Pictographs (e.g., 🤀, 🧿)

    // Default: assume single-width for unhandled ranges
    return 1;
}

size_t utf8_display_length(const char *data, size_t len) {
    size_t disp_len = 0;
    for (size_t i = 0; i < len;) {
        size_t bytes;
        uint32_t cp = utf8_to_codepoint(data, i, len, &bytes);
        disp_len += utf8_char_width(cp);
        i += bytes;
    }
    return disp_len;
}

size_t byte_to_display(const char *data, size_t byte_pos, size_t len) {
    size_t disp_pos = 0;
    for (size_t i = 0; i < byte_pos && i < len;) {
        size_t bytes;
        uint32_t cp = utf8_to_codepoint(data, i, len, &bytes);
        disp_pos += utf8_char_width(cp);
        i += bytes;
    }
    return disp_pos;
}

size_t display_to_byte(const char *data, size_t disp_pos, size_t len) {
    size_t byte_pos = 0, curr_disp = 0;
    for (; byte_pos < len && curr_disp < disp_pos;) {
        size_t bytes;
        uint32_t cp = utf8_to_codepoint(data, byte_pos, len, &bytes);
        curr_disp += utf8_char_width(cp);
        if (curr_disp <= disp_pos) byte_pos += bytes;
    }
    return byte_pos;
}

uint32_t get_utf8_char_at(const char *data, size_t byte_pos, size_t len, size_t *bytes, int *width) {
    if (byte_pos >= len) {
        *bytes = 0;
        *width = 1;  // Space character for end of line
        return ' ';
    }

    size_t remaining = len - byte_pos;
    uint32_t cp = utf8_to_codepoint(data, byte_pos, len, bytes);
    if (cp == 0 && *bytes == 1) {  // Invalid UTF-8
        *width = 1;
        return '?';
    }

    if (cp == '\t') {
        *width = TAB_WIDTH - (byte_to_display(data, byte_pos, len) % TAB_WIDTH);
    } else {
        *width = utf8_char_width(cp);
    }
    return cp;
}

// Helper function to convert a codepoint back to UTF-8 for printing
void print_utf8_char(uint32_t cp) {
    char buf[5];
    if (cp == ' ') {
        printf(" ");
        return;
    }
    size_t len = 0;
    if (cp <= 0x7F) {
        buf[len++] = cp;
    } else if (cp <= 0x7FF) {
        buf[len++] = 0xC0 | (cp >> 6);
        buf[len++] = 0x80 | (cp & 0x3F);
    } else if (cp <= 0xFFFF) {
        buf[len++] = 0xE0 | (cp >> 12);
        buf[len++] = 0x80 | ((cp >> 6) & 0x3F);
        buf[len++] = 0x80 | (cp & 0x3F);
    } else if (cp <= 0x10FFFF) {
        buf[len++] = 0xF0 | (cp >> 18);
        buf[len++] = 0x80 | ((cp >> 12) & 0x3F);
        buf[len++] = 0x80 | ((cp >> 6) & 0x3F);
        buf[len++] = 0x80 | (cp & 0x3F);
    }
    buf[len] = '\0';
    printf("%s", buf);
}
