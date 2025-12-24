#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_JOBS 32

typedef struct Job {
    pid_t pid;
    char command[MAX_INPUT];
    int background;
} Job;

Job job_list[MAX_JOBS];

// ---------------- Signal Handlers ----------------
void sigint_handler(int signo) {
    printf("\n[Zhell] Ctrl-C caught! Type 'exit' to quit.\n$ ");
    fflush(stdout);
}

void sigchld_handler(int signo) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_JOBS; i++) {
            if (job_list[i].pid == pid) {
                printf("\n[Zhell] Background job finished: %s\n$ ", job_list[i].command);
                job_list[i].pid = 0;
                fflush(stdout);
                break;
            }
        }
    }
}

// ---------------- Utility Functions ----------------
int parse_args(char* line, char* argv[]) {
    int argc = 0;
    char* token = strtok(line, " \t\n");
    while (token && argc < MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;
    return argc;
}

char* expand_env(char* arg) {
    if (arg[0] == '$') {
        char* value = getenv(arg + 1);
        return value ? value : "";
    }
    return arg;
}

// ---------------- Built-ins ----------------
int handle_builtin(int argc, char* argv[]) {
    if (argc == 0) return 0;

    if (strcmp(argv[0], "exit") == 0) exit(0);

    else if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; i < argc; i++) printf("%s ", expand_env(argv[i]));
        printf("\n");
        return 1;
    }

    else if (strcmp(argv[0], "help") == 0) {
        printf("[Zhell] Built-in commands: help, echo, exit, cd, pwd\n");
        return 1;
    }

    else if (strcmp(argv[0], "cd") == 0) {
        if (argc > 1) chdir(argv[1]);
        else chdir(getenv("HOME"));
        return 1;
    }

    else if (strcmp(argv[0], "pwd") == 0) {
        char cwd[512];
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
        return 1;
    }

    return 0; // Not built-in
}

// ---------------- Run External Program ----------------
void run_program(char* argv[], int background, char* infile, char* outfile, int append) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: handle redirection
        if (infile) {
            int fd = open(infile, O_RDONLY);
            if (fd < 0) { perror("Zhell: input"); _exit(1); }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (outfile) {
            int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
            int fd = open(outfile, flags, 0644);
            if (fd < 0) { perror("Zhell: output"); _exit(1); }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execve(argv[0], argv, environ);
        perror("Zhell: execve failed");
        _exit(1);
    } else if (pid > 0) {
        if (background) {
            for (int i = 0; i < MAX_JOBS; i++) {
                if (job_list[i].pid == 0) {
                    job_list[i].pid = pid;
                    strncpy(job_list[i].command, argv[0], MAX_INPUT);
                    job_list[i].background = 1;
                    printf("[Zhell] Started background job: %s (pid=%d)\n", argv[0], pid);
                    break;
                }
            }
        } else {
            int status;
            waitpid(pid, &status, 0);
        }
    } else {
        perror("Zhell: fork failed");
    }
}

// ---------------- Parse redirection & background ----------------
void process_command(char* line) {
    char* argv[MAX_ARGS];
    int argc = 0;
    int background = 0;
    char* infile = NULL;
    char* outfile = NULL;
    int append = 0;

    // Simple tokenization for redirection & &
    char* token = strtok(line, " \t\n");
    while (token) {
        if (strcmp(token, "&") == 0) background = 1;
        else if (strcmp(token, ">") == 0) { token = strtok(NULL, " \t\n"); outfile = token; append = 0; }
        else if (strcmp(token, ">>") == 0) { token = strtok(NULL, " \t\n"); outfile = token; append = 1; }
        else if (strcmp(token, "<") == 0) { token = strtok(NULL, " \t\n"); infile = token; }
        else argv[argc++] = token;

        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;

    if (argc == 0) return;

    if (!handle_builtin(argc, argv)) run_program(argv, background, infile, outfile, append);
}

// ---------------- Main Loop ----------------
int main(void) {
    char line[MAX_INPUT];

    signal(SIGINT, sigint_handler);
    signal(SIGCHLD, sigchld_handler);

    printf("[Zhell] Welcome to Zhell v1.0 on AxeialOS!\n");

    while (1) {
        printf("$ "); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) { printf("\n"); continue; }

        // Handle comments
        char* comment = strchr(line, '#');
        if (comment) *comment = '\0';

        process_command(line);
    }

    return 0;
}
