#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_BUFFER_SIZE 80 // buffer to hold user input
#define BUILTIN_COMMANDS 4 // number of bult-in commands

char PATH[1024]; // buffer to hold the home folder path
char* builtin_commands[] = {"exit", "cd", "help", "history"}; // the built-in commands

// kills the program
void sigint_handler(int sig) {
	write(1, "\nmini-shell terminated\n", 23);
	exit(0);
}

// saves the history of user inputs into a hidden file called history.txt located in the home folder 
void save_history(char* input) { 
	strcat(strcpy(PATH, getenv("HOME")), "/.history.txt"); // concatenates the file name with the home directory path 
	FILE* history = fopen(PATH, "a"); // opens the file in append mode (creates a new file if it doesn't already exists, otherwise it adds to it without overwriting it)
	fprintf(history, "%s", input); // writes user inputs in the file
	fclose(history); // closes the file
}

// gets user input
char* get_input(char* buffer) {
	printf("mini-shell>"); // the shell prompt
	fgets(buffer, MAX_BUFFER_SIZE, stdin); // scans the whole user input as a single string 
		
	return buffer;
}

// processes user input by tokenizing each word
char** get_args(char* input, char** args) {
	int i = 0;
	char* token = strtok(input, " ");
	while (NULL != token) { // tokenizes user input
		args[i] = token;
		token = strtok(NULL, " ");
		i++;
	}
	args[i] = NULL; // sets the last token to NULL
	int j = 0;
	while (NULL != args[j]) { // removes the newline character from each token
		args[j] = strtok(args[j], "\n");
		j++;
	}
	
	return args;
}

// exits out of the mini-shell
int builtin_exit(char** args) {
	return 0;
}

// changes the current directory
int builtin_cd(char** args) {
	if (NULL == args[1]) { // prints an error message if the user doesn't input the new directory
		fprintf(stderr, "mini-shell: please provide the new directory\n");
	} else if (chdir(args[1]) < 0) { // changes directory if the new one exists, otherwise prints an error message
		perror("mini-shell");
	}
	
	return 1;
}

// prints the available built-in commands and a short description for each of them 
int builtin_help(char** args) {
	puts("Welcome to Riccardo's mini-shell!");
	puts("\nAvailable built-in commands:");
	puts("\t-exit(terminates the shell)");
	puts("\t-cd(changes directory)");
	puts("\t-help(lists the available built-in commands)");
	puts("\t-history(lists all previous user inputs)");

	return 1;
}

// prints the history of user inputs by reading them from a file
int builtin_history(char** args) {
	FILE *history = fopen(PATH, "r"); // opens the file in reading mode
	if (NULL == history) { // if the file doesn't exist or it's not found, prints an error message 
		fprintf(stderr, "history.txt not found\n");
		exit(1);
	} else {
		char buffer[MAX_BUFFER_SIZE]; // temporary buffer to hold user inputs read in
		fseek(history, 0, SEEK_SET); // reads line by line
		while (fgets(buffer, MAX_BUFFER_SIZE, history)) { // prints line by line
			printf("%s", buffer);
		}
	}
	fclose(history);

	return 1;
}

// an array of function pointers pointing to the functions implementing the built-in commands
int (*builtin_functions[]) (char**) = {
	&builtin_exit,
	&builtin_cd,
	&builtin_help,
	&builtin_history
};

// executes terminal built-in commands in a separate process
int execute_without_pipe(char** args) {
	pid_t child_process_id; // stores the child process's id
	child_process_id = fork(); // initializes the separate process making a copy of teh current one
	if (-1 == child_process_id) { // if forking fails, prints an error message
		puts("Fork failed for some reasoni!");
		exit(EXIT_FAILURE);
	}
	if (0 == child_process_id) { // goes into the child process
		if (-1 == execvp(args[0], args)) { // executes the terminal built-in command, otherwise prints an error message
			puts("Command not found--Did you mean something else?");
		}
		exit(1);
	} else {
		wait(NULL); // waits for the child to finish execution
	}

	return 1;
}

