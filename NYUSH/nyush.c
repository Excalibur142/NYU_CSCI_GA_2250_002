#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>


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

//Will go through a list of commands with the structure of command and arg
//Do something with the given commands
void examine_command(char** ParsedLine, int Args, char* Prompt) {
	//Handle 4 simple commands
	if (!strcmp(ParsedLine[0], "cd\n") || !strcmp(ParsedLine[0], "cd")) {
		if (Args == 1 || Args > 2) {
			printf("%sError: Invalid command\n", Prompt);
			return;
		}
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
	//Checks to see if there are more than 2 arguments
	if (Args >= 2) {
		if (ParsedLine[1][0] == '<') {
			if (Args < 3) {
				printf("%sError: No Input File\n", Prompt);
				return;
			}
			
		}
	}
	
}

//Get and parse command line
//Each command is seperated by space
char** parse_command(char* command, int CountOfSpaces) {
	//Accounts for last space, or if there is only 1
	CountOfSpaces++;
	char** ParsedCommand = malloc((CountOfSpaces) * sizeof(char*));
	char* SingleCommand = strtok(command, " ");
	int i = 0;
	while (SingleCommand != NULL) {
		ParsedCommand[i] = calloc((get_length(SingleCommand) + 1), sizeof(char*));
		for (int j = 0; j < get_length(SingleCommand); ++j) {
			ParsedCommand[i][j] = SingleCommand[j];
		}
		++i;
		SingleCommand = strtok(NULL, " ");
	}
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
	make_dir(&CmdPrompt);
	char Command[1001];
	//Create a while loop that waits for exit and exits on command
	while (1) {
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
