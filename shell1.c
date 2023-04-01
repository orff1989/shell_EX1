#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>

#define MAX_COMMANDS 10
char promptText[1024]="hello:";

void handle_sigint(int sig) {
    if (sig == SIGINT) {
        printf("\nYou typed Control-C!\n%s: ",promptText);
        fflush(stdout); // flush stdout to make sure the message gets printed immediately
    }
}

int main() {
    char command[1024];
    char *token;
    int i;
    char *outfile;
    int fd, amper, redirect, retid, status, redirect_out=0 , redirect_err=0, append=0 ;
    int fildes[2];
    char **argv[MAX_COMMANDS];
    int argc[MAX_COMMANDS];
    int num_commands;

    // register the signal handler for SIGINT (Ctrl+C)
    signal(SIGINT, handle_sigint);

    for (i=0; i<MAX_COMMANDS; i++) {
        argv[i] = malloc(sizeof(char*) * 10);
        argc[i] = 10;
    }

    while (1)
    {
        printf("%s ", promptText);
        fgets(command, 1024, stdin);
        command[strlen(command) - 1] = '\0';

        /* parse command line */
        i = 0;
        num_commands = 0;
        token = strtok (command," ");
        while (token != NULL)
        {
            if (strcmp(token, "|") == 0) {
                argv[num_commands][i] = NULL;
                num_commands++;
                i = 0;
            } else {
                if (i >= argc[num_commands]) {
                    argc[num_commands] *= 2;
                    argv[num_commands] = realloc(argv[num_commands], sizeof(char*) * argc[num_commands]);
                }
                argv[num_commands][i] = token;
                i++;
            }
            token = strtok (NULL, " ");
        }
        argv[num_commands][i] = NULL;
        num_commands++;

        /* Is command empty */
        if (argv[0][0] == NULL)
            continue;

        /* Does command line end with & */
        if (! strcmp(argv[num_commands-1][i - 1], "&")) {
            amper = 1;
            argv[num_commands-1][i - 1] = NULL;
        }
        else
            amper = 0;

        redirect_out = 0;
        redirect_err = 0;
        append = 0;

        if (i > 1 && ! strcmp(argv[num_commands-1][i - 2], ">")) {
            redirect_out = 1;
            argv[num_commands-1][i - 2] = NULL;
            outfile = argv[num_commands-1][i - 1];
        }else if (i > 1 && ! strcmp(argv[num_commands-1][i - 2], ">>")) {
            redirect_out = 1;
            append = 1;
            argv[num_commands-1][i - 2] = NULL;
            outfile = argv[num_commands-1][i - 1];
        } else if (i > 1 && ! strcmp(argv[num_commands-1][i - 2], "2>")) {
            redirect_err = 1;
            argv[num_commands-1][i - 2] = NULL;
            outfile = argv[num_commands-1][i - 1];
        }



        /* for commands not part of the shell command language */
        if (! strcmp(argv[num_commands-1][0], "prompt") && ! strcmp(argv[num_commands-1][1], "=")) {
            strcpy(promptText, argv[num_commands-1][2]);
            continue;
        }
        else if (! strcmp(argv[num_commands-1][0], "echo")) {
            if(! strcmp(argv[num_commands-1][1], "$?")) printf("%d\n",status);
            else {
                int j = 1;
                while (argv[num_commands-1][j]) {
                    printf("%s ", argv[num_commands-1][j]);
                    j++;
                }
                printf("\n");
            }
            continue;
        }
        else if (!strcmp(argv[num_commands-1][0], "cd")) {
            if (chdir(argv[num_commands-1][1]) != 0) {
                perror("cd error");
            }
            continue;
        }
        else if (!strcmp(argv[num_commands-1][0], "quit")) {
            break;
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

            for (i=0; i<num_commands-1; i++) {
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
    }

    for (i=0; i<MAX_COMMANDS; i++) {
        free(argv[i]);
    }
}