#include "parser.h"
#include <ctype.h> 
#include <unistd.h>

void honour_PATH(char *args[]){
    if (args[0][0] == '/') {
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

int parse_args(char *input, char *args[], int max_args) {
    int argc = 0;
    char *p = input;
    
    while (*p && argc < max_args - 1) {
        //skip leading whitespace
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') break;
        
        //check for quoted string
        if (*p == '"') {
            p++; 
            args[argc++] = p;
            // Find closing quote
            while (*p && *p != '"') p++;
            if (*p == '"') {
                *p++ = '\0'; 
            }
        } else {
            args[argc++] = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) *p++ = '\0';
        }
    }
    
    args[argc] = NULL;
    return argc;
}

//Function to trim whitespace and ASCII control characters from buffer
//[Input] char* inputbuffer - input string to trim
//[Input] size_t bufferlen - size of input and output string buffers
//[Output] char* outputbuffer - output string after trimming 
//[Return] size_t - size of output string after trimming
size_t trimstring(char* outputbuffer, const char* inputbuffer, size_t bufferlen)
{   
    strncpy(outputbuffer, inputbuffer, bufferlen);
    outputbuffer[bufferlen - 1] = '\0'; 

    ssize_t ii = strlen(outputbuffer) - 1; 
    while (ii >= 0 && outputbuffer[ii] < '!') {
        outputbuffer[ii] = '\0';
        ii--;
    }

    return strlen(outputbuffer);
}


//Function to test that string only contains valid ascii characters (non-control and not extended)
//[Input] char* inputbuffer - input string to test
//[Input] size_t bufferlen - size of input buffer
//[Return] bool - true if no invalid ASCII characters present
bool isvalidascii(const char* inputbuffer, size_t bufferlen)
{
    size_t testlen = bufferlen;
    size_t stringlength = strlen(inputbuffer);
    if(strlen(inputbuffer) < bufferlen){
        testlen = stringlength;
    }

    bool isValid = true;
    for(size_t ii = 0; ii < testlen; ii++)
    {
        unsigned char c = (unsigned char) inputbuffer[ii];
        //Space is allowed, but other control characters are not
        isValid &= (c >= ' ' && c <= '~');
    }

    return isValid;
}

//Function to find location of pipe character in input string
//[Input] char* inputbuffer - input string to test
//[Input] size_t bufferlen - size of input buffer
//[Return] int - location in the string of the pipe character, or -1 pipe character not found
int findpipe(const char* inputbuffer, size_t bufferlen){
    for (size_t i = 0; i < bufferlen && inputbuffer[i] != '\0'; i++) {
        if (inputbuffer[i] == '|')
            return i;
    }
    return -1;
}