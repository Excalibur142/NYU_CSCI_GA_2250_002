#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <signal.h>

//make variadic to close as many pointers as possible
void close_shell(char* CloseVal) {
	free(CloseVal);
}

void free_command(char** command, int CountOfSpaces) {
	for (int i = 0; i < CountOfSpaces; ++i)
		free(command[i]);
	free(command);
}
//Gets length of array
int get_length(char* ToCount) {
	if (ToCount == NULL)
		return 0;
	int i = 0;
	while (ToCount[i] != '\0') {
		i++;
	}
	return i;
}
//Call when changes to directory are made!
//Otherwise use global directory value

void make_dir(char** directory) {
	if (directory == NULL)
		return;
	char* DirName = get_current_dir_name();
	//Check if DirName is empty, if so return error
	if (DirName == NULL) {
		fprintf(stderr, "Error: invalid directory\n");
		return;
	}
	int DirLength = get_length(DirName);
	for (int i = (DirLength - 1); i >= 0; i--) {
		if (DirName[i] == '/') {
			int MemToAllocate = DirLength - i + 15;
			*directory = (char*)realloc(*directory, MemToAllocate);
			if (*directory == NULL) {
				fprintf(stderr, "Error: realloc failed\n");
				exit(1);
			}
			memset(&*directory[0], 0, sizeof(directory));
			//Copy Dir
			strncpy(*directory, "[nyush \0", 7);
			strcat(*directory, DirName + i + 1);
			char end[] = "]$ ";
			strcat(*directory, end);
			break;
		}
	}
	free(DirName);
}

struct process {
	int processID;
	char* ProcessCommand;
};
struct process ProcessList[100];
//make a pid list that has no more than 100 processes
int pidList[100][1];
char pidCommand[100][1000];
//Signal Handling
void sig_handler(int temp) {
	//IMPLEMENT FOR ALL PIDS NOT NULL
	for (int i = 0; i < 100; ++i) {
		if (pidList[i][0] > 0) {
			fprintf(stderr, "%i Process Killed\n", pidList[0][0]);
			kill(pidList[i][0], SIGKILL);
			kill(getpid(), SIGCONT);
		}
		else
			break;
	}
	kill(getpid(), SIGCONT);

}
void sig_stop(int temp) {
	//IMPLEMENT FOR ALL PIDS NOT NULL
	for (int i = 0; i < 100; ++i) {
		if (pidList[i][0]){
			fprintf(stderr, "%i Process Paused\n", pidList[0][0]);
			//Add process id and commmand to processlist struct
				//Get next available slot
			for (int j = 0; j < 100; ++j) {
				if (!ProcessList[j].processID) {
					ProcessList[j].processID = pidList[i][0];
					if (ProcessList[j].ProcessCommand == NULL)
						ProcessList[j].ProcessCommand = calloc(1000, sizeof(char));
					for (int k = 0; k < 1000; k++) {
						if (pidCommand[i][k] == '\n') {
							ProcessList[j].ProcessCommand[k] = '\0';
							break; //Break after last command is entered
						}
						ProcessList[j].ProcessCommand[k] = pidCommand[i][k];
					}
					break; //Break after first available process id struct is found
				}
			}
			kill(pidList[i][0], SIGTSTP);
			kill(getpid(), SIGCONT);
		}
		else
			break;
	}
}
//Will go through a list of commands with the structure of command and arg
void examine_command(char** ParsedLine, int Args, char* Prompt, _Bool* Cont) {
	//Handle 4 simple commands
	if (!strcmp(ParsedLine[0], "cd\n") || !strcmp(ParsedLine[0], "cd")) {
		*Cont = true;
		if (Args == 1 || Args > 2) {
			fprintf(stderr, "Error: invalid command\n");
			return;
		}
		if (Args == 2) {
			if (!strcmp(ParsedLine[1], "")) {
				fprintf(stderr, "Error: invalid command\n");
				return;
			}
		}
		if (chdir(ParsedLine[1]) == -1) {
			fprintf(stderr, "Error: invalid directory\n");
			return;
		}
		return;
	}
	if (!strcmp(ParsedLine[0], "jobs\n") || !strcmp(ParsedLine[0], "jobs")) {
		*Cont = true;
		if (Args > 1) {
			fprintf(stderr, "Error: invalid command\n");
			return;
		}
		for (int i = 0; i < 100; ++i) {
			if (ProcessList[i].processID) {
				printf("[%i] [%s]\n", i+1, ProcessList[i].ProcessCommand);
			}
			else
				break;
		}
	}
	if (!strcmp(ParsedLine[0], "fg\n") || !strcmp(ParsedLine[0], "fg")) {
		*Cont = true;
		if (Args == 1 || Args > 2) {
			fprintf(stderr, "Error: invalid command\n");
			return;
		}
		int id = *ParsedLine[1]-49;
		if (!ProcessList[id].processID) {
			fprintf(stderr, "Error: invalid job\n");
			return;
		}
		kill(ProcessList[id].processID, SIGCONT);
		kill(getpid(), SIGTSTP);
	}
	if (!strcmp(ParsedLine[0], "exit\n") || !strcmp(ParsedLine[0], "exit")) {
		//Handle suspended jobs
		*Cont = true;
		if (Args > 1) {
			fprintf(stderr, "Error: invalid command\n");
			return;
		}
		free_command(ParsedLine, Args);
		for (int i = 0; i < 100; ++i) {
			if (ProcessList[i].processID) {
				free(ProcessList[i].ProcessCommand);
			}
			else
				break;
		}
		close_shell(Prompt);
		exit(0);
	}
}

