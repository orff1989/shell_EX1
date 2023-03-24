#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>

int main() {
char command[1024];
char *token;
char *outfile;
int i, fd, amper, redirect, retid, status, redirectError, redirectAppend, fd_name;
char *argv[10];

while (1)
{
    printf("hello: ");
    fgets(command, 1024, stdin);
    command[strlen(command) - 1] = '\0';

    /* parse command line */
    i = 0;
    token = strtok (command," ");
    while (token != NULL)
    {
        argv[i] = token;
        token = strtok (NULL, " ");
        i++;
    }
    argv[i] = NULL;

    /* Is command empty */
    if (argv[0] == NULL)
        continue;

    /* Does command line end with & */ 
    if (! strcmp(argv[i - 1], "&")) {
        amper = 1;
        argv[i - 1] = NULL;
    }
    else 
        amper = 0; 

    if (! strcmp(argv[i - 2], ">") || ! strcmp(argv[i - 2], "2>") || ! strcmp(argv[i - 2], ">>")) {
        redirect = 1;
        if(! strcmp(argv[i - 2], "2>")) redirectError=1;
        else if(! strcmp(argv[i - 2], ">>")) redirectAppend=1;
        argv[i - 2] = NULL;
        outfile = argv[i - 1];
        }
    else {
        redirect = 0;
        redirectError = 0;
        redirectAppend = 0;
    }

    /* for commands not part of the shell command language */ 

    if (fork() == 0) { 
        /* redirection of IO ? */
        if (redirect) {
            if (redirectAppend) {
                fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0660);
            } else {
                fd = creat(outfile, 0660);
            }
            close(STDOUT_FILENO);
            dup(fd);
            close(STDERR_FILENO);
            dup(fd);
            close(fd);
            /* stdout and stderr are now redirected */
        }
        execvp(argv[0], argv);
    }
    /* parent continues here */
    if (amper == 0)
        retid = wait(&status);
}
}
