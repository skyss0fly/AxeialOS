#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_INPUT 256
#define MAX_ARGS 16

// Simple function to split input line into arguments
int parse_args(char* line, char* argv[]) {
    int argc = 0;
    char* token = strtok(line, " \t\n");
    while (token != NULL && argc < MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;
    return argc;
}

// Execute a program using fork + exec
void run_program(char* argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execve(argv[0], argv, NULL);
        // If exec fails:
        fprintf(stderr, "Failed to exec %s\n", argv[0]);
        _exit(1);
    } else if (pid > 0) {
        // Parent process, wait for child
        int status;
        waitpid(pid, &status, 0);
    } else {
        fprintf(stderr, "fork() failed\n");
    }
}

int main(void) {
    char line[MAX_INPUT];
    char* argv[MAX_ARGS];
    int argc;

    printf("Zhell V0.1!\n");
    fflush(stdout);

    while (1) {
        printf("$ ");        // prompt
        fflush(stdout);

        // Read user input
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            continue;
        }

        // Parse input
        argc = parse_args(line, argv);
        if (argc == 0) continue;

        // Handle built-in commands
        if (strcmp(argv[0], "exit") == 0) {
            printf("Exiting shell...\n");
            break;
        } else if (strcmp(argv[0], "help") == 0) {
            printf("Available commands:\n");
            printf("  help  - Show this message\n");
            printf("  echo  - Print text\n");
            printf("  exit  - Exit the shell\n");
            continue;
        } else if (strcmp(argv[0], "echo") == 0) {
            for (int i = 1; i < argc; i++) {
                printf("%s ", argv[i]);
            }
            printf("\n");
            continue;
        }

        // Execute external program
        run_program(argv);
    }

    return 0;
}
