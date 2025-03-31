// files.c

#include "sc.h"

int compare_files(const void *a, const void *b) {
    File *fa = (File *)a;
    File *fb = (File *)b;
    if (fa->is_dir != fb->is_dir) {
        return fb->is_dir - fa->is_dir; // Директорії першими
    }
    if (active_panel->sort_type == 1) { // За розміром
        return fb->size - fa->size;
    } else if (active_panel->sort_type == 2) { // За датою
        return fb->mtime - fa->mtime;
    }
    return strcmp(fa->name, fb->name); // За ім'ям
}

void load_files(Panel *panel) {
    DIR *dir = opendir(panel->path);
    if (!dir) {
        panel->file_count = 0;
        return;
    }

    struct dirent *entry;
    panel->file_count = 0;

    // Додаємо ".." для всіх директорій, крім кореня
    if (strcmp(panel->path, "/") != 0) {
        strcpy(panel->files[panel->file_count].name, "..");
        panel->files[panel->file_count].is_dir = 1;
        panel->files[panel->file_count].size = 0;
        panel->files[panel->file_count].mtime = 0;
        panel->files[panel->file_count].mode = 0;
        panel->file_count++;
    }

    while ((entry = readdir(dir)) && panel->file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[1024*8];
        snprintf(full_path, sizeof(full_path), "%s/%s", panel->path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1) {
            continue;
        }

        strcpy(panel->files[panel->file_count].name, entry->d_name);
        panel->files[panel->file_count].size = st.st_size;
        panel->files[panel->file_count].mtime = st.st_mtime;
        panel->files[panel->file_count].is_dir = S_ISDIR(st.st_mode);
        panel->files[panel->file_count].mode = st.st_mode; // Зберігаємо права доступу
        panel->file_count++;
    }

    closedir(dir);

    // Сортуємо файли
    qsort(panel->files, panel->file_count, sizeof(File), compare_files);

    // Перевіряємо, чи курсор не виходить за межі
    if (panel->cursor >= panel->file_count) {
        panel->cursor = panel->file_count - 1;
    }
    if (panel->cursor < 0) {
        panel->cursor = 0;
    }
}
