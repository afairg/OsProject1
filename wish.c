#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_ARGS 1024 // Max number of arguements command
#define MAX_PATH 1024 // Max number of length of a path

// Global variables for the shell's path
char **path_dirs = NULL; // Array of directories
int num_path_dirs = 0; // Number of directories

// Function to free memory for searched path
void free_path(){
    if(path_dirs != NULL){
        for(int i =0; i < num_path_dirs; i++){
            free(path_dirs[i]) // free function to free the directory
        }
        free(path_dirs);
        path_dirs = NULL;
        num_path_dirs = 0;

    }
}

// Function to intialize the deafult search path to /bin
void intialize_deafult_path(){
    free_path_dirs(); // Free any existing path
    path_dirs = malloc(sizeof(char *)); // Allocate memory for one directory
    if (path_dirs[0] == NULL){
        perror("malloc failed");
        exit(1);
    }
    
    path_dirs[0] = strdup("/bin"); // set deafult to /bin
    
    if(path_dir[0] == NULL){
        perror("strdup failed");
        exit(1);
    }
    
    num_path_dirs = 1; // Update the number of directories
}

// function to parse a line of input into token (arguement)
char **parse_line(char * line){
    char **tokens = malloc(MAX_ARGS * sizeof(char *)); // allocate memories for tokens
    if(!tokens){
        perror("malloc failed");
        exit(1);
    }

    // Tokenize the input line using spaces, tabs, and newlines as delimiters
    char * token = strok(line, " \t\n");
    int i = 0;
    while(token != NULL && i < MAX_ARGS - 1){
        tokens[i] = token; // Store each token
        i++;
        token = strtok(NULL, " \t\n");
    }
    tokens[i] = NULL; // Null teriminate the entire array of tokens

    return tokens;
}

// Function to execute exeternal command
void execute_external_command(char **args, int redirect, char *output_file){
    char *command = args[0]; // First token is command
    if(command == NULL){
        return // no command to execute
    }

    // If command contains a '/', treat it as as a realative path
    if(strchr(command, '/') != NULL){
        if(access(command, X_OK)){ // Check if the command is executable
            fprintf(stderr, "An error has occured\n");
            return;
        }

    }
    else{
        // Search for the command in the directories specified by the path
        int found = 0;
        for(int i = 0; i < num_path_dirs; i++){
            char *dir = path_dirs[i];
            char full_path[MAX_PATH_LENGTH];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, command) // Construct full path

            if(access(full_path, X_OK) == 0){
                command = full_path; // Use 
                found = 1;
                break;
            }
        }
        if(!found){
            fprintf(stderr, "An error has occured\n");
            return;
        }
    }

    //Fork a child process to execute the command
    pid_t pid = fork();
    if(pid == 0){
        // Child process
        if(redirect){
            // Redirect output to the specificed file
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd < 0){
                fprintf(stderr, "An error had occured\n");
                exit(1);
            }
            dup2(fd,STDOUT_FILENO); // Redirect stdout
            dup2(fd, STDERR_FILENO); // Redirect stderr
            close(fd);
        }
        execv(command,args);
        fprintf(stderr, "An error has occured\n");
        exit(1);
    }
    else if(pid > 0){
        //parent process waits for child to finish
        waitpid(pid, NULL, 0);
    }
    else{
        //fork failed
        fprintf(stderr, "An error has occured\n");
    }
}

