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
	printf("\n");
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

void make_dir(char **directory) {
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
			*directory = (char *)realloc(*directory, MemToAllocate);
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

//Redirect Functions
int redirect_output(char* FileToRedirect, _Bool ShouldAppend) {
	int fd1;
	if (ShouldAppend) {
		fd1 = open(FileToRedirect, O_WRONLY | O_APPEND, 0600);
	}
	else {
		fd1 = open(FileToRedirect, O_WRONLY | O_TRUNC | O_CREAT, 0600);
	}
	//If file cant be opened or created, throw
	if (fd1 == -1) {
		fprintf(stderr, "Error: invalid file\n");
		return -1;
	}
	if (dup2(fd1, 1) == -1) {
		fprintf(stderr, "Error: invalid file\n");
		return -1;
	}
	if (close(fd1) == -1) {
		fprintf(stderr, "Error: invalid file\n");
		return-1;
	}
	return 0;
}
int redirect_input(char* FileToRedirect) {
	int fd0 = open(FileToRedirect, O_RDONLY);
	//If file doesnt exist or cant be opened, throw
	if (fd0 == -1) {
		fprintf(stderr, "Error: invalid file\n");
		return -1;
	}
	if(dup2(fd0, 0) == -1) {
		fprintf(stderr, "Error: invalid file\n");
		return -1;
	}
	if (close(fd0) == -1) {
		fprintf(stderr, "Error: invalid file\n");
		return -1;
	}
	return 0;
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
	}
	if (!strcmp(ParsedLine[0], "fg\n") || !strcmp(ParsedLine[0], "fg")) {
		*Cont = true;
		if (Args == 1 || Args > 2) {
			fprintf(stderr, "Error: invalid command\n");
			return;
		}
	}
	if (!strcmp(ParsedLine[0], "exit\n") || !strcmp(ParsedLine[0], "exit")) {
		//Handle suspended jobs
		*Cont = true;
		if (Args > 1) {
			fprintf(stderr, "Error: invalid command\n");
			return;
		}
		free_command(ParsedLine, Args);
		close_shell(Prompt);
		exit(0);
	}
}
char invalid_arg[] = {'>','<','|','*','!', '`', '\'','\"'};

//Run command and arg
//Do something with the given commands
void run_command(char** ParsedLine, char* Prompt, int Flag){
	//Run a simple command
	////Check if command or argument contain any invalid arguments
	//for (int i = 0; i < Args; ++i) {
	//	for (int j = 0; j < 8; ++j) {
	//		if (*ParsedLine[i] == invalid_arg[j]) {
	//			printf("%sError: Invalid command\n", Prompt);
	//			return;
	//		}
	//	}
	//}
	//Otherwise execute command
	int ChildID = fork();
	if(ChildID == 0) {
		//First Check Absolute path
		if (ParsedLine[Flag][0] == '/') {
			if(execv(ParsedLine[Flag], ParsedLine+Flag) == -1)
				fprintf(stderr, "Error: invalid program\n");
			exit(0);
		}
		//Then check Relative path
		if (ParsedLine[0][0] == '.' && ParsedLine[0][1] == '/') {
			char PathRel[] = "./";
			strcat(PathRel, ParsedLine[Flag]);
			if (execv(PathRel, ParsedLine+Flag) == -1)
				fprintf(stderr, "Error: invalid program\n");
			exit(0);
		}
		//Then check /bin and usr/bin
		char PathBin[] ="/bin/";
		strcat(PathBin, ParsedLine[Flag]);
		if (execv(PathBin, ParsedLine+Flag) == -1) {
			char PathUsr[] = "/usr/bin/";
			strcat(PathUsr, ParsedLine[Flag]);
			if (execv(PathUsr, ParsedLine+Flag) == -1) 
				fprintf(stderr, "Error: invalid program\n");
		}
		exit(0);
	}
	else {
		waitpid(ChildID, NULL, 0);
		//REFERENCE: https://stackoverflow.com/questions/8514735/what-is-special-about-dev-tty Explanation of /dev/tty and why its needed to give control back to terminal
		redirect_input("/dev/tty");
		redirect_output("/dev/tty", false);
	}
}
//Pipe function
void pipe_cmd(char** ParsedLine, char* Prompt, int Flag) {
	int pipe_fd[2];
	if (pipe(pipe_fd) == -1) {
		fprintf(stderr, "Error: Cant create pip\n");
		return;
	}
	if (dup2(pipe_fd[1], 1) == -1) {
		fprintf(stderr, "Error: cant dup\n");
		return;
	}
	if (close(pipe_fd[1]) == -1) {
		fprintf(stderr, "Error: Cant close pipe\n");
		return;
	}
	//Run the first command, the next command will either get pipe'd or ran as the final command
	run_command(ParsedLine, Prompt, Flag);
	if (dup2(pipe_fd[0], 0) == -1) {
		fprintf(stderr, "Error: cant dup\n");
		return;
	}
	if (close(pipe_fd[0]) == -1) {
		fprintf(stderr, "Error: cant close pipe\n");
		return;
	}
}


