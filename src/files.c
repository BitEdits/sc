// files.c

#include "socha.h"

int compare_by_name(const void *a, const void *b) {
    const File *file_a = (const File *)a;
    const File *file_b = (const File *)b;

    // Put ".." at the top
    if (strcmp(file_a->name, "..") == 0) return -1;
    if (strcmp(file_b->name, "..") == 0) return 1;

    // Directories first
    if (file_a->is_dir && !file_b->is_dir) return -1;
    if (!file_a->is_dir && file_b->is_dir) return 1;

    // Then sort by name
    return strcmp(file_a->name, file_b->name);
}

void load_files(Panel *panel) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;

    // Clear existing files
    panel->file_count = 0;

#ifdef _WIN32
    // On Windows, replace "/" with a valid drive path (e.g., "C:/")
    if (strcmp(panel->path, "/") == 0) {
        strcpy(panel->path, "C:/"); // Use C:/ as the default root
    }
    // Convert forward slashes to backslashes for Windows
    char win_path[1024];
    strcpy(win_path, panel->path);
    for (char *p = win_path; *p; p++) {
        if (*p == '/') *p = '\\';
    }
    dir = opendir(win_path);
#else
    dir = opendir(panel->path);
#endif

    if (!dir) {
        // Log error (for debugging)
        fprintf(stderr, "Failed to open directory: %s\n", panel->path);
        return;
    }

    // Read directory entries, but limit to MAX_FILES
    while ((entry = readdir(dir)) != NULL && panel->file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0) continue; // Skip current directory

        File *file = &panel->files[panel->file_count];
        strncpy(file->name, entry->d_name, sizeof(file->name) - 1);
        file->name[sizeof(file->name) - 1] = '\0';

        // Get file stats
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", panel->path, entry->d_name);
#ifdef _WIN32
        // Convert path for stat on Windows
        for (char *p = full_path; *p; p++) {
            if (*p == '/') *p = '\\';
        }
#endif
        if (stat(full_path, &st) == 0) {
            file->is_dir = S_ISDIR(st.st_mode);
            file->size = st.st_size;
            file->mtime = st.st_mtime;
            file->mode = st.st_mode;
        } else {
            file->is_dir = 0;
            file->size = 0;
            file->mtime = 0;
            file->mode = 0;
        }

        panel->file_count++;
    }

    closedir(dir);

    // Add ".." entry for parent directory (if not root and space is available)
    if (strcmp(panel->path, "C:/") != 0 && strcmp(panel->path, "/") != 0 && panel->file_count < MAX_FILES) {
        File *parent = &panel->files[panel->file_count];
        strcpy(parent->name, "..");
        parent->is_dir = 1;
        parent->size = 0;
        parent->mtime = 0;
        parent->mode = 0;
        panel->file_count++;
    }

    // Sort files (assuming sort_type 0 means by name)
    if (panel->sort_type == 0) {
        qsort(panel->files, panel->file_count, sizeof(File), compare_by_name);
    }
}
