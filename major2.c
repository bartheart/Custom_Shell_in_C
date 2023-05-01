//major2.c

#include "major2.h"

///command line max input length
#define MAX_LENGTH 512

//global variables for signal handling
pid_t foreground_pgid;
struct sigaction default_action;

//main function
int main(int argc, char* argv[]) {
    //intialize prompt buffer
    char prompt[MAX_LENGTH];
    char choice;

    //check if user wants to customize prompt
    printf("Do you want to customize your shell prompt for extra credit? (y/n): ");
    scanf("%c", &choice);
    clean_stdin();

    //custom prompt
    if (choice == 'y') {
        //prompt shell 
        printf("Enter your custom shell prompt: ");
        fgets(prompt, sizeof(prompt), stdin);
        //strip the newline character
        prompt[strcspn(prompt, "\n")] = 0;
    } else {
        strcpy(prompt, "prompt");
    }

    //check if user specified batch file
    if (argc == 2) {
        execute_batch(argv[1]);
        return 0;
    }

    //register signal handler
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting SIGINT handler");
    }
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("Error setting SIGTSTP handler");
    }

    //run in interactive mode
    char* command = NULL;
    //initalize size for input buffer
    size_t len = 0;
    while (1) {
        printf("%s: ", prompt);
        //clean_stdin();

        //read input
        if (getline(&command, &len, stdin) == -1) {
            perror("Error reading input");
            break;
        }

        //execute command
        execute_command(command);
    }

    free(command);
    return 0;
}

void execute_command(char *command){
    char* sub_commands[MAX_LENGTH/2 +1];
    char* token = strtok(command, ";\n");
    int num_sub_commands = 0;

    while (token != NULL){
        sub_commands[num_sub_commands++] = token;
        token = strtok(NULL, ";\n");
    }

    execute_sub_commands(sub_commands, num_sub_commands);
}

void execute_sub_commands(char** sub_commands, int num_sub_commands){
    for (int i = 0; i < num_sub_commands; i++) {
        char* args[MAX_LENGTH/2 +1];
        char* arg_token = strtok(sub_commands[i], " \n");
        int num_args = 0;
        
        while (arg_token != NULL) {
            args[num_args++] = arg_token;
            arg_token = strtok(NULL, " \n");
        }
        args[num_args] = NULL;

        if (num_args == 0) {
            continue;
        }

        if (strcmp(args[0], "path") == 0) {
            handle_path(args);
        } else if (strcmp(args[0], "exit") == 0) {
            exit_shell();
        } else if (strcmp(args[0], "cd") == 0) {
            change_directory(args, num_args);
        } else {
            execute_single_command(args, num_args);
        }
    }
}

void execute_single_command(char** args, int num_args) {
    // Check for pipes and execute pipes if present
    int pipe_positions[num_args];
    int pipe_count = 0;
    int status;

    check_for_pipes(args, num_args, pipe_positions, &pipe_count);
    if (pipe_count > 0) {
        execute_pipes(args, num_args, pipe_positions, pipe_count);
        return;
    }

    // Check for input/output redirection and open the files if present
    int inputFd = STDIN_FILENO, outputFd = STDOUT_FILENO;
    check_for_redirection(args, num_args, &inputFd, &outputFd);

    // create new process group before forking
    setpgid(0, 0);

    // Fork a child process to execute the command
    pid_t pid = fork();
    if (pid < 0) {
        perror("Error forking child process");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        // Redirect input/output if necessary
        setpgid(getpid(), getpid());
        if (inputFd != STDIN_FILENO) {
            if (dup2(inputFd, STDIN_FILENO) == -1) {
                perror("Error redirecting input");
                exit(EXIT_FAILURE);
            }
            close(inputFd);
        }
        if (outputFd != STDOUT_FILENO) {
            if (dup2(outputFd, STDOUT_FILENO) == -1) {
                perror("Error redirecting output");
                exit(EXIT_FAILURE);
            }
            close(outputFd);
        }

        // Execute the command
        if (execvp(args[0], args) == -1) {
            perror("Error executing command");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process
        setpgid(getpid(), getpid());
        waitpid(pid, &status, 0);

        // Close the files if necessary
        if (inputFd != STDIN_FILENO) {
            close(inputFd);
        }
        if (outputFd != STDOUT_FILENO) {
            close(outputFd);
        }
    }
}

//handle batch execution
void execute_batch(char* file_name) {
    //open batch file
    FILE* file = fopen(file_name, "r");
    if (file == NULL) {
        perror("Error opening batch file");
        return;
    }

    //read each line and execute command
    char line[MAX_LENGTH];
    while (fgets(line, MAX_LENGTH, file) != NULL) {
        //echo line back to user
        printf("%s", line);

        //execute command
        execute_command(line);
    }

    fclose(file);
}

void check_for_pipes(char** args, int num_args, int* pipe_positions, int* pipe_count) {
    //intitalize pipe count
    *pipe_count = 0;
    for (int i = 0; i < num_args; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_positions[*pipe_count] = i;
            (*pipe_count)++;
        }
    }
}