char invalid_arg[] = { '>','<','|','*','!', '`', '\'','\"' };

//Run command and arg
//Do something with the given commands
void run_command(char** ParsedLine, int Flag, _Bool* ShouldContinue) {
	//Run a simple command
	
	//Otherwise execute command
	//First Check Absolute path
	if (ParsedLine[Flag][0] == '/') {
		if (execv(ParsedLine[Flag], ParsedLine + Flag) == -1)
			fprintf(stderr, "Error: invalid program\n");
		exit(-1);
	}
	//Then check Relative path
	if (ParsedLine[0][0] == '.' && ParsedLine[0][1] == '/') {
		char PathRel[] = "./";
		strcat(PathRel, ParsedLine[Flag]);
		if (execv(PathRel, ParsedLine + Flag) == -1)
			fprintf(stderr, "Error: invalid program\n");
		exit(-1);
	}
	//Then check /bin and usr/bin
	char PathBin[] = "/bin/";
	strcat(PathBin, ParsedLine[Flag]);
	if (execv(PathBin, ParsedLine + Flag) == -1) {
		char PathUsr[] = "/usr/bin/";
		strcat(PathUsr, ParsedLine[Flag]);
		if (execv(PathUsr, ParsedLine + Flag) == -1)
			fprintf(stderr, "Error: invalid program\n");
	}
	exit(-1);
}

