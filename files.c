// file_utils.c
#include "sc.h"

int compare_files(const void *a, const void *b) {
    File *fa = (File *)a;
    File *fb = (File *)b;
    if (active_panel->sort_type == 0) { // За ім'ям
        return strcmp(fa->name, fb->name);
    } else if (active_panel->sort_type == 1) { // За розміром
        return fb->size - fa->size;
    } else { // За датою
        return fb->mtime - fa->mtime;
    }
}

void load_files(Panel *panel) {
    DIR *dir = opendir(panel->path);
    if (!dir) return;

    panel->file_count = 0;
    struct dirent *entry;
    struct stat st;
    char full_path[1024];

    while ((entry = readdir(dir)) && panel->file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0) continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", panel->path, entry->d_name);
        if (stat(full_path, &st) == 0) {
            strncpy(panel->files[panel->file_count].name, entry->d_name, sizeof(panel->files[panel->file_count].name));
            panel->files[panel->file_count].size = st.st_size;
            panel->files[panel->file_count].mtime = st.st_mtime;
            panel->files[panel->file_count].is_dir = S_ISDIR(st.st_mode);
            panel->file_count++;
        }
    }
    closedir(dir);

    // Сортування
    qsort(panel->files, panel->file_count, sizeof(File), compare_files);

    // Перевіряємо, чи ми повернулися до батьківської директорії
    if (panel->dir_history_count > 0) {
        DirHistory *last = &panel->dir_history[panel->dir_history_count - 1];
        char current_path[1024];
        strcpy(current_path, panel->path);
        // Прибираємо завершальний слеш, якщо є
        if (current_path[strlen(current_path) - 1] == '/') {
            current_path[strlen(current_path) - 1] = 0;
        }
        if (strcmp(last->parent_path, current_path) == 0) {
            // Шукаємо директорію, з якої вийшли
            for (int i = 0; i < panel->file_count; i++) {
                if (strcmp(panel->files[i].name, last->dir_name) == 0) {
                    panel->cursor = last->cursor_pos; // Відновлюємо точну позицію курсору
                    panel->scroll_offset = panel->cursor - (rows - 4 - 2) / 2;
                    if (panel->scroll_offset < 0) panel->scroll_offset = 0;
                    break;
                }
            }
            panel->dir_history_count--;
        }
    }
}
