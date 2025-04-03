// sh.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 64
#define MAX_LINE 1024

// Structure to hold command and arguments
struct Command {
    char *argv[MAX_ARGS];
    int argc;
};

// Simple key-value store for variables
struct Variable {
    char *name;
    char *value;
};

struct Variable vars[64];
int var_count = 0;

// Function prototypes
void parse_command(char *line, struct Command *cmd);
void execute_command(struct Command *cmd);
char *get_var(char *name);
void set_var(char *name, char *value);

// Main shell loop
int main() {
    char line[MAX_LINE];
    struct Command cmd;

    while (1) {
        printf("$ ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break; // EOF
        }

        line[strcspn(line, "\n")] = 0; // Remove newline
        if (strlen(line) == 0) continue;

        parse_command(line, &cmd);
        execute_command(&cmd);
    }
    return 0;
}

void parse_command(char *line, struct Command *cmd) {
    cmd->argc = 0;
    char *token = strtok(line, " ");
    while (token && cmd->argc < MAX_ARGS - 1) {
        cmd->argv[cmd->argc++] = token;
        token = strtok(NULL, " ");
    }
    cmd->argv[cmd->argc] = NULL;
}

void execute_command(struct Command *cmd) {
    if (cmd->argc == 0) return;

    // Built-in commands
    if (strcmp(cmd->argv[0], "cd") == 0) {
        chdir(cmd->argc > 1 ? cmd->argv[1] : getenv("HOME"));
    }
    else if (strcmp(cmd->argv[0], "echo") == 0) {
        for (int i = 1; i < cmd->argc; i++) {
            printf("%s%s", cmd->argv[i], i < cmd->argc - 1 ? " " : "");
        }
        printf("\n");
    }
    else if (strcmp(cmd->argv[0], "exit") == 0) {
        exit(0);
    }
    else if (strcmp(cmd->argv[0], "pwd") == 0) {
        char cwd[MAX_LINE];
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
    }
    else if (strcmp(cmd->argv[0], "read") == 0) {
        char input[MAX_LINE];
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0;
        if (cmd->argc > 1) {
            set_var(cmd->argv[1], input);
        }
    }
    else if (strcmp(cmd->argv[0], "set") == 0) {
        if (cmd->argc > 1) {
            char *eq = strchr(cmd->argv[1], '=');
            if (eq) {
                *eq = '\0';
                set_var(cmd->argv[1], eq + 1);
            }
        }
    }
    else if (strcmp(cmd->argv[0], "unset") == 0) {
        if (cmd->argc > 1) {
            for (int i = 0; i < var_count; i++) {
                if (strcmp(vars[i].name, cmd->argv[1]) == 0) {
                    free(vars[i].name);
                    free(vars[i].value);
                    vars[i] = vars[--var_count];
                    break;
                }
            }
        }
    }
    else if (strcmp(cmd->argv[0], "export") == 0) {
        if (cmd->argc > 1) {
            char *eq = strchr(cmd->argv[1], '=');
            if (eq) {
                *eq = '\0';
                setenv(cmd->argv[1], eq + 1, 1);
            }
        }
    }
    // Simple external command execution
    else {
        pid_t pid = fork();
        if (pid == 0) {
            execvp(cmd->argv[0], cmd->argv);
            printf("Command not found: %s\n", cmd->argv[0]);
            exit(1);
        }
        else if (pid > 0) {
            wait(NULL);
        }
    }
}

char *get_var(char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            return vars[i].value;
        }
    }
    return getenv(name);
}

void set_var(char *name, char *value) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            free(vars[i].value);
            vars[i].value = strdup(value);
            return;
        }
    }
    if (var_count < 64) {
        vars[var_count].name = strdup(name);
        vars[var_count].value = strdup(value);
        var_count++;
    }
}
