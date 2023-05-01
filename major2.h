#ifndef _MAJOR2
#define _MAJOR2

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>


//function declarations
//implemnent execute command 
void execute_command(char* command);

//implement single commands 
void execute_single_command(char** args, int num_args);

//implement sub_commands
void execute_sub_commands(char** sub_commands, int num_sub_commands);

//implement batch execution
void execute_batch(char* filename);

//implement path
void handle_path(char** args);

//implement pipes
void check_for_pipes(char** args, int num_args, int* pipe_positions, int* pipe_count);

//execute pipes
void execute_pipes(char** args, int num_args, int* pipe_positions, int pipe_count);

//implement redirection
void check_for_redirection(char** args, int num_args, int* inputFd, int* outputFd);

//implement exit
void exit_shell(void);

//implement CD
void change_directory(char** args, int num_args);

//signal handling
void handle_signal(int signum);

//clean stdin
void clean_stdin(void);

#endif
