#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "parser.h"
#include <stdlib.h>
#include <errno.h>

#define BUFLEN 1024

int main()
{
    char buffer[1024];
    char *parsedinput;

    printf("Welcome to the Group23 shell! Enter commands, enter 'quit' to exit\n");
    do
    {
        // Print the terminal prompt and get input
        printf("$ ");
        char *input = fgets(buffer, sizeof(buffer), stdin);
        if (!input)
        {
            fprintf(stderr, "Error reading input\n");
            return -1;
        }

        // Clean and parse the input string
        // Clean and parse the input string
        parsedinput = (char *)malloc(BUFLEN * sizeof(char));
        size_t parselength = trimstring(parsedinput, input, BUFLEN);

        // Skip empty input
        if (parselength == 0)
        {
            free(parsedinput);
            continue; // just print a new $ prompt
        }

        // Sample shell logic implementation
        if (strcmp(parsedinput, "quit") == 0)
        {
            printf("Bye!!\n");
            free(parsedinput);
            return 0;
        }
        else
        {
            // Check if there's a pipe in the command
            int pipe_pos = findpipe(parsedinput, BUFLEN);

            if (pipe_pos != -1)
            {
                // handle pipe: split into two commands
                parsedinput[pipe_pos] = '\0'; // Split at pipe
                char *cmd1 = parsedinput;
                char *cmd2 = parsedinput + pipe_pos + 1;

                // Create pipe
                int pipefd[2];
                if (pipe(pipefd) == -1)
                {
                    perror("pipe failed");
                    free(parsedinput);
                    continue;
                }

                // Fork first child for first command
                pid_t pid1 = fork();
                if (pid1 == 0)
                {

                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe write end
                    close(pipefd[1]);

                    char *args1[64];
                    parse_args(cmd1, args1, 64);
                    honour_PATH(args1);

                    perror("execve failed");
                    _exit(1);
                }
                else if (pid1 < 0)
                {
                    perror("fork failed");
                    close(pipefd[0]);
                    close(pipefd[1]);
                    free(parsedinput);
                    continue;
                }

                // Fork second child for second command
                pid_t pid2 = fork();
                if (pid2 == 0)
                {
                    // Second child: read from pipe
                    close(pipefd[1]);              // Close write end
                    dup2(pipefd[0], STDIN_FILENO); // Redirect stdin to pipe read end
                    close(pipefd[0]);

                    char *args2[64];
                    parse_args(cmd2, args2, 64);
                    honour_PATH(args2);

                    perror("execve failed");
                    _exit(1);
                }
                else if (pid2 < 0)
                {
                    perror("fork failed");
                    close(pipefd[0]);
                    close(pipefd[1]);
                    free(parsedinput);
                    continue;
                }

                // Parent: close both pipe ends and wait for both children
                close(pipefd[0]);
                close(pipefd[1]);

                int status;
                waitpid(pid1, &status, 0);
                waitpid(pid2, &status, 0);
            }
            else
            {
                // No pipe: single command execution
                pid_t pid = fork();
                if (pid == 0)
                {
                    char *args[64];
                    parse_args(parsedinput, args, 64);
                    honour_PATH(args);

                    perror("execve failed");
                    _exit(1);
                }
                else if (pid > 0)
                {
                    int status;
                    waitpid(pid, &status, 0);
                }
                else
                {
                    perror("fork failed.");
                }
            }
        }

        // Remember to free any memory you allocate!
        free(parsedinput);
    } while (1);

    return 0;
}