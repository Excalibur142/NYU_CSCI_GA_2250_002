#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

#define ARR_SIZE 2147483648

//Use getopt to see if -j jobs were added, if so read argv of files from after jobs, if not just read argv of jobs

void encode_File(off_t FileSize, char* file, int* offset, char *arr) {
	unsigned char count;
	char letter;
	int j = 0;
	for (int i = *offset; i < FileSize+*offset; ++i) {
		if (!file[i])
			break;
		letter = file[i];
		count++;
		while (file[i] == file[i + 1]) {
			count++;
			i++;
		}
		arr[j] = file[i];
		arr[j + 1] = count;
		j+=2;
		//printf("%c%c", file[i], count);
		count = 0;
	}
	return;
}

int main(int argc, char* const* argv) {
	//read through argv
	int optIndex;
	int threadCount;
	int chunkSize = 4000;
	int fileOffset = 0;
	//int arrOffset = 0;
	char* encode = malloc(ARR_SIZE);
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
		char* fileToRead = mmap(NULL, fileInfo.st_size, PROT_READ, MAP_SHARED, fd, 0);
		if (fileToRead == MAP_FAILED) {
			perror("Map failed to open file in virtual memory\n"); //error checking
			exit(1);
		}
		close(fd); //close opened file now that it is in memory
		//Encode file now.
		char tempArr[8000];
		encode_File(chunkSize, fileToRead, &fileOffset, tempArr);
		
		//fileOffset = fileInfo.st_size;
		if (munmap(fileToRead, fileInfo.st_size) != 0) {
			perror("Error unmapping file\n");
			exit(1);
		}
		strcat(encode, tempArr);
	}
	
	for (int i = 0; i < ARR_SIZE; ++i) {
		if (encode[i]) {
			if (encode[i + 3]) {
				if (encode[i] == encode[i + 2]) {
					printf("%c%c", encode[i], encode[i + 1] + encode[i + 3]);
					i += 3;
				}
				else
					printf("%c", encode[i]);
			}
			else
				printf("%c", encode[i]);
		}
		else
			break;
	}
	free(encode);
	return 0;
}