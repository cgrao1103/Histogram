#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

#define MAX_CHAR 26

void compute_histogram(const char *filename, int pipe_fd) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int histogram[MAX_CHAR] = {0};
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (isalpha(ch)) {
            ch = tolower(ch);
            histogram[ch - 'a']++;
        }
    }

    if (write(pipe_fd, histogram, MAX_CHAR * sizeof(int)) == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> <file2> ... <fileN>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_children = argc - 1;
    int pipes[num_children][2];

    for (int i = 0; i < num_children; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_children; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            close(pipes[i][0]);
            compute_histogram(argv[i + 1], pipes[i][1]);
            close(pipes[i][1]);
            exit(EXIT_SUCCESS);
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_children; i++) {
        int histogram[MAX_CHAR];
        close(pipes[i][1]);

        if (read(pipes[i][0], histogram, MAX_CHAR * sizeof(int)) == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        close(pipes[i][0]);

        char filename[20];
        sprintf(filename, "file%d.hist", i + 1);  // Use index of the child process
        FILE *histogram_file = fopen(filename, "w");
        if (histogram_file == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < MAX_CHAR; j++) {
            fprintf(histogram_file, "%c %d\n", 'a' + j, histogram[j]);
        }
        fclose(histogram_file);
    }

    int status;
    for (int i = 0; i < num_children; i++) {
        wait(&status);
    }

    return 0;
}
