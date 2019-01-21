/**************
 * Joanna Lew
 * 5/26/17
 * HW 3: Processes & Signals
 **************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_CHAR 2048
#define MAX_ARGS 512

void main_shell();
void output_arr (char ** arguments, int size);

/**
 Struct used to track pids running in background
 @pids: array of pids
 @size: number of pids currently listed in array
 @capacity: number of pids array can hold without resizing
**/
struct pid_array {
	pid_t * pids; 
	int size;
	int capacity;
};


// pointer to strings that list env variables
extern char **env_vars_arr;

// create single pid_array in global scope
struct pid_array bg_process_pids;

/**
 remove specified pid from process pid array
 @pid - pid of process that will be removed from array
**/
void delete_bg_pid(pid_t pid) {
	int i, j;
	if (pid > 0) {
		for (i = 0; i < bg_process_pids.size; i++) {
			if (bg_process_pids.pids[i] == pid) {
				bg_process_pids.size--;
				for (j = i; j < bg_process_pids.size; j++)
					bg_process_pids.pids[j] = bg_process_pids.pids[j + 1];
			}
		}
	}
}

// create dynamic array composed of pid_t, assign it to global var
void init_bg_process_arr() {
	bg_process_pids.capacity = 10;
	bg_process_pids.size = 0;
	bg_process_pids.pids = malloc(sizeof(pid_t) * bg_process_pids.capacity );
}

/**
 check if process array has reached its capacity
 @return true if array if full; false otherwise
**/
bool process_array_is_full() {
	return (bg_process_pids.size >= bg_process_pids.capacity);
}

/**
 add pid to global process array and dynamically resize as needed
 @pid - pid of new process being added
*/
void append_bg_pid(pid_t pid) {
	// check if array is full and resize if true
	if (process_array_is_full()) {
		bg_process_pids.capacity *= 2;
		bg_process_pids.pids = realloc(bg_process_pids.pids, sizeof(pid_t) * bg_process_pids.capacity );
	}
	bg_process_pids.pids[bg_process_pids.size++] = pid;
}

/**
 check if process's pid is in bg_process_pids array
 @pid - pid of process being looked up in array
**/
bool is_bg_process_pid(pid_t pid) {
	int i;
	for (i = 0; i < bg_process_pids.size; i++) {
		if (bg_process_pids.pids[i] == pid)
			return true;
	}
	return false;
}


// kill all processes (by issuing SIGKILL) referenced in bg_process_pids array
void term_all_child_process() {
	int i;
	for (i = 0; i < bg_process_pids.size; i++) {
		kill(bg_process_pids.pids[i], SIGKILL); 	// kill all child processes
	}
}

/**
 change current working directory
 @dir - directory name (or path) user is changing into
**/
void change_dir(char * dir) {
	// check if directory can be changed into
	// output error if chdir fails
	if((!chdir(dir)) == 0) {
		perror("cd");
	}
}

/**
 see if any child processes have exited
 if true, exit status / termination signal is printed
 recursively look for other finished processes and do same
*/
void lookup_all_bg_process() {
	// cite: slide 21 lecture 9, and manpage for wait()
	// iterates through all bg id's so that we don't print foreground process?
	pid_t bg_pid = -1;
	int bg_exit_status;

	bg_pid = waitpid(-1, &bg_exit_status, WNOHANG);
	if (bg_pid != 0 && bg_pid != -1) {
		if (bg_pid > 0) {
			if (bg_exit_status !=0)
				printf("background pid %d is done: terminated by sig %d\n", bg_pid, bg_exit_status);
			else
				printf("background pid %d exited with code %d\n", bg_pid, bg_exit_status);
			delete_bg_pid(bg_pid);
		}
		// recursively check for additional processes
		lookup_all_bg_process(); 
	}
}

/**
  clean up shell by killing all child processes
  free pids array in bg_process_pids struct
**/
void term_shell() {
	term_all_child_process();
	free(bg_process_pids.pids);
	exit(0);
}