// executes terminal built-in commands by feeding the output of the first command into the input of the second command
int execute_with_pipe(char** args) {
	int i = 0;
	char* left_side[3]; // stores the first command together with a flag and NULL
	char* right_side[3]; // stores the second command together with a flag and NULL
	while (0 != strcmp(args[i], "|")) { // saves the tokens on the left side of the pipe character into a character array
		left_side[i] = args[i];
		i++;
	}
	left_side[i] = NULL; // adds NULL as the last token of the first command
	i++; // skips the pipe character so it doesn't get stored
	int j = 0;
	while (args[i] != NULL) { // saves the tokens on the right side of the pipe character into a character array
		right_side[j] = args[i];
		i++;
		j++;
	}
	right_side[j] = NULL; // adds NULL as the last token of the second command
	int fd[2]; // stores the file descriptors
	if (-1 == pipe(fd)) { // starts piping
		puts("Piping failed for some reason!");
		exit(EXIT_FAILURE);
	}
	pid_t child_process_id;
	int child_status;
	pid_t child_process_id2;
	int child_status2;
	child_process_id = fork();
	if(-1 == child_process_id) {
		puts("Fork failed for some reason!");
		exit(EXIT_FAILURE);
	} else if (0 == child_process_id) { // 1st child
		dup2(fd[1], STDOUT_FILENO);
		close(fd[0]);
		close(fd[1]);
		if (-1 == execvp(left_side[0], left_side)) {
			puts("Command not found--Did you mean something else?");
		}
		exit(1);
	}
	child_process_id2 = fork();
	if (-1 == child_process_id2) {
		puts("Fork failed for some reason!");
		exit(EXIT_FAILURE);
	} else if (0 == child_process_id2) { // 2nd child
		dup2(fd[0], STDIN_FILENO);
		close(fd[0]);
		close(fd[1]);
		if (-1 == execvp(right_side[0], right_side)) {
			puts("Command not found--Did you mean something else?");
		}
		exit(1);
	} else { // parent
		close(fd[0]);
		close(fd[1]);
		waitpid(child_process_id, &child_status, 0); // waits execution of the 1st child
		waitpid(child_process_id2, &child_status2, 0); // waits execution of the 2nd child
	}
	
	return 1;
}	

// decides which execution to run based on user input
int execute(char** args) {
	int j = 0;
	int loop_status;
	int i;
	for (i = 0; i < BUILTIN_COMMANDS; i++) { // if the user inputs a built-in command, executes it
			if (0 == strcmp(args[0], builtin_commands[i])) {
				loop_status = (*builtin_functions[i])(args);
				return loop_status;
			}
	}	
	while (NULL != args[j]) { // if the user inputs the pipe character, executes the execute_with_pipe function
		if (0 == strcmp(args[j], "|")) {
			loop_status = execute_with_pipe(args);
			return loop_status;
		}
		j++;
	}
	loop_status = execute_without_pipe(args); // if the user inputs a terminal built-in command, executes the execute_without_pipe function

	return loop_status;
}

int main() {
	alarm(180); // kills the program after 180 seconds
	
	signal(SIGINT, sigint_handler); // kills the program with ctrl-c

	char* input = (char*)malloc(sizeof(char) * MAX_BUFFER_SIZE); // allocates memory on the heap for user input
	if (NULL == input) {
		perror("malloc");
		return -1;
	}
	char** args_buffer = (char**)malloc(sizeof(char*) * MAX_BUFFER_SIZE); // allocates memory on the heap for the tokens
	if (NULL == args_buffer) {
		perror("malloc");
		return -1;
	}
	char** args; // initializes the variable that refers to the tokens
	int loop_status = 1; // initializes the variable that makes the loop run forever unless the exit function is called, which changes it to 0
	
	while(loop_status) {
		input = get_input(input);
		if (32 == input[0] || 9 == input[0] || 0 == strcmp(input, "\n") || NULL == input) { // if the user inputs spaces, tabs, returns, or NULL is assigned to the input, just asks for input again
			continue;
		} else {
			save_history(input); // saves user input to be retrieved as history
			args = get_args(input, args_buffer); // tokenizes user input
			loop_status = execute(args); // executes the commands
		}
	}
	free(input);
	free(args);

	return 0;
}
