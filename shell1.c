#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>

#define MAX_COMMANDS 10
#define MAX_VARIABLES 100

char promptText[1024]="hello:";
char* history[20];
int history_index = 0;
void handle_if_else(char* command);

typedef struct {
    char* name;
    char* value;
} variable;

variable variables[MAX_VARIABLES];
int num_variables = 0;

void set_variable(char* name, char* value) {
    int i;
    for (i = 0; i < num_variables; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            free(variables[i].value);
            variables[i].value = strdup(value);
            return;
        }
    }
    if (num_variables < MAX_VARIABLES) {
        variables[num_variables].name = strdup(name);
        variables[num_variables].value = strdup(value);
        num_variables++;
    } else {
        printf("Error: maximum number of variables reached\n");
    }
}
char* get_variable(char* name) {
    int i;
    for (i = 0; i < num_variables; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].value;
        }
    }
    return NULL;
}
void handle_sigint(int sig) {
    if (sig == SIGINT) {
        printf("\nYou typed Control-C!\n%s: ",promptText);
        fflush(stdout); // flush stdout to make sure the message gets printed immediately
    }
}

void add_to_history(char* command) {
    // free the oldest command in history if the array is full
    if (history_index == 20) {
        free(history[0]);
        for (int i = 0; i < 20 - 1; i++) {
            history[i] = history[i+1];
        }
        history_index--;
    }
    // add the current command to history
    history[history_index] = strdup(command);
    history_index++;
}

void execute_command(char* command) {
    char *token;
    int i;
    char *outfile;
    int fd, amper, redirect, retid, status, redirect_out = 0, redirect_err = 0, append = 0;
    int fildes[2];
    char **argv[MAX_COMMANDS];
    int argc[MAX_COMMANDS];
    int num_commands;
    char newCommand[1024];

    strcpy(newCommand, command);

    for (i = 0; i < MAX_COMMANDS; i++) {
        argv[i] = malloc(sizeof(char *) * 10);
        argc[i] = 10;
    }

    /* parse command line */
    i = 0;
    num_commands = 0;
    int in_if = 0;
    token = strtok(command, " ");
    while (token != NULL) {
        if (!in_if && strcmp(token, "|") == 0) {
            argv[num_commands][i] = NULL;
            num_commands++;
            i = 0;
        } else {
            if (i >= argc[num_commands]) {
                argc[num_commands] *= 2;
                argv[num_commands] = realloc(argv[num_commands], sizeof(char *) * argc[num_commands]);
            }
            argv[num_commands][i] = token;
            i++;
            if (!strcmp(token, "if")) {
                in_if = 1;
            } else if (in_if && !strcmp(token, "fi")) {
                in_if = 0;
            }
        }
        token = strtok(NULL, " ");
    }
    argv[num_commands][i] = NULL;
    num_commands++;

    /* Is command empty */
    if (argv[0][0] == NULL)
        return;

    /* Does command line end with & */
    if (!strcmp(argv[num_commands - 1][i - 1], "&")) {
        amper = 1;
        argv[num_commands - 1][i - 1] = NULL;
    } else
        amper = 0;

    redirect_out = 0;
    redirect_err = 0;
    append = 0;

    if (i > 1 && !strcmp(argv[num_commands - 1][i - 2], ">")) {
        redirect_out = 1;
        argv[num_commands - 1][i - 2] = NULL;
        outfile = argv[num_commands - 1][i - 1];
    } else if (i > 1 && !strcmp(argv[num_commands - 1][i - 2], ">>")) {
        redirect_out = 1;
        append = 1;
        argv[num_commands - 1][i - 2] = NULL;
        outfile = argv[num_commands - 1][i - 1];
    } else if (i > 1 && !strcmp(argv[num_commands - 1][i - 2], "2>")) {
        redirect_err = 1;
        argv[num_commands - 1][i - 2] = NULL;
        outfile = argv[num_commands - 1][i - 1];
    }
    /* for commands not part of the shell command language */
    if (!strcmp(argv[num_commands - 1][0], "prompt") && !strcmp(argv[num_commands - 1][1], "=")) {
        strcpy(promptText, argv[num_commands - 1][2]);
        return;
    } else if (!strcmp(argv[num_commands - 1][0], "echo")) {
        if (!strcmp(argv[num_commands - 1][1], "$?")) printf("%d\n", status);
        else {
            int j = 1;
            while (argv[num_commands - 1][j]) {
                if (argv[num_commands - 1][j][0] == '$') {
                    char *value = get_variable(argv[num_commands - 1][j] + 1);
                    if (value != NULL) {
                        printf("%s ", value);
                    } else {
                        printf("%s ", argv[num_commands - 1][j]);
                    }
                } else {
                    if (argv[num_commands - 1][j][0] == '\"' || argv[num_commands - 1][j][0] == '\'') {
                        argv[num_commands - 1][j]++;
                    }
                    int len = strlen(argv[num_commands - 1][j]);
                    if (argv[num_commands - 1][j][len - 1] == '\"' || argv[num_commands - 1][j][len - 1] == '\'') {
                        argv[num_commands - 1][j][len - 1] = '\0';
                    }
                    printf("%s ", argv[num_commands - 1][j]);
                }
                j++;
            }
            printf("\n");
        }
        return;
    } else if (!strcmp(argv[num_commands - 1][0], "cd")) {
        if (chdir(argv[num_commands - 1][1]) != 0) {
            perror("cd error");
        }
        return;
    } else if (!strcmp(argv[num_commands - 1][0], "read")) {
        char value[1024];
        fgets(value, 1024, stdin);
        value[strlen(value) - 1] = '\0';
        set_variable(argv[num_commands - 1][1], value);
        return;
    } else if (!strcmp(argv[num_commands - 1][0], "quit")) {
        exit(0);
    } else if (!strcmp(argv[num_commands - 1][0], "if")) {
        handle_if_else(newCommand);
    }

    if (fork() == 0) {
        /* redirection of IO ? */
        if (redirect_out) {
            if (append) {
                fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0660);
            } else {
                fd = creat(outfile, 0660);
            }
            close(STDOUT_FILENO);
            dup(fd);
            close(fd);
            /* stdout is now redirected */
        } else if (redirect_err) {
            fd = creat(outfile, 0660);
            close(STDERR_FILENO);
            dup(fd);
            close(fd);
            /* stderr is now redirected */
        }
        for (i = 0; i < num_commands - 1; i++) {
            pipe(fildes);
            if (fork() == 0) {
                close(STDOUT_FILENO);
                dup(fildes[1]);
                close(fildes[1]);
                close(fildes[0]);
                execvp(argv[i][0], argv[i]);
                exit(0);
            } else {
                close(STDIN_FILENO);
                dup(fildes[0]);
                close(fildes[0]);
                close(fildes[1]);
            }
        }
        execvp(argv[i][0], argv[i]);
        exit(0);
    }
    /* parent continues over here... */
    /* waits for child to exit if required */
    if (amper == 0)
        retid = wait(&status);

    for (i = 0; i < MAX_COMMANDS; i++) {
        free(argv[i]);
    }
}

