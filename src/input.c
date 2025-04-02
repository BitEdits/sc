// input.c

#include "sokhatsky.h"

char *command_copy; //strdup(cmd);
char *output_copy; //strdup(output);
char output[16384 * 4] = {0};
char buffer[1024 * 3];

void execute_command(const char *cmd) {

    memset(output, 0, sizeof(output));

 #ifdef _WIN32
    HANDLE hChildStdoutRead, hChildStdoutWrite;
    SECURITY_ATTRIBUTES saAttr;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD bytesRead;
    size_t total_read = 0;

    printf("\x1b[%d;1H\x1b[1;37;40m%-*s", rows, cols, "");
    printf("\x1b[%d;1H", rows);   // Move to last row (rows is from get_window_size), clear line
    printf("\x1b[K");                 // Clear line

    // Create pipe to capture output
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hChildStdoutRead, &hChildStdoutWrite, &saAttr, 0)) {
        snprintf(output, sizeof(output), "Error: Failed to create pipe");
        finalize_exec(cmd);
        return;
    }

    // Set up STARTUPINFO for process creation
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = hChildStdoutWrite;
    si.hStdError = hChildStdoutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    // Create the process
    if (!CreateProcess(NULL, (char *)cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        snprintf(output, sizeof(output), "Error: Failed to create process");
        finalize_exec(cmd);
        return;
    }

    // Close write end of pipe in parent
    CloseHandle(hChildStdoutWrite);

    // Read output in real-time and display it
    while (ReadFile(hChildStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        write(STDOUT_FILENO, buffer, bytesRead); // Display immediately
        if (total_read + bytesRead < sizeof(output)) {
            strncat(output, buffer, bytesRead);
            total_read += bytesRead;
        }
    }

    // Wait for child process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStdoutRead);

    #else

    pid_t pid;
    int pipefd[2];
    size_t total_read = 0;
    int status;
    command_copy = malloc(1024); //strdup(cmd);
    output_copy = malloc(65536); //strdup(output);

    printf("\x1b[%d;1H\x1b[1;37;40m%-*s", rows, cols, "");
    printf("\x1b[%d;1H", rows);   // Move to last row (rows is from get_window_size), clear line
    printf("\x1b[K");                 // Clear line

    // Create pipe to capture output
    if (pipe(pipefd) == -1) {
        snprintf(output, sizeof(output), "Error: Failed to create pipe");
        finalize_exec(cmd);
        return;
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
        finalize_exec(cmd);
        return;
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
    fflush(stdout);                   // Ensure sequences are applied

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

#endif

    finalize_exec(cmd);
}

void finalize_exec(const char *cmd) {
    // Use temporary variables to store command and output copies
    char *command_copy = strdup(cmd);
    char *output_copy = strdup(output);

    // Store in history at current start position
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

    // Update circular buffer state
    history_start = (history_start + 1) % MAX_HISTORY;
    if (history_count < MAX_HISTORY) {
        history_count++;
    }

    // Append to display (this will handle the latest command/output pair)
    append_to_history_display(cmd, output);
    command_buffer[0] = 0;
}

#ifdef _WIN32

int get_input() {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD ir;
    DWORD count;

    while (1) {
        ReadConsoleInput(hStdin, &ir, 1, &count);
        if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
            switch (ir.Event.KeyEvent.wVirtualKeyCode) {
                case VK_UP: return KEY_UP;
                case VK_DOWN: return KEY_DOWN;
                case VK_RIGHT: return KEY_RIGHT;
                case VK_LEFT: return KEY_LEFT;
                case VK_PRIOR: return KEY_PGUP;
                case VK_NEXT: return KEY_PGDOWN;
                case VK_HOME: return KEY_HOME;
                case VK_END: return KEY_END;
                case VK_F1: return KEY_F1;
                case VK_F2: return KEY_F2;
                case VK_F3: return KEY_F3;
                case VK_F4: return KEY_F4;
                case VK_F5: return KEY_F5;
                case VK_F6: return KEY_F6;
                case VK_F7: return KEY_F7;
                case VK_F8: return KEY_F8;
                case VK_F9: return KEY_F9;
                case VK_F10: return KEY_F10;
                case VK_INSERT: return KEY_INSERT;
                case VK_DELETE: return KEY_DELETE;
                case VK_RETURN: return KEY_ENTER;
                case VK_ESCAPE: return KEY_ESC;
                default:
                    switch (ir.Event.KeyEvent.uChar.AsciiChar) {
                        case 8: return KEY_BACKSPACE;
                        case 9: return KEY_TAB;
                        case 13: return KEY_ENTER;
                        case 15: return KEY_CTRL_O;
                        case 127: return KEY_BACKSPACE;
                        default: return ir.Event.KeyEvent.uChar.AsciiChar;
                    }
            }
        }
    }
}

#else

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
            if (c3 == '3' && getchar() == '~') return KEY_DELETE;
            if (c3 == '5') { // PgUp
                getchar(); // ~
                return KEY_PGUP;
            }
            if (c3 == '6') { // PgDown
                getchar(); // ~
                return KEY_PGDOWN;
            } else if (c3 == '1' && getchar() == ';') { // Ctrl/Shift modifiers
                int c5 = getchar();
                if (c5 == '5') { // Ctrl
                    int c6 = getchar();
                    if (c6 == 'D') return KEY_CTRL_LEFT;  // Ctrl+Left: \033[1;5D
                    if (c6 == 'C') return KEY_CTRL_RIGHT; // Ctrl+Right: \033[1;5C
                } else if (c5 == '2') { // Shift
                    int c6 = getchar();
                    if (c6 == 'D') return KEY_SHIFT_LEFT;  // Shift+Left: \033[1;2D
                    if (c6 == 'C') return KEY_SHIFT_RIGHT; // Shift+Right: \033[1;2C
                }
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
                    if (c4 == '~') { return KEY_INSERT; } // F10: \033[21~
                }
            }
        } else if (c2 == 'O') {
            int c3 = getchar();
            if (c3 == 'P') return KEY_F1;   // F1: \033OP
            if (c3 == 'Q') return KEY_F2;   // F2: \033OQ
            if (c3 == 'R') return KEY_F3;   // F3: \033OR
            if (c3 == 'S') return KEY_F4;   // F4: \033OS
        } else if (c2 >= 'A' && c2 <= 'Z') { // ESC + Shift
            int c3 = getchar();
            if (c2 == 'E' && c3 == '\n') return KEY_ESC_SHIFT_ENTER; // ESC+Shift+Enter
            if (c2 == '[') return KEY_ESC_SHIFT_LEFT_BRACKET; // ESC+Shift+[
            if (c2 == ']') return KEY_ESC_SHIFT_RIGHT_BRACKET; // ESC+Shift+]
        } else if (c2 == 27) {
            return KEY_ESC; // Esc
        } else {
            ungetc(c2, stdin); // Повернути символ назад
            return KEY_ESC;
        }
    } else if (c == '\n') {
        return KEY_ENTER;
    } else if (c == 9) {
        return KEY_TAB;
    } else if (c == 15) {
        return KEY_CTRL_O;
    } else if (c == 127) {
        return KEY_BACKSPACE;
    }
    return c;
}

#endif