/**
 main function for smallsh program 
 this process reads and parses user text input; prints output
**/
void main_shell() {
	// store current exit status to display to user when prompted
	int foreground_exit_stat = 0;

	// runs shell process until terminated or 'exit' received
	while (true) {
		// store data to properly parse user input
		int arg_count = 0, 
			word_count = 0;
		char ** arguments = malloc(MAX_ARGS * sizeof(char *));
		char * words[MAX_ARGS + 1];
		char * command = NULL, * input_file = NULL, * output_file = NULL;
		char input[MAX_CHAR + 1];
		memset (input, '\0', MAX_CHAR);
		
		// track redirection mode, background vs foreground
		bool background_mode = false, redir_input = false, redir_output = false;
		
		struct sigaction act;
		act.sa_handler = SIG_IGN;
		sigaction(SIGINT, &act, NULL);

		// look up all running background processes
		lookup_all_bg_process();

		// prompt, then read user provided input
		printf(": ");
		fflush(stdout);
		fflush(stdin);
		fgets(input, MAX_CHAR, stdin);

		if (strlen(input) > 1) {
			// check if lines entered are comments (begin with '#')
			if (input[0] == '#') {
				continue;
			}
			input[strlen(input)-1] = '\0';
		}

		// check that user input is non-null
		if ((command = strtok(input, " \n"))) {
			// store command as arguments[0]; tokenize rest
			arguments[arg_count++] = command;

			// tokenize user input without needing to check if syntax errors occur
			while ((words[word_count] = strtok(NULL, " "))) {
				char * word = words[word_count];
				
				// end while loop if comment (word begins with '#')
				if (word[0] == '#') {
					word[0] = '\0';
					break;
				}

				// set input redirection mode and filename
				if (strcmp(word, "<") == 0) {
					words[++word_count] = strtok(NULL, " ");
					input_file = words[word_count];
					redir_input = true;
				}
				
				// set output redirection mode and filename
				else if (strcmp(word, ">") == 0) {
					words[++word_count] = strtok(NULL, " ");
					output_file = words[word_count];
					redir_output = true;
				}

				// set BG mode, stop reading rest of line (must be last argument)
				else if (strcmp(word, "&") == 0) {
					background_mode = true;
					break;
				}
				
				// all others are arguments
				else {
					arguments[arg_count++] = words[word_count++];
				}
			}

			// add NULL entry to end of arguments array
			arguments[arg_count] = NULL;
			
			// check if user enters exit
			// if true, terminate main shell process
			if (strcmp(command, "exit") == 0) {
				free(arguments);
				term_shell();
			}

			// run cd command and change to directory provided
			// if no directory/path provided, change to HOME env variable
			else if (strcmp(command, "cd") == 0) {
				// if only argument is 'cd', change to HOME path
				if (arg_count == 1) {
					change_dir(getenv("HOME"));
				}

				// if argument contains path, change to that directory
				else if (arg_count == 2) {
					change_dir(arguments[1]);
				}

				// else more arguments provided, print usage message
				else {
					printf("smallsh: cd: usage: cd [directory]\n");
				}
			}

			// outputs exit status of last foreground process that finished
			else if (strcmp(command, "status") == 0) {
				printf("exit value %d\n", foreground_exit_stat);
			}

			// this branch forks processes into their own child processes
			else {
				// fork process, then set up in/out redirection as appropriate
				pid_t pid = fork(); // parent process gets pid of child assigned, child gets 0
				int fd_in, fd_out, fd_in2, fd_out2;

				// execute process if child process
				if (pid == 0) {
					
					if (!redir_input && background_mode)
						input_file = "/dev/null";
					if (!redir_output && background_mode)
						output_file = "/dev/null";

					// Open input_file if not null 
					// if it's null, then some file was provided or it's foreground
					if (input_file) {
						fd_in = open(input_file, O_RDONLY);
						if (fd_in == -1) {
							perror("open");
							exit(1);
						}
						fd_in2 = dup2(fd_in, 0); // 0 = stdin
						if (fd_in2 == -1) {
							perror("dup2");
							exit(2);
						}
					}

					// Open output_file if not null 
					// if null, then some file was provided or it's foreground
					if (output_file) {
						fd_out = open(output_file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
						if (fd_out == -1) {
							perror("open");
							exit(1);
						}

						fd_out2 = dup2(fd_out, 1); // 1 = stdout
						if (fd_out2 == -1) {
							perror("dup2");
							exit(2);
						}					
					}

					// Intercept SIGINT signal if process is not in background
					if (!background_mode) {
						act.sa_flags = 0;
						act.sa_handler = SIG_DFL;
						sigaction(SIGINT, &act, NULL);
					}

					foreground_exit_stat = execvp(command, arguments);

					// check if status denotes an error
					// if error, output error to user
					if(foreground_exit_stat == -1) {
						perror(command);
						foreground_exit_stat = 1;
					}
					exit(EXIT_FAILURE); 
				} // end child process
				
				// fork failed
				else if (pid == -1) {
					perror("fork()");
				}

				// main process branch
				else {
					// checks if process was placed into background
					// if true, track pid but don't wait
					if (background_mode) {
						append_bg_pid(pid);
						printf("background pid is %d\n", pid);
					}
					else {
						// wait until process finishes or is terminated
	               		pid = waitpid(pid, &foreground_exit_stat, 0);

						if (pid != -1 && pid != 0) {
							if (WIFSIGNALED(foreground_exit_stat)) {
								foreground_exit_stat = WTERMSIG(foreground_exit_stat);
								printf("pid %d is done: terminated by signal %d\n", pid, foreground_exit_stat);
							}
							else if(WIFEXITED(foreground_exit_stat)) {
								foreground_exit_stat = WEXITSTATUS(foreground_exit_stat);
							}
						}
			        }
				}
			}
		}
		// deallocate memory assigned to arguments array
		if(arguments) free(arguments);	
	}
}

/**
 initializes bg_process_arr global and starts main_shell process
 @argc - number of command-line arguments passed to smallsh execution
 @argv - array of char arrays representing arguments passed to smallsh
**/
int main (int argc, char const *argv[])
{
	// initialize bg_process_arr global to track background process pids
	init_bg_process_arr();

	main_shell();
	return 0;
}