// Function to process a command (built-in or external)
void process_command(char **tokens, int parallel){
    if(tokens[0] == NULL){
        return; // No command to process
    }

    // Handle built-in commands
    if(strcmp(tokens[0], "exit") == 0){
        if(tokens[1] != NULL){
            fprintf(stderr, "An error has occured\n"); // exit should have no arguments
            return;
        }
        exit(0); // Exit the shell 
    }
    else if(strcmp(tokens[0], "cd") == 0){
        if(tokens[1] == NULL || tokens[2] != NULL){
            fprintf(stderr, "An error has occured\n"); // cd should have exactly one argument
        }
        else{
            if(chdir(tokens[1])){ // Changed directory
                fprintf(stderr, "An error has occurred\n");
            }
        }
    }
    else if(strcmp(tokens[0], "path") == 0){
        // Update the search path
        free_path_dirs(); // Free the old path
        num_path_dirs = 0;
        for(int i = 1; tokens[i] != NULL; i++){
            num_path_dirs++ // Count the number of new directories
        }
        if(num_path_dirs > 0){
            path_dirs = malloc(num_path_dirs * sizeof(char *)); // Allocate memory for new path
            if(path_dirs == NULL){
                perror("malloc failed");
                exit(1);
            }
        }
        for(int i = 0; i < num_path_dirs; i++){
            path_dirs[i] = strdup(tokens[i + 1]);
            if(path_dirs == NULL){
                perror("strdup failed");
                exit(1);
            }
        }
    }
    else{
        // Handle the external commands
        int redirect = 0;
        char *output_file = NULL;
        for(int i = 0; tokens[i] != NULL; i++){
            if(strcmp(tokens[i], ">") == 0){
                // Check for output redirection
                if(tokens[i + 1] == NULL || tokens[i + 2] != NULL){
                    fprintf(stderr, "An error has occured\n");
                    return;
                }
                redirect = 1;
                output_file = tokens[i + 1];
                tokens[i] = NULL;
                break;
            }
        }
        if(!parallel){
            // Execute the command in the foreground
            execute_external_command(tokens, redirect, output_file);
        }
        else{
            // Execute the command in the background
            pid_t pid = fork();
            if(pid == 0){
                execute_external_command(tokens, redirect, output_file);
                exit(0);
            }
            else if(pid < 0){
                fprintf(stderr, "An error has occured\n");
            }
        }
    }
}


int main(int argc, char *argv[]) {
    if(argv > 2){
        fprintf(stderr, "An error had occured\n");
        exit(1);
    }

    FILE *input_stream;
    int batch_mode = 0

    if(argc == 2){
        // Batch Mode: Read commands from a file
        input_stream = fopen(argv[1],"r");
        if(input_stream == NULL){
            fprintf(stderr, "An error had occured\n");
            exit(1);
        }
        batch_mode = 1;
    }
    else{
        // Interactive Mode: Read commands from stdin
        input_stream = stdin;
    }

    intialize_deafult_path();

    char *line = NULL;
    size_t len = 0;

    while(1){
        if(!batch_mode){
            // Print the prompt in interactive mode
            printf("wish> ");
            fflush(stdout);
        }

        // Read the line of input
        ssize_t read = getline(&line, &len, input_stream);
        if(read == -1){
            break; // Exit EOF
        }

        // Parse the input line into tokens
        char **tokens = parse_line(line);
        if(tokens[0] == NULL){
            free(tokens);
            continue // Skip empty line
        }

        // Check for parallel commands (seperated by '&')
        int parallel = 0;
        for(int i = 0; tokens[i] != NULL, i++){
            if(strcmp(tokens[i], "&") == 0){
                parallel = 1;
                tokens[i] = NULL; // Remove "&" from arguments
                break;
            }
        }

        if(parallel){
            // Execute commands  in parallel
            for(int i = 0; tokens[i] !- NULL; i++){
                char **sub_tokens = parse_line(tokens[i]);
                process_command(sub_tokens, 1); // Process each command in the background
                free(sub_tokens);
            }
        }
        else{
            // Execute a single command
            process_command(tokens, 0);
        }

        free(tokens); // Free memory allocated for tokens
    }

    free(line); // Free memory allocatedfor the input line
    if(input_stream != stdin){
        fclose(input_stream); // Close the batch file 
    }

    free_path_dirs() // Free memory allocated for the search file
    return 0;
}