//Get and parse command line
//Each command is seperated by space
char** parse_command(char* command, int* CountOfSpaces) {
	char** ParsedCommand = malloc((*CountOfSpaces + 1) * sizeof(char*));
	char* SingleCommand = strtok(command, " ");
	int i = 0;
	while (SingleCommand != NULL) {
		ParsedCommand[i] = calloc((get_length(SingleCommand)), sizeof(char*));
		for (int j = 0; j < get_length(SingleCommand); ++j) {
			ParsedCommand[i][j] = SingleCommand[j]; //Possible null terminate each command?
		}
		++i;
		SingleCommand = strtok(NULL, " ");
	}
	*CountOfSpaces = i;
	ParsedCommand[*CountOfSpaces] = '\0';
	if (*CountOfSpaces == 1) {
		ParsedCommand[0][get_length(ParsedCommand[0]) - 1] = '\0';
	}
	else
		ParsedCommand[*CountOfSpaces - 1][get_length(ParsedCommand[*CountOfSpaces - 1]) - 1] = '\0';

	return ParsedCommand;
}
//Add processes to a list and update that list
//List will keep pid, and process name
void add_process(pid_t id, char** Command) {

}
int main(int argc, const char* const* argv) {
	int gFlag = 0;
	signal(SIGINT, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGTSTP, sig_stop);
	//Create variable to store current directory in
	char* CmdPrompt = calloc(10, sizeof(char*));
	if (CmdPrompt == NULL) {
		fprintf(stderr, "Error: calloc failed\n");
		exit(1);
	}
	char Command[1001];
	//Create array of ints that can store pid
	char** StoppedProcess;
	//Create a while loop that waits for exit and exits on command
	while (1) {
		//Clear pidlist array
		for (int i = 0; i < 100; ++i) {
			pidList[i][0] = 0;
		}
		make_dir(&CmdPrompt);
		if (CmdPrompt != NULL)
			printf("%s", CmdPrompt);

		fflush(stdout);
		if (fgets(Command, 1000, stdin) == NULL) {
			printf("\n");
			exit(0);
		}

		if (Command[strlen(Command - 1)] == '\n' || Command[strlen(Command - 1)] == '\0') {
			//Ignore and prompt again
			continue;
		}
		//Check command length and prompt for new one
		if (strlen(Command) > 1000) {
			printf("%sError: Your command was over 1000 characters\n", CmdPrompt);
			fflush(stdin);
			continue;
		}
		//Get count of arguments
		int CountOfArgs = 0;
		int CountOfPipes = 0;
		for (int i = 0; i < get_length(Command); ++i) {
			if (Command[i] == ' ')
				CountOfArgs++;
			if (Command[i] == '|')
				CountOfPipes++;
		}
		//Accounts for last space, or if there is only 1
		CountOfArgs++;
		_Bool ShouldContinue = false;
		char** cmdline = parse_command(Command, &CountOfArgs);

		examine_command(cmdline, CountOfArgs, CmdPrompt, &ShouldContinue);
		//Go through entered command and check for pipes and redirects

		//Check for error in command
		int CountOfInputR;
		for (int i = 0; i < CountOfArgs; ++i) {
			if (!strcmp(cmdline[i], "<")) { //will catch first input redirect
				if (cmdline[i + 2] != NULL) {
					if (strcmp(cmdline[i + 2], "|") != 0 && strstr(cmdline[i + 2], ">") == 0) {
						fprintf(stderr, "Error: invalid command\n");
						ShouldContinue = true;
						break;
					}
				}
			}
			if (strstr(cmdline[i], "<<")) {
				ShouldContinue = true;
				fprintf(stderr, "Error: invalid command\n");
				break;
			}
			if (!strcmp(cmdline[i], "|")) {
				if (i == 0) {
					fprintf(stderr, "Error: invalid command\n");
					ShouldContinue = true;
					break;
				}
				if (cmdline[i - 1] != NULL) {
					if (!strcmp(cmdline[i - 1], "")) {
						fprintf(stderr, "Error: invalid command\n");
						ShouldContinue = true;
						break;
					}
				}
				if (cmdline[i + 1] != NULL) {
					if (!strcmp(cmdline[i + 1], "")) {
						fprintf(stderr, "Error: invalid command\n");
						ShouldContinue = true;
						break;
					}
				}
			}
		}
		if (ShouldContinue == true) {
			continue;
		}
		//Find pipes and store index of pipe
		int** indexOfPipes = calloc(CountOfPipes, sizeof(int));
		int k = 0;
		for (int i = 0; i < CountOfArgs; ++i) {
			if (!strcmp(cmdline[i], "|")) {
				indexOfPipes[k] = calloc(4, sizeof(int));
				*indexOfPipes[k] = i;
				//Set the pipe to null now
				cmdline[i] = NULL;
				k++;
			}
		}
		int Flag = 0; //Will be used to move command along the 2d array
		

		//Create a number of pipes
		int pipes[CountOfPipes][2];
		int i;
		for (i = 0; i < CountOfPipes; ++i) {
			pipe(pipes[i]);
		}
		//If no pipes, run command normally, if pipes, fork multiple times
		for (i = 0; i < CountOfPipes + 1; ++i) {
			if (ShouldContinue == true)
				break;
			int pid = fork();
			pidList[i][0] = pid;
			for (int j = 0; j < get_length(Command); ++j) {
				pidCommand[i][j] = Command[j];
			}
			
			if (pid == 0) {
				//Child occurs whether there is a pipe or not!
				if (i == 0) {
					dup2(pipes[i][1], STDOUT_FILENO);
					if (CountOfPipes > 0) { // If command is first and there are multiple pipes present, check for redirect
						for (int k = 0; k < *indexOfPipes[0]; ++k) {//Shouldnt have to worry about the pipe being null because it only searches up until the first pipe. Any other < is incorrect
							if (!strcmp(cmdline[k], "<")) {
								cmdline[k] = NULL;
								int fd0 = open(cmdline[k + 1], O_RDONLY);
								if (fd0 == -1) {
									fprintf(stderr, "Error: invalid file\n");
									ShouldContinue = true;
								}
								dup2(fd0, STDIN_FILENO);
								close(fd0);
							}
						}
						cmdline[*indexOfPipes[0]] = NULL;
					}
					else if (CountOfPipes == 0) {// Again, shouldnt have to worry about any | being null because there are none
						for (int k = 0; k < CountOfArgs; ++k) {
							if (!strcmp(cmdline[k], "<")) {
								cmdline[k] = NULL;
								int fd0 = open(cmdline[k + 1], O_RDONLY);
								if (fd0 == -1) {
									fprintf(stderr, "Error: invalid file\n");
									ShouldContinue = true;
								}
								dup2(fd0, STDIN_FILENO);
								close(fd0);
							}
						}
						for (int i = 0; i < CountOfArgs; ++i) {// Again, shouldnt have to worry about any | being null because there are none
							if (cmdline[i] == NULL)
								continue;
							if (strstr(cmdline[i], ">")) { //will catch final out
								if (cmdline[i + 1] != NULL) {
									int fd1;
									if (strstr(cmdline[i], ">>")) { // Append
										fd1 = open(cmdline[i + 1], O_WRONLY | O_APPEND, 0600);
									}
									else {
										fd1 = open(cmdline[i+1], O_WRONLY | O_TRUNC | O_CREAT, 0600);
									}
									cmdline[i] = NULL;
									if (fd1 == -1) {
										ShouldContinue = true;
										fprintf(stderr, "Error: invalid file\n");
										return -1;
									}
									dup2(fd1, STDOUT_FILENO);
									close(fd1);			
								}
							}
						}
					}
				}
				else if (i == CountOfPipes) {
					dup2(pipes[i - 1][0], STDIN_FILENO);
					Flag = *indexOfPipes[i - 1] + 1; //Take the last index of pipe for the second argument
					cmdline[*indexOfPipes[i-1]] = NULL;
					for (int k=i; k < CountOfArgs-i+1; ++k) {// Check for output redirect if it is the final command at end of pipe
						if (cmdline[k] == NULL)
							continue;
						if (strstr(cmdline[k], ">")) { //will catch final out
							if (cmdline[k + 1] != NULL) {
								int fd0;
								if (strstr(cmdline[k], ">>")) { //append
									fd0 = open(cmdline[k + 1], O_WRONLY | O_APPEND, 0600);
								}
								else {
									fd0 = open(cmdline[k + 1], O_WRONLY | O_TRUNC | O_CREAT, 0600);
								}
								cmdline[k] = NULL;
								if (fd0 == -1) {
									ShouldContinue = true;
									fprintf(stderr, "Error: invalid file\n");
									return -1;
								}
								dup2(fd0, STDOUT_FILENO);
								close(fd0);
							}
						}
					}
				}
				else {
					dup2(pipes[i - 1][0], STDIN_FILENO);
					dup2(pipes[i][1], STDOUT_FILENO);
					Flag = *indexOfPipes[i - 1] + 1; //Take the first index of pipe for the second argument
					cmdline[*indexOfPipes[i - 1]] = NULL;
				}
				int j;
				for (j = 0; j < CountOfPipes; ++j) {
					close(pipes[j][0]);
					close(pipes[j][1]);
				}
				if (ShouldContinue == true)
					exit(-1);
				run_command(cmdline, Flag, &ShouldContinue);
				//exit on failure handled in run command
			}
		}

		for (i = 0; i < CountOfPipes; ++i) {
			close(pipes[i][0]);
			close(pipes[i][1]);
		}
		
		for (int i = 0; i < CountOfPipes + 1; ++i) {
			//wait(NULL);
			waitpid(pidList[i][0], NULL, WUNTRACED);
		}


		//Free the cmdline
		free_command(cmdline, CountOfArgs);
		//Free index of pipes counter
		for (int i = 0; i < CountOfPipes; ++i)
			free(indexOfPipes[i]);
		free(indexOfPipes);

	}

	//Should never get here because exit will take care of it
	printf("\n");
	close_shell(CmdPrompt);
}
