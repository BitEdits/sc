// input.c
#include "sc.h"

#include <sys/wait.h>

int get_input() {
    int c = getchar();
    if (c == 27) { // Esc або стрілки
        int c2 = getchar();
        if (c2 == '[') {
            int c3 = getchar();
            if (c3 == 'A') return KEY_UP;    // Up
            if (c3 == 'B') return KEY_DOWN;  // Down
            if (c3 == 'C') return KEY_RIGHT; // Right
            if (c3 == 'D') return KEY_LEFT;  // Left
            if (c3 == 'H') return KEY_HOME;  // Home
            if (c3 == 'F') return KEY_END;   // End
            if (c3 == '5') { // PgUp
                getchar(); // ~
                return KEY_PGUP;
            }
            if (c3 == '6') { // PgDown
                getchar(); // ~
                return KEY_PGDOWN;
            }
            // Обробка F1-F10
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
                } else if (c3 == '2') {
                    if (c4 == '0') { getchar(); return KEY_F9; }  // F9: \033[20~
                    if (c4 == '1') { getchar(); return KEY_F10; } // F10: \033[21~
                }
            }
        } else if (c2 == 27) {
            return 27; // Esc
        } else {
            ungetc(c2, stdin); // Повернути символ назад
            return 27;
        }
    }
    return c;
}

void execute_command_fast(const char *cmd) {
    pid_t pid;
    int pipefd[2];
    char buffer[16384];
    int output_len = 0;

    // Створюємо пайп для захоплення виводу
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid == 0) { // Дочірній процес
        close(pipefd[0]); // Закриваємо кінець для читання

        // Перенаправляємо stdout і stderr у пайп
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // Виконуємо команду
        execlp("sh", "sh", "-c", cmd, NULL);
        perror("execlp");
        exit(1);
    } else { // Батьківський процес
        close(pipefd[1]); // Закриваємо кінець для запису

        // Вимикаємо сирий режим, щоб вивід команди відображався коректно
        disable_raw_mode();

        // Відображаємо команду
        printf("> %s\n", cmd);

        // Читаємо вивід із пайпа і виводимо його в реальному часі
        ssize_t count;
        while ((count = read(pipefd[0], buffer + output_len, sizeof(buffer) - output_len - 1)) > 0) {
            buffer[output_len + count] = 0;
            // Виводимо вивід у термінал
            printf("%s", buffer + output_len);
            output_len += count;
        }

        close(pipefd[0]);

        // Чекаємо завершення команди
        waitpid(pid, NULL, 0);

        // Додаємо команду та її вивід до історії
        if (history_count < MAX_HISTORY) {
            history_count++;
        } else {
            history_start = (history_start + 1) % MAX_HISTORY;
        }
        int idx = (history_start + history_count - 1) % MAX_HISTORY;
        strncpy(history[idx].command, cmd, sizeof(history[idx].command) - 1);
        history[idx].command[sizeof(history[idx].command) - 1] = 0;
        strncpy(history[idx].output, buffer, sizeof(history[idx].output) - 1);
        history[idx].output[sizeof(history[idx].output) - 1] = 0;

        append_to_history_display(cmd, buffer);

        history_scroll_pos = history_count;
        history_display_offset = 0; // Скидаємо зміщення відображення

    }
}

