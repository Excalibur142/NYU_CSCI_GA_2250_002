#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

//Use getopt to see if -j jobs were added, if so read argv of files from after jobs, if not just read argv of jobs

void encode_File(off_t FileSize, char* file) {
	unsigned char count;
	char letter;
	for (int i = 0; i < FileSize; ++i) {
		letter = file[i];
		count++;
		while (file[i] == file[i + 1]) {
			count++;
			i++;
		}
		printf("%c%c", file[i], count);
		count = 0;
	}
	return;
}

int main(int argc, char* const *argv) {
	//read through argv
	int optIndex;
	int threadCount;
	//Get amount of threads to create and store in variable
	while ((optIndex = getopt(argc, argv, ":j:")) != -1) {
		switch (optIndex)
		{
		case 'j':
			//sscanf(optarg, "%d", &threadCount);
			threadCount = atol(optarg);
			printf("Jobs used, amount is: %d\n", threadCount);
		default:
			break;
		}
	}
	//loop through commands and open files accordingly
	for (; optind < argc; optind++) {
		//each file is argv[optind]
		int fd = open(argv[optind], O_RDONLY); //Open file to read
		if (fd < 0) {
			perror("File open failed\n"); //error checking
			exit(1);
		}
		//Get file length information
		//REFERENCE -> https://linuxhint.com/using_mmap_function_linux/ Idea for using fstat
		struct stat fileInfo;
		fstat(fd, &fileInfo);
		char *fileToRead = mmap(NULL, fileInfo.st_size, PROT_READ, MAP_SHARED, fd, 0);
		if (fileToRead == MAP_FAILED) {
			perror("Map failed to open file in virtual memory\n"); //error checking
			exit(1);
		}
		close(fd); //close opened file now that it is in memory
		//Encode file now. 
		encode_File(fileInfo.st_size, fileToRead);
		if (munmap(fileToRead, fileInfo.st_size) != 0) {
			perror("Error unmapping file\n");
			exit(1);
		}
	}

	return 0;
}