void handle_if_else(char* command) {
    // find the positions of the then and else keywords
    int then_pos = -1;
    int else_pos = -1;
    int i = 0;

    while (command[i]) {
        if(command[i]=='\n') command[i]=' ';
        if (command[i-1]=='f' && command[i]=='i') command[i-1]='\0';
        i++;
    }
    i=0;
    while (command[i]) {
        if (strncmp(command + i, "then", 4) == 0 && (i == 0 || command[i - 1] == ' ')) {
            then_pos = i;
        } else if (strncmp(command + i, "else", 4) == 0 && (i == 0 || command[i - 1] == ' ')) {
            else_pos = i;
        }
        i++;
    }

    // check that the then keyword was found
    if (then_pos == -1) {
        printf("Error: missing then keyword\n");
        return;
    }

    // extract the conditional expression and commands in the then and else blocks
    char cond_expr[1024] = "";
    char then_block[1024] = "";
    char else_block[1024] = "";
    strncpy(cond_expr, command + 3, then_pos - 3);
    cond_expr[then_pos - 3] = '\0';
    if (else_pos != -1) {
        strncpy(then_block, command + then_pos + 5, else_pos - then_pos - 5);
        then_block[else_pos - then_pos - 5] = '\0';
        strcpy(else_block, command + else_pos + 5);
    } else {
        strcpy(then_block, command + then_pos + 5);
    }

    strcat(cond_expr,"> cond_expr_out.txt");
    // execute the conditional expression
    int condition = 0;
    int status;
    pid_t pid = fork();
    if (pid == 0) {
        // child process
        execute_command(cond_expr);
        exit(0);
    } else {
        // parent process
        waitpid(pid, &status, 0);
        FILE *fp = fopen("cond_expr_out.txt", "r");
        fseek(fp, 0, SEEK_END);
        long lengthOfFile = ftell(fp);
        fclose(fp);
        remove("cond_expr_out.txt");
        condition= lengthOfFile>0;
    }

    // execute the commands in the then or else block
    if (condition) {
        // execute commands in then block
        execute_command(then_block);
    } else if (else_pos != -1) {
        // execute commands in else block
        execute_command(else_block);
    }
}

int main() {
    char command[1024];
    int n = 0;
    char c;
    int history_pos = history_index;
    int flag = 1, if_else_flag=0;

    // register the signal handler for SIGINT (Ctrl+C)
    signal(SIGINT, handle_sigint);

    printf("%s ", promptText);
    while ((c = getchar()) != EOF) {
        if (c == '\033') {
            printf("\033[1A"); // line up
            printf("\x1b[2K"); // delete line
            getchar(); // skip the [
            switch (getchar()) { // the real value
                case 'A':
                    // code for arrow up
                    if (history_pos > 0) {
                        history_pos--;
                        printf("%s %s", promptText, history[history_pos]);
                        strcpy(command, history[history_pos]);
                        flag = 0;
                        n = strlen(command);
                    }
                    break;
                case 'B':
                    // code for arrow down
                    if (history_pos < history_index - 1) {
                        history_pos++;
                        printf("%s %s", promptText, history[history_pos]);
                        strcpy(command, history[history_pos]);
                        flag = 0;
                        n = strlen(command);
                    }
                    break;
            }
        } else if (flag == 0) {
            flag = 1;
            continue;
        } else if (!if_else_flag && flag && c == '\n') {
            char* token;
            command[n] = '\0';
            if (!strcmp(command, "!!")) {
                if (history_index == 0) {
                    printf("No commands in history.\n");
                    continue;
                }
                strcpy(command, history[history_index - 1]);
            } else if (command[0] == '$') {
                token = strtok(command + 1, " = ");
                if (token != NULL) {
                    char *name = token;
                    token = strtok(NULL, " = ");
                    if (token != NULL) {
                        char *value = token;
                        set_variable(name, value);
                        continue;
                    }
                }
            } else {
                // otherwise, add the current command to history and return it
                add_to_history(command);
            }
            execute_command(command);
            n = 0;
            history_pos = history_index;
            printf("%s ", promptText);
        } else {
            command[n++] = c;
            if(command[0] && command[1] && command[0]=='i' && command[1]=='f') if_else_flag=1;
            if(command[n-1] && command[n-2] && command[n-2]=='f' && command[n-1]=='i') if_else_flag=0;
        }
    }
}

