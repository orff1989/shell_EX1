#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 80

char promptText[1024]="hello";
char *history[20];
int history_count = 0;

void add_to_history(char *command) {
    if (history_count == 20) {
        free(history[0]);
        for (int i = 1; i < 20; i++) {
            history[i - 1] = history[i];
        }
        history_count--;
    }
    history[history_count++] = strdup(command);
}

void print_history() {
    for (int i = history_count - 1; i >= 0; i--) {
        printf("%d %s\n", i + 1, history[i]);
    }
}

void execute_command(char **args, int background) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else {
        if (!background) {
            wait(NULL);
        }
    }
}

void execute_pipeline(char ***commands, int num_commands) {
    int (*pipefd)[2] = malloc((num_commands - 1) * sizeof(int[2]));
    for (int i = 0; i < num_commands - 1; i++) {
        pipe(pipefd[i]);
    }

    for (int i = 0; i < num_commands; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            if (i != 0) {
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }
            if (i != num_commands - 1) {
                dup2(pipefd[i][1], STDOUT_FILENO);
            }
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }
            if (execvp(commands[i][0], commands[i]) == -1) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
    }

    for (int i = 0; i < num_commands - 1; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    while (wait(NULL) > 0);

    free(pipefd);
}

void parse_command(char *command, char ***commands, int *num_commands) {
    char *token = strtok(command, "|");
    while (token != NULL) {
        commands[*num_commands] = malloc(MAX_LINE * sizeof(char *));
        int j = 0;
        int in_quotes = 0;
        char *arg_start = token;
        for (char *p = token; ; p++) {
            if (*p == '"') {
                in_quotes = !in_quotes;
            } else if ((*p == ' ' && !in_quotes) || *p == '\0') {
                int arg_len = p - arg_start;
                if (arg_len > 0) {
                    char *arg = malloc(arg_len + 1);
                    strncpy(arg, arg_start, arg_len);
                    arg[arg_len] = '\0';
                    commands[*num_commands][j++] = arg;
                }
                arg_start = p + 1;
            }
            if (*p == '\0') {
                break;
            }
        }
        commands[*num_commands][j] = NULL;
        token = strtok(NULL, "|");
        (*num_commands)++;
    }
}

void redirect_io(char **args, int *background) {
    int i;
    for (i = 0; args[i] != NULL; i++) {
        if (!strcmp(args[i], "&")) {
            *background = 1;
            args[i] = NULL;
        } else if (!strcmp(args[i], ">")) {
            int fd = creat(args[i + 1], 0644);
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
        } else if (!strcmp(args[i], "<")) {
            int fd = open(args[i + 1], O_RDONLY);
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
        }
    }
}

void handle_sigint(int sig) {
    if (sig == SIGINT) {
        printf("\nYou typed Control-C!\n%s: ",promptText);
        fflush(stdout); // flush stdout to make sure the message gets printed immediately
    }
}

int main() {
    char command[1024];
    char ***commands = malloc(1024 * sizeof(char **));
    int num_commands;
    int should_run = 1;

    signal(SIGINT, handle_sigint);

    while (should_run) {
        printf("%s: ", promptText);
        fflush(stdout);

        fgets(command, MAX_LINE, stdin);
        command[strcspn(command, "\n")] = '\0';

        if (!strcmp(command, "!!")) {
            if (history_count == 0) {
                printf("No commands in history.\n");
                continue;
            } else {
                strcpy(command, history[history_count - 1]);
            }
        } else if (!strcmp(command, "history")) {
            print_history();
            continue;
        } else if (!strcmp(command, "quit")) {
            should_run = 0;
            continue;
        }

        add_to_history(command);

        num_commands = 0;
        parse_command(command, commands, &num_commands);

        int background = 0;
        redirect_io(commands[0], &background);

        if (!strcmp(commands[0][0], "cd")) {
            chdir(commands[0][1]);
            continue;
        } else if (!strcmp(commands[0][0], "prompt") && !strcmp(commands[0][1], "=")){
            strcpy(promptText, commands[0][2]);
            continue;
        }
        else if (!strcmp(commands[0][0], "prompt") && !strcmp(commands[0][1], "=")){
            strcpy(promptText, commands[0][2]);
            continue;
        }
        else if (num_commands == 1) {
            execute_command(commands[0], background);
        } else {
            execute_pipeline(commands, num_commands);
        }
    }

    for (int i = 0; i < history_count; i++) {
        free(history[i]);
    }
    free(commands);

    return 0;
}
