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
//Get and parse command line
//Each command is terminated by enter key or \n newline


int main(int argc, const char* const* argv) {
	//Create variable to store current directory in
	//This way only get_dir() needs to be called if directory changes
	char* ReturnVal = calloc(10, sizeof(char*));
	if (ReturnVal == NULL) {
		perror("calloc failed");
		exit(1);
	}
	make_dir(&ReturnVal);
	if(ReturnVal !=NULL)
		printf("%s\n",ReturnVal);
	

	//Create a while loop that waits for exit and exits on command
	//while (1) {


	//}

	close_shell(ReturnVal);
}
