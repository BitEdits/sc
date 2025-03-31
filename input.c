// input.c

#include "sc.h"

void execute_command(const char *cmd) {
    pid_t pid;
    int pipefd[2];
    char output[16384 * 4] = {0};
    size_t total_read = 0;
    char buffer[1024 * 3];
    int status;

    // Create pipe to capture output
    if (pipe(pipefd) == -1) {
        snprintf(output, sizeof(output), "Error: Failed to create pipe");
        goto log_and_exit;
    }

    // Block SIGCHLD to prevent race conditions (from MC's my_system)
    sigset_t sigchld_mask, old_mask;
    sigemptyset(&sigchld_mask);
    sigaddset(&sigchld_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &sigchld_mask, &old_mask);

    pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        snprintf(output, sizeof(output), "Error: Failed to fork");
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        goto log_and_exit;
    }

    if (pid == 0) { // Child process
        // Unblock SIGCHLD in child (from MC)
        sigprocmask(SIG_SETMASK, &old_mask, NULL);

        // Reset signal handlers to default (from MC)
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        // Redirect stdout and stderr to pipe
        close(pipefd[0]); // Close read end
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // Restore terminal to cooked mode for child
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

        // Execute command via shell (from MC's EXECUTE_AS_SHELL)
        execlp("/bin/sh", "sh", "-c", cmd, (char *)NULL);

        // If exec fails
        snprintf(buffer, sizeof(buffer), "Error: Failed to execute command");
        write(STDOUT_FILENO, buffer, strlen(buffer));
        _exit(127);
    }

    // Parent process
    close(pipefd[1]); // Close write end

    // Suspend our raw mode to let child take over terminal
    disable_raw_mode();
    printf("\x1b[2J\x1b[H"); // Clear screen

    // Read output in real-time and display it
    ssize_t count;
    while ((count = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[count] = '\0';
        write(STDOUT_FILENO, buffer, count); // Display immediately
        if (total_read + count < sizeof(output)) {
            strncat(output, buffer, count);
            total_read += count;
        }
    }
    close(pipefd[0]);

    // Wait for child to finish (from MC)
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            snprintf(output, sizeof(output), "Error: Waitpid failed");
            break;
        }
    }

    // Restore signal mask (from MC)
    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    // Restore our terminal mode and UI
    enable_raw_mode();
    draw_interface();

log_and_exit:
    // Log to history (adapted from your original)
    char *command_copy = strdup(cmd);
    char *output_copy = strdup(output);
    if (command_copy && output_copy) {
        strncpy(history[history_start].command, command_copy, sizeof(history[history_start].command) - 1);
        history[history_start].command[sizeof(history[history_start].command) - 1] = '\0';
        strncpy(history[history_start].output, output_copy, sizeof(history[history_start].output) - 1);
        history[history_start].output[sizeof(history[history_start].output) - 1] = '\0';
    } else {
        strncpy(history[history_start].command, cmd, sizeof(history[history_start].command) - 1);
        history[history_start].command[sizeof(history[history_start].command) - 1] = '\0';
        strncpy(history[history_start].output, "Error: Memory allocation failed", sizeof(history[history_start].output) - 1);
        history[history_start].output[sizeof(history[history_start].output) - 1] = '\0';
    }
    free(command_copy);
    free(output_copy);

    history_start = (history_start + 1) % MAX_HISTORY;
    if (history_count < MAX_HISTORY) history_count++;
    append_to_history_display(cmd, output);
}

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
        } else if (c2 == 'O') {
            int c3 = getchar();
            if (c3 == 'P') return KEY_F1;   // F1: \033OP
            if (c3 == 'Q') return KEY_F2;   // F2: \033OQ
            if (c3 == 'R') return KEY_F3;   // F3: \033OR
            if (c3 == 'S') return KEY_F4;   // F4: \033OS
        } else if (c2 == 27) {
            return 27; // Esc
        } else {
            ungetc(c2, stdin); // Повернути символ назад
            return 27;
        }
    }
    return c;
}


