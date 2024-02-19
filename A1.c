#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

#define MAX_CHAR 26
#define MAX_CHILDREN 100

int pipes[MAX_CHILDREN][2]; // Global array of pipes
pid_t child_pids[MAX_CHILDREN] = {0}; // Initialize with zeros to track child PIDs
char *child_filenames[MAX_CHILDREN]; // Array to store child filenames

void compute_histogram(const char *filename, int pipe_fd, int child_index) {
    // Install SIGINT handler for child processes
    signal(SIGINT, SIG_DFL); 

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("fopen");
        close(pipe_fd);
        exit(EXIT_FAILURE);
    }

    printf("Processing file: %s\n", filename);

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

    // Sleep for 10+3*i seconds
    sleep(10 + 3 * child_index);

    // Construct output filename without extension
    char output_filename[300]; // Adjust buffer size as needed
    const char *file_basename = strrchr(filename, '/');
    if (file_basename == NULL)
        file_basename = filename;
    else
        file_basename++;  // Move past the '/'
    char file_basename_no_ext[256];
    strncpy(file_basename_no_ext, file_basename, strchr(file_basename, '.') - file_basename);
    file_basename_no_ext[strchr(file_basename, '.') - file_basename] = '\0';
    snprintf(output_filename, sizeof(output_filename), "%s%d.hist", file_basename_no_ext, getpid());

    // Write histogram to output file
    FILE *histogram_file = fopen(output_filename, "w");
    if (histogram_file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    for (int j = 0; j < MAX_CHAR; j++) {
        fprintf(histogram_file, "%c %d\n", 'a' + j, histogram[j]);
    }
    fclose(histogram_file);

    printf("Histogram written to: %s\n", output_filename);

    // Terminate normally
    exit(EXIT_SUCCESS);
}

void sigchld_handler(int signum) {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("Child with PID %d terminated successfully.\n", pid);
            
            // Read histogram from the corresponding child's pipe
            int index;
            for (index = 0; index < MAX_CHILDREN; index++) {
                if (child_pids[index] == pid) {
                    break;
                }
            }
            
            if (index < MAX_CHILDREN) {
                int histogram[MAX_CHAR];
                close(pipes[index][1]);

                if (read(pipes[index][0], histogram, MAX_CHAR * sizeof(int)) == -1) {
                    perror("read");
                    exit(EXIT_FAILURE);
                }
                close(pipes[index][0]);

                char filename[256];
                snprintf(filename, sizeof(filename), "%s%d.hist", child_filenames[index], pid);  // Use PID of the child process
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
        } else {
            printf("Child with PID %d terminated abnormally.\n", pid);
        }
    }
}

void sigint_handler(int signum) {
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (child_pids[i] != 0) {
            kill(child_pids[i], SIGINT);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> <file2> ... <fileN>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_children = 0;

    signal(SIGCHLD, sigchld_handler); // Register SIGCHLD handler
    signal(SIGINT, sigint_handler); // Register SIGINT handler

    for (int i = 1; i < argc; i++) { // Start from index 1 to skip program name
        if (strcmp(argv[i], "SIG") == 0) {
            // Send SIGINT to corresponding child process
            if (num_children >= MAX_CHILDREN) {
                fprintf(stderr, "Too many files provided.\n");
                exit(EXIT_FAILURE);
            }
            if (child_pids[num_children] != 0) {
                kill(child_pids[num_children], SIGINT);
            }
            num_children++;
        } else {
            // Process regular file
            if (num_children >= MAX_CHILDREN) {
                fprintf(stderr, "Too many files provided.\n");
                exit(EXIT_FAILURE);
            }
            if (pipe(pipes[num_children]) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            pid_t pid = fork();
            if (pid == 0) {
                close(pipes[num_children][0]);
                compute_histogram(argv[i], pipes[num_children][1], num_children);
                close(pipes[num_children][1]);
                exit(EXIT_SUCCESS);
            } else if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else {
                child_pids[num_children] = pid; // Track child PID
                child_filenames[num_children] = argv[i]; // Track child filename
            }
            num_children++;
        }
    }

    // Wait for all children to terminate
    while (num_children > 0) {
        int status;
        pid_t pid = wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("Child with PID %d terminated successfully.\n", pid);
        } else {
            printf("Child with PID %d terminated abnormally.\n", pid);
        }
        num_children--;
    }

    return 0;
}