void execute_command(const char *command) {
    struct timespec start, end;
    double elapsed;

    // Start timing the entire function
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Create a pipe to capture the command output
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        char output[16384];
        snprintf(output, sizeof(output), "Error: Failed to create pipe");
        strncpy(history[history_start].command, command, sizeof(history[history_start].command) - 1);
        history[history_start].command[sizeof(history[history_start].command) - 1] = '\0';
        strncpy(history[history_start].output, output, sizeof(history[history_start].output) - 1);
        history[history_start].output[sizeof(history[history_start].output) - 1] = '\0';
        history_start = (history_start + 1) % MAX_HISTORY;
        if (history_count < MAX_HISTORY) history_count++;
        append_to_history_display(command, output);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        // Fork failed
        close(pipefd[0]);
        close(pipefd[1]);
        char output[16384];
        snprintf(output, sizeof(output), "Error: Failed to fork");
        strncpy(history[history_start].command, command, sizeof(history[history_start].command) - 1);
        history[history_start].command[sizeof(history[history_start].command) - 1] = '\0';
        strncpy(history[history_start].output, output, sizeof(history[history_start].output) - 1);
        history[history_start].output[sizeof(history[history_start].output) - 1] = '\0';
        history_start = (history_start + 1) % MAX_HISTORY;
        if (history_count < MAX_HISTORY) history_count++;
        append_to_history_display(command, output);
        return;
    }

    if (pid == 0) { // Child process
        // Close the read end of the pipe
        close(pipefd[0]);

        // Redirect stdout to the write end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        // Redirect stderr to stdout so we capture both
        dup2(STDOUT_FILENO, STDERR_FILENO);

        // Execute the command using /bin/sh to handle arguments and shell features
        execlp("/bin/sh", "sh", "-c", command, (char *)NULL);

        // If exec fails, print an error and exit
        char output[16384];
        snprintf(output, sizeof(output), "Error: Failed to execute command");
        write(STDOUT_FILENO, output, strlen(output));
        _exit(127);
    }

    // Parent process
    // Close the write end of the pipe
    close(pipefd[1]);

    // Read the output from the child process
    char output[16384*4] = {0};
    size_t total_read = 0;
    char buffer[1024*3];

    ssize_t count;
    while ((count = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[count] = '\0';
        if (total_read + count < sizeof(output)) {
            strcat(output, buffer);
            total_read += count;
        }
    }
    close(pipefd[0]);

    // Wait for the child process to finish (non-blocking)
    int status;

    waitpid(pid, NULL, 0);

    // Add the command and its output to history
    char *command_copy = strdup(command);
    char *output_copy = strdup(output);
    if (command_copy && output_copy) {
        strncpy(history[history_start].command, command_copy, sizeof(history[history_start].command) - 1);
        history[history_start].command[sizeof(history[history_start].command) - 1] = '\0';
        strncpy(history[history_start].output, output_copy, sizeof(history[history_start].output) - 1);
        history[history_start].output[sizeof(history[history_start].output) - 1] = '\0';
    } else {
        // Handle memory allocation failure
        strncpy(history[history_start].command, command, sizeof(history[history_start].command) - 1);
        history[history_start].command[sizeof(history[history_start].command) - 1] = '\0';
        strncpy(history[history_start].output, "Error: Memory allocation failed", sizeof(history[history_start].output) - 1);
        history[history_start].output[sizeof(history[history_start].output) - 1] = '\0';
    }

    free(command_copy);
    free(output_copy);

    history_start = (history_start + 1) % MAX_HISTORY;
    if (history_count < MAX_HISTORY) history_count++;

    append_to_history_display(command, output);

}

void execute_command_popen(const char *cmd) {
    // Додаємо перенаправлення stderr у stdout
    char cmd_with_redirect[2048];
    snprintf(cmd_with_redirect, sizeof(cmd_with_redirect), "%s 2>&1", cmd);

    FILE *pipe = popen(cmd_with_redirect, "r");
    if (!pipe) {
        snprintf(history[history_start].output, sizeof(history[history_start].output), "Failed to execute command\n");
        return;
    }

    // Читаємо весь вивід після завершення команди
    char buffer[16384] = {0};
    size_t pos = 0;
    int ch;
    while ((ch = fgetc(pipe)) != EOF && pos < sizeof(buffer) - 1) {
        buffer[pos++] = ch;
    }
    buffer[pos] = 0;

    pclose(pipe);

    // Скидаємо стан терміналу після виконання команди
    disable_raw_mode();
    enable_raw_mode();

    // Зберігаємо команду та її вивід у кільцевому буфері
    strncpy(history[history_start].command, cmd, sizeof(history[history_start].command) - 1);
    history[history_start].command[sizeof(history[history_start].command) - 1] = 0;
    strncpy(history[history_start].output, buffer, sizeof(history[history_start].output) - 1);
    history[history_start].output[sizeof(history[history_start].output) - 1] = 0;

    // Оновлюємо індекси для кільцевого буфера
    history_start = (history_start + 1) % MAX_HISTORY;
    if (history_count < MAX_HISTORY) {
        history_count++;
    }

    // Скидаємо позицію прокручування історії
    history_scroll_pos = history_count;
    history_display_offset = 0; // Скидаємо зміщення відображення
}