//Get and parse command line
//Each command is seperated by space
char** parse_command(char* command, int* CountOfSpaces) {
	char** ParsedCommand = malloc((*CountOfSpaces+1) * sizeof(char*));
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
		ParsedCommand[0][get_length(ParsedCommand[0])-1] = '\0';
	}else
		ParsedCommand[*CountOfSpaces-1][get_length(ParsedCommand[*CountOfSpaces-1])-1] = '\0';

	return ParsedCommand;
}
//Signal Handling
void sig_handler(int temp) {
	
}
//Add processes to a list and update that list
//List will keep pid, and process name
void add_process(pid_t id, char** Command) {

}
int main(int argc, const char* const* argv) {
	int gFlag = 0;
	//signal(SIGINT, sig_handler);
	//signal(SIGQUIT, sig_handler);
	//signal(SIGTERM, sig_handler);
	//signal(SIGTSTP, sig_handler);
	//Create variable to store current directory in
	//This way only get_dir() needs to be called if directory changes
	char* CmdPrompt = calloc(10, sizeof(char*));
	if (CmdPrompt == NULL) {
		fprintf(stderr, "Error: calloc failed\n");
		exit(1);
	}
	char Command[1001];
	//Create a while loop that waits for exit and exits on command

	while (1) {
		if (!isatty(STDIN_FILENO)) {
			gFlag = 1;
		}

		make_dir(&CmdPrompt);
		if (CmdPrompt != NULL)
			printf("%s", CmdPrompt);

		if (fgets(Command, 1000, stdin) == NULL) {
			printf("\n");
			exit(0);
			fflush(stdout);
		}
		fflush(stdout);

		if (Command[strlen(Command-1)] == '\n' || Command[strlen(Command - 1)] == '\0') {
			//Ignore and prompt again
			continue;
		}
		//Check command length and prompt for new one
		if (strlen(Command) > 1000) {
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
		_Bool ShouldContinue = false;
		char** cmdline = parse_command(Command, &CountOfArgs);
		
		examine_command(cmdline, CountOfArgs, CmdPrompt, &ShouldContinue);
		//Go through entered command and check for pipes and redirects
		int Flag = 0;
		_Bool ChangeFlag = true;
		for (int i = 0; i < CountOfArgs; ++i) {
			if (strstr(cmdline[i], "<")) {
				if (redirect_input(cmdline[i + 1]) == -1) {
					ShouldContinue = true;
					break;
				}
				cmdline[i] = NULL;
				pipe_cmd(cmdline, CmdPrompt, Flag);
			}
			else if (strstr(cmdline[i], ">")) {
				if (strstr(cmdline[i], ">>")) {
					if (redirect_output(cmdline[i + 1], true) == -1) {
						ShouldContinue = true;
						break;
					}
				}
				else if (redirect_output(cmdline[i + 1], false) == -1) {
					ShouldContinue = true;
					break;
				}
				cmdline[i] = NULL;
			}
			else if (strstr(cmdline[i], "|")) {
				//Found pipe, set flag 1 to location after operation so next command executes after pipe
				cmdline[i] = NULL;
				pipe_cmd(cmdline, CmdPrompt,Flag);
				Flag = i + 1;
			}
		}
		if (ShouldContinue == true) {
			ShouldContinue = false;
			continue;
		}
		run_command(cmdline, CmdPrompt,Flag);


		//Free the cmdline
		free_command(cmdline, CountOfArgs);


		if (gFlag = 1) {
			//printf("\n");
			//exit(0);
		}
	}

	//Should never get here because exit will take care of it
	close_shell(CmdPrompt);
}