//Implement execute pipes
void execute_pipes(char **args, int num_args, int *pipe_positions, int pipe_count) {
    int fd[2 * pipe_count];
    // Create pipes
    for (int i = 0; i < pipe_count; i++) {
        if (pipe(fd + 2 * i) < 0) {
            perror("Pipe creation failed");
            return;
        }
    }

    for (int i = 0; i <= pipe_count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            return;
        }

        if (pid == 0) {
            if (i != 0) { // Not the first command
                if (dup2(fd[2 * (i - 1)], 0) < 0) {
                    perror("Dup2 failed");
                    return;
                }
            }

            if (i != pipe_count) { // Not the last command
                if (dup2(fd[2 * i + 1], 1) < 0) {
                    perror("Dup2 failed");
                    return;
                }
            }

            // Close all pipes in the child process
            for (int j = 0; j < 2 * pipe_count; j++) {
                close(fd[j]);
            }

            // Execute the command
            char *cmd_args[MAX_LENGTH/2 +1];
            int start = (i == 0) ? 0 : pipe_positions[i - 1] + 1;
            int end = (i == pipe_count) ? num_args : pipe_positions[i];
            int k = 0;
            for (int j = start; j < end; j++) {
                cmd_args[k++] = args[j];
            }
            cmd_args[k] = NULL;

            if (execvp(cmd_args[0], cmd_args) < 0) {
                perror("Execvp failed");
                exit(1);
            }
        }
    }

    // Close all pipes in the parent process
    for (int j = 0; j < 2 * pipe_count; j++) {
        close(fd[j]);
    }

    // Wait for all child processes to finish
    for (int i = 0; i <= pipe_count; i++) {
        wait(NULL);
    }
}

void check_for_redirection(char** args, int num_args, int* inputFd, int* outputFd) {
    for (int i = 0; i < num_args; i++) {
        if (strcmp(args[i], "<") == 0) {
            if (i + 1 >= num_args) {
                fprintf(stderr, "Error: no input file specified after '<'\n");
                return;
            }
            *inputFd = open(args[i+1], O_RDONLY);
            if (*inputFd == -1) {
                perror("Error opening input file");
                return;
            }
            args[i] = NULL;
            args[i+1] = NULL;
            i++;
        } else if (strcmp(args[i], ">") == 0) {
            if (i + 1 >= num_args) {
                fprintf(stderr, "Error: no output file specified after '>'\n");
                return;
            }
            *outputFd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (*outputFd == -1) {
                perror("Error opening output file");
                return;
            }
            args[i] = NULL;
            args[i+1] = NULL;
            i++;
        }
    }

}

//handle path built in command
void handle_path(char** args) {
    if (args[1] == NULL) {
            printf("Invalid path command. Please specify an operation (add, remove, or reset).\n");
            return;
        }

        if (strcmp(args[1], "add") == 0) {
            if (args[2] != NULL) {
                char *path_env = getenv("PATH");
                char new_path[MAX_LENGTH];
                strcpy(new_path, path_env);
                strcat(new_path, ":");
                strcat(new_path, args[2]);
                setenv("PATH", new_path, 1);
                printf("Path added: %s\n", args[2]);
            } else {
                printf("Invalid path command. Please specify a path value to add.\n");
            }
        }
        else if (strcmp(args[1], "remove") == 0) {
        if (args[2] != NULL) {
            char *path_env = getenv("PATH");
            char new_path[MAX_LENGTH] = "";
            char *token = strtok(path_env, ":");
            bool removed = false;
            while (token != NULL) {
                if (strcmp(token, args[2]) != 0) {
                    if (strlen(new_path) > 0) {
                        strcat(new_path, ":");
                    }
                    strcat(new_path, token);
                } else {
                    removed = true;
                }
                token = strtok(NULL, ":");
            }
            if (removed) {
                setenv("PATH", new_path, 1);
                printf("Path removed: %s\n", args[2]);
            } else {
                printf("Path not found: %s\n", args[2]);
            }
        } else {
            printf("Invalid path command. Please specify a path value to remove.\n");
        }
    }
    else if (strcmp(args[1], "reset") == 0) {
        char original_path[] = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"; // Your original PATH value
        setenv("PATH", original_path, 1);
        printf("Path reset to the original value.\n");
    }

    else {
        printf("Invalid path command.\n");
    }
}

//change directory 
void change_directory(char** args, int num_args) {
    // Check for cd implementation
    if (strcmp(args[0], "cd") == 0) {
        // change directory to specified path or home directory
        if (num_args == 1) {
            chdir(getenv("HOME"));
        } else {
            if (chdir(args[1]) != 0) {
                perror("cd");
            }
        }
    }
}


void handle_signal(int signo) {
    // Handle the signal here
    switch (signo) {
        case SIGINT:
            // Handle interrupt signal (Ctrl+C)
            printf("Received SIGINT signal.\n");
            break;
        case SIGTSTP:
            // Handle stop signal (Ctrl+Z)
            printf("Received SIGTSTP signal.\n");
            break;
        default:
            // Handle all other signals
            printf("Received signal %d.\n", signo);
            break;
    }
}

//exit shell
void exit_shell(void){
    //exit system call
    exit(0);
}

void clean_stdin(void)
{
    int c;
    do {
        c = getchar();
    } while (c != '\n' && c != EOF);
}

