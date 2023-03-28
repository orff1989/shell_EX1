#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

char promptText[1024]="hello";
void getStrFrom(char command[1024], char *pString[10]);
void handle_sigint(int sig);

int main() {
char command[1024];
char lastCommand[1024]="";
char *token;
char *outfile;
int i, j, fd, amper, redirect, retid, status, redirectError, redirectAppend, lastCommandFlag=0;
char *argv[10];

// register the signal handler for SIGINT (Ctrl+C)
signal(SIGINT, handle_sigint);
argv[0]=NULL;
while (1) {

    if (lastCommandFlag == 0){
        printf("%s: ", promptText);
        if(argv[0] && strlen(argv[0])>0) getStrFrom(lastCommand, argv);
        fgets(command, 1024, stdin);
        command[strlen(command) - 1] = '\0';
    }
    lastCommandFlag=0;

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
    if (argv[i - 1] && ! strcmp(argv[i - 1], "&")) {
        amper = 1;
        argv[i - 1] = NULL;
    }
    else
        amper = 0;

    if (argv[i - 2] && (! strcmp(argv[i - 2], ">") || ! strcmp(argv[i - 2], "2>") || ! strcmp(argv[i - 2], ">>"))) {
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
    if (! strcmp(argv[0], "prompt") && ! strcmp(argv[1], "=")) {
        strcpy(promptText, argv[2]);
        continue;
    }
    else if (! strcmp(argv[0], "echo")) {
        if(! strcmp(argv[1], "$?")) printf("%d\n",status);
        else {
            j = 1;
            while (argv[j]) {
                printf("%s ", argv[j]);
                j++;
            }
            printf("\n");
        }
        continue;
    }
    else if (!strcmp(argv[0], "cd")) {
        if (chdir(argv[1]) != 0) {
            perror("cd error");
        }
        continue;
    }
    else if (!strcmp(argv[0], "!!")) {
        if(strlen(lastCommand)>0) {
            lastCommandFlag = 1;
            strcpy(command, lastCommand);
        }
        else{
            printf("Error: There is no last command\n");
        }
        continue;
    }
    else if (!strcmp(argv[0], "quit")) {
        break;
    }

    if (fork() == 0) {
        /* redirection of IO ? */
        if (redirect) {
            if (redirectAppend) fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0660);
            else fd = creat(outfile, 0660);

            if (redirectError) dup2(fd, STDERR_FILENO);
            else dup2(fd, STDOUT_FILENO);

            close(fd);
        }
        if (execvp(argv[0], argv) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }
    /* parent continues here */
    if (amper == 0)
        retid = wait(&status);
}
}

void getStrFrom(char command[1024], char *pString[10]) {
    int i = 0;
    command[0] = '\0'; // initialize the command string as empty
    while (pString[i] != NULL && i < 10) {
        strcat(command, pString[i]); // append the current string to the command
        if (pString[i+1] != NULL && i+1 < 10) {
            strcat(command, " "); // add a space if there are more strings to come
        }
        i++;
    }
}

void handle_sigint(int sig) {
    if (sig == SIGINT) {
        printf("\nYou typed Control-C!\n%s: ",promptText);
        fflush(stdout); // flush stdout to make sure the message gets printed immediately
    }
}
