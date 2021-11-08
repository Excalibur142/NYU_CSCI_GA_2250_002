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

#define ARR_SIZE 2147483648
#define CHUNK_SIZE 4000
#define TASKSAVAILABLE 250000
//Use getopt to see if -j jobs were added, if so read argv of files from after jobs, if not just read argv of jobs
//offset needs to be subtracted from one if greater than zero?
void encode_File(off_t FileSize, char* file, int offset, char *arr) {
	unsigned char count = 0;
	char letter;
	int j = 0;
	for (int i = offset; i < offset+CHUNK_SIZE; ++i) {
		if (!file[i])
			break;
		arr[j] = file[i];
		count++;
		while (file[i] == file[i + 1]) {
			count++;
			i++;
			if (i >= FileSize)
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
		//fprintf(stderr, "File size: %i\nFile offset:%i\nTempor Array: %s\n", task.fileInf.st_size, task.fileOff, task.arr);
		//task.arr[0] = 'h';
		encode_File(task.fileInf.st_size, task.file, task.fileOff, task.arr);
	}
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
	//struct listhead head;
	//LIST_INIT(&head);
	//read through argv
	int optIndex;
	int threadCount = 1; //Default to one thread
	off_t chunkSize = CHUNK_SIZE; //ASK ABOUT IF ITS 4096
	int fileOffset = 0;
	int countOfChunks = 0;
	//int arrOffset = 0;
	//char* encode = malloc(ARR_SIZE);
	char** TempArr = malloc(250000); //MAKE SURE TO FREE LATER!
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
			failOnExit(TempArr);
		}
	}
	//Find out how many files there are
	int fileCount = argc;
	char** fileArr = malloc(fileCount);
	off_t* fileSizes = malloc(fileCount);
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
		if (fileArr[i] == MAP_FAILED) {
			perror("Map failed to open file in virtual memory\n"); //error checking
			failOnExit(TempArr);
		}
		close(fd); //close opened file now that it is in memory

		//Divide file into chunks and submit to task queue
		int tempSize = fileInfo.st_size;
		while (true) {
			TempArr[countOfChunks] = malloc(8000);
			if (tempSize - chunkSize > 0) {
				//printf("Tempsize is: %i\n", tempSize);
				//printf("Count of chunks is: %i\n", countOfChunks);
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
				//printf("Tempsize is: %i\n", tempSize);
				//fileOffset += chunkSize;
				//printf("Count of chunks is: %i\n", countOfChunks);
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
		fileOffset = 0;
		++i;
	}
	//Go through task queue
	//for (int i = 0; i < taskCount; ++i) {
	//	//if (taskQueue[i].fileOff) {
	//	//printf("File offset is: %i\n", taskQueue[i].fileOff);
	//		encode_File(taskQueue[i].fileInf.st_size, taskQueue[i].file, taskQueue[i].fileOff, taskQueue[i].arr);
	//		//strcat(encode, taskQueue[i].arr);
	//	//}
	//}
	//for (int i = 0; i < taskCount; ++i) {
	//	fprintf(stderr, "File size: %i\nFile offset:%i\nTempor Array: %s\nActual array: %s\n", taskQueue[i].fileInf.st_size, taskQueue[i].fileOff, taskQueue[i].arr, TempArr[i]);
	//}

	//Stop all running threads
	while (taskCount != 0){}
	shouldContinue = false;
	fprintf(stderr, "test\n");
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
	
	free(fileArr);
	free(fileSizes);
	pthread_mutex_destroy(&jobMutex);
	pthread_cond_destroy(&jobCondition);
	for (int i = 0; i < TASKSAVAILABLE; ++i) {
		if (TempArr[i] != NULL) {
			printf("%s", TempArr[i]);
		}
		else
			break;
	}
	//Print resulting encoded file
	//for (int i = 0; i < ARR_SIZE; ++i) {
	//	if (encode[i]) {
	//		if (encode[i + 3]) {
	//			//Concatenate ending and beginning of chunks/files
	//			//if (encode[i] == encode[i + 2]) {
	//			//	printf("%c%c", encode[i], encode[i + 1] + encode[i + 3]);
	//			//	i += 3;
	//			//}
	//			//else
	//				printf("%c", encode[i]);
	//		}
	//		else
	//			printf("%c", encode[i]);
	//	}
	//	else
	//		break;
	//}
	//free(encode);
	i = 0;
	while (TempArr[i] != NULL) {
		free(TempArr[i]);
		++i;
	}
	free(TempArr);
	return 0;
}