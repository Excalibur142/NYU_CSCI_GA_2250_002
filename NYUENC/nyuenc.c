#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

#define ARR_SIZE 2147483648
//Use getopt to see if -j jobs were added, if so read argv of files from after jobs, if not just read argv of jobs
//offset needs to be subtracted from one if greater than zero?
void encode_File(off_t FileSize, char* file, int* offset, char *arr) {
	unsigned char count = 0;
	char letter;
	int j = 0;
	for (int i = *offset; i < FileSize+*offset; ++i) {
		if (!file[i])
			break;
		arr[j] = file[i];
		count++;
		while (file[i] == file[i + 1]) {
			count++;
			i++;
		}
		arr[j + 1] = count;
		j+=2;
		//printf("%c%c", file[i], count);
		count = 0;
	}
	return;
}


typedef struct Task {
	char* file; //file to read from
	int fileOffset; //where to read from
	int chunkSize; //how much to read (could be determined by offset)
	char* arr; //where to store the information
} Task;

pthread_mutex_t jobMutex;
pthread_cond_t jobCondition;

_Bool shouldContinue = true;

void* funcStart(void* args) {
	while (shouldContinue) {
		Task task;
	}
}


int main(int argc, char* const* argv) {
	//read through argv
	int optIndex;
	int threadCount = 1; //Default to one thread
	int chunkSize = 4000; //ASK ABOUT IF ITS 4096
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
			//printf("Jobs used, amount is: %d\n", threadCount);
		default:
			break;
		}
	}
	//Creating threads
	pthread_t* threadPool = malloc(threadCount * sizeof(pthread_t));
	pthread_mutex_init(&jobMutex, NULL);
	pthread_cond_init(&jobCondition, NULL);
	for (int i = 0; i < threadCount; i++) {
		if (pthread_create(&threadPool[i], NULL, &funcStart, NULL) != 0) {
			perror("Could not create thread pool");
			exit(1);
		}
	}
	//loop through commands and open files accordingly
	for (; optind < argc; optind++) {
		//each file is argv[optind]
		int fd = open(argv[optind], O_RDONLY); //Open file to read
		if (fd < 0) {
			fprintf(stderr,"File open failed on: %s \n", argv[optind]); //error checking
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
		char tempArr[8000]; //ASK ABOUT 8192
		encode_File(chunkSize, fileToRead, &fileOffset, tempArr);
		
		//fileOffset = fileInfo.st_size;
		if (munmap(fileToRead, fileInfo.st_size) != 0) {
			perror("Error unmapping file\n");
			exit(1);
		}
		strcat(encode, tempArr);
	}

	//Stop all running threads
	shouldContinue = false;
	//Destroying threads and mutex
	for (int i = 0; i < threadCount; i++) {
		if (pthread_join(threadPool[i], NULL) != 0) {
			perror("Could not join thread");
			exit(1);
		}
	}
	pthread_mutex_destroy(&jobMutex);
	pthread_cond_destroy(&jobCondition);

	//Print resulting encoded file
	for (int i = 0; i < ARR_SIZE; ++i) {
		if (encode[i]) {
			if (encode[i + 3]) {
				//Concatenate ending and beginning of chunks/files
				//if (encode[i] == encode[i + 2]) {
				//	printf("%c%c", encode[i], encode[i + 1] + encode[i + 3]);
				//	i += 3;
				//}
				//else
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