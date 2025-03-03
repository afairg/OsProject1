#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_ARGS 1024        // Maximum number of arguments for a command
#define MAX_PATH_LENGTH 1024 // Maximum length of a path

// Global variables for the shell's search path
char **path_dirs = NULL;     // Array of directories in the search path
int num_path_dirs = 0;       // Number of directories in the search path

// Function to free memory allocated for the search path
void free_path_dirs() {
    if (path_dirs != NULL) {
        for (int i = 0; i < num_path_dirs; i++) {
            free(path_dirs[i]); // Free each directory string
        }
        free(path_dirs); // Free the array itself
        path_dirs = NULL;
        num_path_dirs = 0;
    }
}

// Function to initialize the default search path (/bin)
void initialize_default_path() {
    free_path_dirs(); // Free any existing path
    path_dirs = malloc(sizeof(char *)); // Allocate memory for one directory
    if (path_dirs == NULL) {
        perror("malloc failed");
        exit(1);
    }
    path_dirs[0] = strdup("/bin"); // Set the default path to /bin
    if (path_dirs[0] == NULL) {
        perror("strdup failed");
        exit(1);
    }
    num_path_dirs = 1; // Update the number of directories
}

// Function to parse a line of input into tokens (arguments)
char **parse_line(char *line) {
    char **tokens = malloc(MAX_ARGS * sizeof(char *)); // Allocate memory for tokens
    if (!tokens) {
        perror("malloc failed");
        exit(1);
    }

    // Tokenize the input line using spaces, tabs, and newlines as delimiters
    char *token = strtok(line, " \t\n");
    int i = 0;
    while (token != NULL && i < MAX_ARGS - 1) {
        tokens[i] = token; // Store each token
        i++;
        token = strtok(NULL, " \t\n");
    }
    tokens[i] = NULL; // Null-terminate the array of tokens

    return tokens;
}

// Function to execute an external command
void execute_external_command(char **args, int redirect, char *output_file) {
    char *command = args[0]; // The first token is the command
    if (command == NULL) {
        return; // No command to execute
    }

    // If the command contains a '/', treat it as an absolute or relative path
    if (strchr(command, '/') != NULL) {
        if (access(command, X_OK)) { // Check if the command is executable
            fprintf(stderr, "An error has occurred\n");
            return;
        }
    } else {
        // Search for the command in the directories specified by the path
        int found = 0;
        for (int i = 0; i < num_path_dirs; i++) {
            char *dir = path_dirs[i];
            char full_path[MAX_PATH_LENGTH];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, command); // Construct full path
            if (access(full_path, X_OK) == 0) { // Check if the command is executable
                command = full_path; // Use the full path
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "An error has occurred\n");
            return;
        }
    }

    // Fork a child process to execute the command
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (redirect) {
            // Redirect output to the specified file
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                fprintf(stderr, "An error has occurred\n");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO); // Redirect stdout
            dup2(fd, STDERR_FILENO); // Redirect stderr
            close(fd);
        }
        execv(command, args); // Execute the command
        fprintf(stderr, "An error has occurred\n"); // If execv fails
        exit(1);
    } else if (pid > 0) {
        // Parent process waits for the child to finish (if not parallel)
        if (!redirect) {
            waitpid(pid, NULL, 0);
        }
    } else {
        // Fork failed
        fprintf(stderr, "An error has occurred\n");
    }
}

// Function to process a command (built-in or external)
void process_command(char **tokens, int parallel) {
    if (tokens[0] == NULL) {
        return; // No command to process
    }

    // Handle built-in commands
    if (strcmp(tokens[0], "exit") == 0) {
        if (tokens[1] != NULL) {
            fprintf(stderr, "An error has occurred\n"); // exit should have no arguments
            return;
        }
        exit(0); // Exit the shell
    } else if (strcmp(tokens[0], "cd") == 0) {
        if (tokens[1] == NULL || tokens[2] != NULL) {
            fprintf(stderr, "An error has occurred\n"); // cd should have exactly one argument
        } else {
            if (chdir(tokens[1])) { // Change directory
                fprintf(stderr, "An error has occurred\n");
            }
        }
    } else if (strcmp(tokens[0], "path") == 0) {
        // Update the search path
        free_path_dirs(); // Free the old path
        num_path_dirs = 0;
        for (int i = 1; tokens[i] != NULL; i++) {
            num_path_dirs++; // Count the number of new directories
        }
        if (num_path_dirs > 0) {
            path_dirs = malloc(num_path_dirs * sizeof(char *)); // Allocate memory for the new path
            if (path_dirs == NULL) {
                perror("malloc failed");
                exit(1);
            }
            for (int i = 0; i < num_path_dirs; i++) {
                path_dirs[i] = strdup(tokens[i + 1]); // Copy each directory
                if (path_dirs[i] == NULL) {
                    perror("strdup failed");
                    exit(1);
                }
            }
            for (int i = 0; i < num_path_dirs; i++) {
                printf ("new path: %s\n", path_dirs[i]);
            }
        }
    } else {
        // Handle external commands
        int redirect = 0;
        char *output_file = NULL;
        for (int i = 0; tokens[i] != NULL; i++) {
            if (strcmp(tokens[i], ">") == 0) {
                // Check for output redirection
                if (tokens[i + 1] == NULL || tokens[i + 2] != NULL) {
                    fprintf(stderr, "An error has occurred\n");
                    return;
                }
                redirect = 1;
                output_file = tokens[i + 1]; // File to redirect output to
                tokens[i] = NULL; // Remove the '>' and filename from arguments
                break;
            }
        }
        execute_external_command(tokens, redirect, output_file);
        exit(0);
    }
}

// Main function
int main(int argc, char *argv[]) {
    if (argc > 2) {
        // Error: too many arguments
        fprintf(stderr, "An error has occurred\n");
        exit(1);
    }

    FILE *input_stream;
    int batch_mode = 0;

    if (argc == 2) {
        // Batch mode: read commands from a file
        input_stream = fopen(argv[1], "r");
        if (input_stream == NULL) {
            fprintf(stderr, "An error has occurred\n");
            exit(1);
        }
        batch_mode = 1;
    } else {
        // Interactive mode: read commands from stdin
        input_stream = stdin;
    }

    initialize_default_path(); // Initialize the default search path

    char *line = NULL;
    size_t len = 0;

    while (1) {
        if (!batch_mode) {
            // Print the prompt in interactive mode
            printf("wish> ");
            fflush(stdout);
        }

        // Read a line of input
        ssize_t read = getline(&line, &len, input_stream);
        if (read == -1) {
            break; // Exit on EOF
        }

        // Split the input line into commands separated by &
        char *command = strtok(line, "&");
        while (command != NULL) {
            char **tokens = parse_line(command);
            if (tokens[0] != NULL) {
                // Process the command (parallel if there are more commands)
                process_command(tokens, command != line);
            }
            free(tokens);
            command = strtok(NULL, "&");
        }
    }

    free(line); // Free memory allocated for the input line
    if (input_stream != stdin) {
        fclose(input_stream); // Close the batch file
    }

    free_path_dirs(); // Free memory allocated for the search path

    return 0;
}
