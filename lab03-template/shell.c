#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "parser.h"
#include <stdlib.h>
#include <errno.h>

#define BUFLEN 1024

//To Do: This base file has been provided to help you start the lab, you'll need to heavily modify it to implement all of the features
void honour_PATH(char *args[]){
    char command[BUFLEN];
                 
                if (command[0] == '/') {
                    execve(args[0], args, NULL);
                } else if (strchr(args[0], '/')) {
                    execve(args[0], args, NULL);
                } else {
                    char *path_env = getenv("PATH");
                    if (path_env) {
                        char *path_copy = strdup(path_env);
                        char *dir = strtok(path_copy, ":");
                        char fullpath[1024];

                        while (dir != NULL) {
                            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, args[0]);
                            if (access(fullpath, X_OK) == 0) {
                                execve(fullpath, args, NULL);
                            }
                            dir = strtok(NULL, ":");
                        }
                        free(path_copy);
                    }
                    fprintf(stderr, "Command not found: %s\n", args[0]);
                    _exit(1);
                }

}


int main() {
    char buffer[1024];
    char* parsedinput;
    char* args[3];
    char newline;

    printf("Welcome to the Group23 shell! Enter commands, enter 'quit' to exit\n");
    do {
        //Print the terminal prompt and get input
        printf("$ ");
        char *input = fgets(buffer, sizeof(buffer), stdin);
        if(!input)
        {
            fprintf(stderr, "Error reading input\n");
            return -1;
        }
        
        //Clean and parse the input string
        parsedinput = (char*) malloc(BUFLEN * sizeof(char));
        size_t parselength = trimstring(parsedinput, input, BUFLEN);

        //Sample shell logic implementation
        if ( strcmp(parsedinput, "quit") == 0 ) {
            printf("Bye!!\n");
            return 0;
        }
        else {
            pid_t pid = fork();
            if (pid == 0){
                char *args[] = {parsedinput, NULL};
                parse_args(parsedinput, args, 64);
                honour_PATH(args);
                
                perror("execve failed");
                _exit(1);

            } else if (pid > 0 ){
                int status;
                waitpid(pid, &status, 0);
            } else {
                perror("fork failed.");
            }
            
        }

        //Remember to free any memory you allocate!
        free(parsedinput);
    } while ( 1 );

    return 0;
}
