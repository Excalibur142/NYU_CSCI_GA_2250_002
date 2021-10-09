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

//make variadic to close as many pointers as possible
void close_shell(char* CloseVal) {
	free(CloseVal);
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

void make_dir(char **directory) {
	if (directory == NULL)
		return;
	char* DirName = get_current_dir_name();
	//Check if DirName is empty, if so return error
	if (DirName == NULL) {
		perror("get_current_dir_name");
		return;
	}
	int DirLength = get_length(DirName);
	for (int i = (DirLength - 1); i >= 0; i--) {
		if (DirName[i] == '/') {
			int MemToAllocate = DirLength - i + 15;
			*directory = (char *)realloc(*directory, MemToAllocate);
			if (*directory == NULL) {
				perror("realloc failed");
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
//Redirect Functions
void redirect_output();
void redirect_input();
//Pipe function
void pipe_cmd();
//Run command and arg
void run_command(char** ParsedLine, int Args);

char invalid_arg[] = {'>','<','|','*','!', '`', '\'','\"'};

//Will go through a list of commands with the structure of command and arg
//Do something with the given commands
void examine_command(char** ParsedLine, int Args, char* Prompt) {
	//Handle 4 simple commands
	if (!strcmp(ParsedLine[0], "cd\n") || !strcmp(ParsedLine[0], "cd")) {
		if (Args == 1 || Args > 2) {
			printf("%sError: Invalid command\n", Prompt);
			return;
		}
		if (chdir(ParsedLine[1]) == -1) {
			printf("%sError: Invalid directory\n", Prompt);
			return;
		}
		return;
	}
	if (!strcmp(ParsedLine[0], "jobs\n") || !strcmp(ParsedLine[0], "jobs")) {
		if (Args > 1) {
			printf("%sError: Invalid command\n", Prompt);
			return;
		}
	}
	if (!strcmp(ParsedLine[0], "fg\n") || !strcmp(ParsedLine[0], "fg")) {
		if (Args == 1 || Args > 2) {
			printf("%sError: Invalid command\n", Prompt);
			return;
		}
	}
	if (!strcmp(ParsedLine[0], "exit\n") || !strcmp(ParsedLine[0], "exit")) {
		//Handle suspended jobs
		if (Args > 1) {
			printf("%sError: Invalid command\n", Prompt);
			return;
		}
		exit(0);
	}

	//Check the command and see if there are any <, or |, or <, or <<. If none, execute the command and all arguments
	//Create a flag every time a pipe has been seen and the previous pipe to create a parsed char** of cmd arg between pipes
	int Flag = 0;
	_Bool ChangeFlag = true;
	for (int i = 0; i < Args; ++i) {
		
		//Checks for < > or |
		if (strstr(ParsedLine[i], "<") || strstr(ParsedLine[i], "|")) {
			//Set flag as new starting point for handling pipe and <
			//Handle redirecting input
			if (strstr(ParsedLine[i], "<")) { //****************************************************************** FIX MULTIPLE INPUT REDIRECTS, CURRENTLY DOES ALL? PLUS COMMANDS IN BETWEEN?
				ParsedLine[i] = NULL;
				int pid = fork();
				if (pid == 0) {
					int fd0 = open(ParsedLine[i + 1], O_RDONLY);
					if (fd0 == -1) {
						printf("%sError: Cant open file in input direct\n", Prompt);
						exit(-1);
					}
					fchmod(fd0, 0600);
					dup2(fd0, STDIN_FILENO);
					close(fd0);
					if (execvp(ParsedLine[0], ParsedLine) == -1) {
						printf("%sError: Invalid command\n", Prompt);
						exit(-1);
					}
				}
				else {
					wait(NULL);
				}
			
			}
			//Handle Piping
			else if (strstr(ParsedLine[i], "|")) { //************************************************FIX TO ONLY RUN 2ND PIPE UP TO 2ND COMMAND
				ParsedLine[i] = NULL;
				int pipefd[2];
				int status =0;
				pid_t p1, p2;
				if (pipe(pipefd) == -1) {
					printf("%sPipe Failed", Prompt);
					return;
				}
				p1 = fork();
				if (p1 == 0) {
					dup2(pipefd[1], 1);
					close(pipefd[0]);
					execvp(ParsedLine[Flag], ParsedLine+Flag);
					exit(-1);
				}
				p2 = fork();
				if (p2 == 0) {
					dup2(pipefd[0], 0);
					close(pipefd[1]);
					execvp(ParsedLine[i + 1], ParsedLine + i + 1);
					exit(-1);
				}
				else {
					wait(NULL);
				}
							
			}
			
			if (ChangeFlag == true) {
				Flag = i+1;
				ChangeFlag = false;
			}
			else
				ChangeFlag = false;
			//Continue because i at this point is null -> go to next value to check
			continue;
		}

		//Handle output redirect
		if (strstr(ParsedLine[i], ">")) {
			ParsedLine[i] = NULL;
			int pid = fork();
			if (pid == 0) {
				//In child
				int fd0;
				if ((fd0 = open(ParsedLine[i + 1], O_WRONLY | O_CREAT | O_TRUNC)) == -1) {
					printf("%sError: Cant open file in redirect\n", Prompt);
					exit(-1);
				}
				fchmod(fd0, 0600);
				dup2(fd0, STDOUT_FILENO);
				close(fd0);
				if (execvp(ParsedLine[0], ParsedLine) == -1) {
					printf("%sError: Invalid command\n", Prompt);
					exit(-1);
				}
			}
			else {
				//parent waits for redirect to complete
				wait(NULL);
			}

			return;
		}
	}

	if (Flag > 0)
		return;


	//Run a simple command
	//Check if command or argument contain any invalid arguments
	for (int i = 0; i < Args; ++i) {
		for (int j = 0; j < 8; ++j) {
			if (*ParsedLine[i] == invalid_arg[j]) {
				printf("%sError: Invalid command\n", Prompt);
				return;
			}
		}
	}
	//Otherwise execute command
	int ChildID = fork();
	if(ChildID == 0) {
		//First Check Absolute path
		if (ParsedLine[0][0] == '/') {
			if(execv(ParsedLine[0], ParsedLine) == -1)
				printf("%sError: Invalid Program or relative path\n", Prompt);
			exit(0);
		}
		//Then check Relative path
		if (ParsedLine[0][0] == '.' && ParsedLine[0][1] == '/') {
			char PathRel[] = "./";
			strcat(PathRel, ParsedLine[0]);
			if (execv(PathRel, ParsedLine) == -1)
				printf("%sError: Invalid Program\n", Prompt);
			exit(0);
		}
		//Then check /bin and usr/bin
		char PathBin[] ="/bin/";
		strcat(PathBin, ParsedLine[0]);
		if (execv(PathBin, ParsedLine) == -1) {
			char PathUsr[] = "/usr/bin/";
			strcat(PathUsr, ParsedLine[0]);
			if (execv(PathUsr, ParsedLine) == -1) 
				printf("%sError: Invalid Program\n", Prompt);
		}
		exit(0);
	}
	wait(NULL);
}

//Get and parse command line
//Each command is seperated by space
char** parse_command(char* command, int CountOfSpaces) {
	char** ParsedCommand = malloc((CountOfSpaces+1) * sizeof(char*));
	char* SingleCommand = strtok(command, " ");
	int i = 0;
	while (SingleCommand != NULL) {
		ParsedCommand[i] = calloc((get_length(SingleCommand)), sizeof(char*));
		for (int j = 0; j < get_length(SingleCommand); ++j) {
			ParsedCommand[i][j] = SingleCommand[j];
		}
		++i;
		SingleCommand = strtok(NULL, " ");
	}
	ParsedCommand[CountOfSpaces] = '\0';
	if (CountOfSpaces == 1) {
		ParsedCommand[0][get_length(ParsedCommand[0])-1] = '\0';
	}else
		ParsedCommand[CountOfSpaces-1][get_length(ParsedCommand[CountOfSpaces-1])-1] = '\0';

	return ParsedCommand;
}
void free_command(char** command, int CountOfSpaces) {
	for (int i = 0; i < CountOfSpaces; ++i)
		free(command[i]);
	free(command);
}


int main(int argc, const char* const* argv) {
	//Create variable to store current directory in
	//This way only get_dir() needs to be called if directory changes
	char* CmdPrompt = calloc(10, sizeof(char*));
	if (CmdPrompt == NULL) {
		perror("calloc failed");
		exit(1);
	}
	char Command[1001];
	//Create a while loop that waits for exit and exits on command
	while (1) {
		make_dir(&CmdPrompt);
		if (CmdPrompt != NULL)
			printf("%s", CmdPrompt);
		fgets(Command, 1001, stdin);
		//Check command length and prompt for new one
		if (strlen(Command) > 999) {
			printf("%sError: Your command was over 1000 characters\n",CmdPrompt);
			fflush(stdin);
			continue;
		}
		//Get count of arguments
		int CountOfArgs = 0;
		for (int i = 0; i < get_length(Command); ++i) {
			if (Command[i] == ' ')
				CountOfArgs++;
		}
		//Accounts for last space, or if there is only 1
		CountOfArgs++;

		char** cmdline = parse_command(Command, CountOfArgs);
		examine_command(cmdline, CountOfArgs, CmdPrompt);
		

		//Free the cmdline
		free_command(cmdline, CountOfArgs);
	}

	close_shell(CmdPrompt);
}
