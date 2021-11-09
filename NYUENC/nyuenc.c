#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <semaphore.h>

#define ARR_SIZE 2147483648
#define CHUNK_SIZE 4096
#define TASKSAVAILABLE 2500000
//Use getopt to see if -j jobs were added, if so read argv of files from after jobs, if not just read argv of jobs
//offset needs to be subtracted from one if greater than zero?
void encode_File(off_t FileSize, char* file, int offset, char *arr) {
	unsigned char count = 0;
	char letter;
	int j = 0;
	for (int i = offset; i < offset+CHUNK_SIZE; i++) {
		if (!file[i])
			break;
		arr[j] = file[i];
		count++;
		while (file[i] == file[i + 1]) {
			count++;
			i++;
			if (i >= offset+CHUNK_SIZE-1)
				break;
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
	int fileOff; //where to read from
	int chunkSize; //how much to read (could be determined by offset)
	char* arr; //where to store the information
	struct stat fileInf;
	//LIST_ENTRY(Task) entries;
} Task;

//LIST_HEAD(listhead, Task);

Task taskQueue[TASKSAVAILABLE];
int taskCount = 0;

pthread_mutex_t counting;
pthread_cond_t countSignal;
int threadsStillRunning = 0;

pthread_mutex_t jobMutex;
pthread_cond_t jobCondition;

_Bool shouldContinue = true;
void addTask(Task task) {
	pthread_mutex_lock(&jobMutex);
	taskQueue[taskCount++] = task;
	pthread_mutex_unlock(&jobMutex);
	pthread_cond_signal(&jobCondition);
}
void* funcStart(void* args) {
	while (shouldContinue) {
		Task task;
		pthread_mutex_lock(&jobMutex);
		while (taskCount == 0) {
			pthread_cond_wait(&jobCondition, &jobMutex);
		}

		task = taskQueue[0];
		int i;
		for (i = 0; i < taskCount - 1; i++) {
			taskQueue[i] = taskQueue[i + 1];
		}
		taskCount--;
		pthread_mutex_unlock(&jobMutex);
		//fprintf(stderr, "File size: %i\nFile offset:%i\n", task.fileInf.st_size, task.fileOff);
		encode_File(task.fileInf.st_size, task.file, task.fileOff, task.arr);
	}
	pthread_mutex_lock(&counting);
	threadsStillRunning--;
	pthread_mutex_unlock(&counting);
	if(threadsStillRunning == 0)
		pthread_cond_signal(&countSignal);
}

void failOnExit(char** arr) {
	int i = 0;
	while (arr[i] != NULL) {
		free(arr[i]);
		++i;
	}
	free(arr);
	exit(1);
}
int main(int argc, char* const* argv) {
	int optIndex;
	int threadCount = 1; //Default to one thread
	off_t chunkSize = CHUNK_SIZE; //ASK ABOUT IF ITS 4096
	int fileOffset = 0;
	int countOfChunks = 0;
	char** TempArr = malloc(TASKSAVAILABLE); 
	//Get amount of threads to create and store in variable
	while ((optIndex = getopt(argc, argv, ":j:")) != -1) {
		switch (optIndex)
		{
		case 'j':
			//sscanf(optarg, "%d", &threadCount);
			threadCount = atol(optarg);
			//fprintf(stderr, "Jobs used, amount is: %d\n", threadCount);
		default:
			break;
		}
	}
	//Creating threads
	pthread_t* threadPool = malloc(threadCount * sizeof(pthread_t));
	pthread_mutex_init(&jobMutex, NULL);
	pthread_mutex_init(&counting, NULL);
	pthread_cond_init(&jobCondition, NULL);
	pthread_cond_init(&countSignal, NULL);
	for (int i = 0; i < threadCount; i++) {
		if (pthread_create(&threadPool[i], NULL, &funcStart, NULL) != 0) {
			perror("Could not create thread pool");
			failOnExit(TempArr);
		}
		pthread_mutex_lock(&counting);
		threadsStillRunning++;
		pthread_mutex_unlock(&counting);
	}
	//Find out how many files there are
	int fileCount = 0;
	char** fileArr = malloc(argc * sizeof(char*));
	off_t* fileSizes = calloc(argc, sizeof(off_t));
	//loop through commands and open files accordingly
	int i = 0;
	for (; optind < argc; optind++) {
		//each file is argv[optind]
		int fd = open(argv[optind], O_RDONLY); //Open file to read
		if (fd < 0) {
			fprintf(stderr,"File open failed on: %s \n", argv[optind]); //error checking
			failOnExit(TempArr);
		}
		//Get file length information
		//REFERENCE -> https://linuxhint.com/using_mmap_function_linux/ Idea for using fstat
		struct stat fileInfo;
		fstat(fd, &fileInfo);
		fileSizes[i] = fileInfo.st_size;
		fileArr[i] = mmap(NULL, fileSizes[i], PROT_READ, MAP_SHARED, fd, 0);
		fileCount++;
		if (fileArr[i] == MAP_FAILED) {
			perror("Map failed to open file in virtual memory\n"); //error checking
			failOnExit(TempArr);
		}
		close(fd); //close opened file now that it is in memory

		//Divide file into chunks and submit to task queue
		int tempSize = fileInfo.st_size;
		while (true) {
			TempArr[countOfChunks] = malloc(chunkSize *2);
			if (tempSize - chunkSize > 0) {
				Task temp = {
					.file = fileArr[i],
					.fileOff = fileOffset,
					.arr = TempArr[countOfChunks++],
					.fileInf = fileInfo
				};
				fileOffset += chunkSize;
				addTask(temp);
				tempSize -= chunkSize;
			}
			else {
				Task temp = {
					.file = fileArr[i],
					.fileOff = fileOffset,
					.arr = TempArr[countOfChunks++],
					.fileInf = fileInfo
				};
				addTask(temp);
				break;
			}
		}
		//fprintf(stderr, "Made it to 193\n");
		fileOffset = 0;
		++i;
	}

	//Stop all running threads
	while (taskCount != 0){}
	shouldContinue = false;
	pthread_cond_wait(&countSignal, &counting);
	//fprintf(stderr, "test\n");
	//implement a semaphore here where each thread ups it and downs only when leaving. 
	for (int i = 0; i < fileCount; ++i) {
		if (fileArr[i] == NULL)
			break;
		
		if (munmap(fileArr[i], fileSizes[i]) != 0) {
			fprintf(stderr, "Error unmapping file: %c\n", fileArr[i]);
			failOnExit(TempArr);
		}
	}
	//Destroying threads and mutex
	for (int i = 0; i < threadCount; i++) {
		if (pthread_join(threadPool[i], NULL) != 0) {
			perror("Could not join thread");
			failOnExit(TempArr);
		}
	}

	//fprintf(stderr, "before free filecount: %i\n", fileCount);

	free(fileArr);
	//fprintf(stderr, "in between free filecount: %i\n", fileCount);
	free(fileSizes);
	//fprintf(stderr, "after free filecount: %i\n", fileCount);
	pthread_mutex_destroy(&jobMutex);
	pthread_mutex_destroy(&counting);
	pthread_mutex_destroy(&jobMutex);
	pthread_cond_destroy(&countSignal);

	int printOffset = 0;
	_Bool setOffset = false;
	for (int i = 0; i < TASKSAVAILABLE; ++i) {
		if (setOffset) {
			printOffset = 2;
			setOffset = false;
		}
		else
			printOffset = 0;
		if (TempArr[i] == NULL)
			break; //break if no more left to print
		for (int j = 0 + printOffset; j < chunkSize * 2; ++j) {
			if (TempArr[i][j] != 0) {
				if(TempArr[i][j+2])
					printf("%c", TempArr[i][j]);
				else
				{
					//last element char and count
					//check if next element exits
					if (TempArr[i + 1] != NULL) {
						if (TempArr[i][j] == TempArr[i + 1][0]) {
							printf("%c%c", TempArr[i][j], TempArr[i][j + 1] + TempArr[i + 1][1]);
							setOffset = true;
							break;
						}
					}
					//next element does not exist, print both char and count
					printf("%c%c", TempArr[i][j], TempArr[i][j + 1]);
					break;
					
				}
			}
		}
	}
	//for (int i = 0; i < TASKSAVAILABLE; ++i) {
	//	if (TempArr[i] != NULL) {
	//		printf("%s", TempArr[i]);
	//	}
	//	else
	//		break;
	//}
	
	i = 0;
	while (TempArr[i] != NULL) {
		free(TempArr[i]);
		++i;
	}
	free(TempArr);
	return 0;
}