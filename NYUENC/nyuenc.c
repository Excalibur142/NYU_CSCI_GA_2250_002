#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <semaphore.h>

//#define ARR_SIZE 2147483648
#define CHUNK_SIZE 4000
#define TASKSAVAILABLE 2500000
//Use getopt to see if -j jobs were added, if so read argv of files from after jobs, if not just read argv of jobs
//offset needs to be subtracted from one if greater than zero?
void encode_File(int FileSize, char* file, int offset, char *arr) {
	unsigned char count = 0;
	char letter;
	int j = 0;
	for (int i = offset; i < offset+CHUNK_SIZE; i++) {
		if (!file[i])
			break;
		arr[j] = file[i];
		count++;
		while (file[i] == file[i + 1]) {
			if (i >= offset+CHUNK_SIZE -1)
				break;
			count++;
			i++;
		}
		arr[j + 1] = count;		
		j+=2;
		count = 0;
	}
	return;
}

typedef struct Task {
	char* file; //file to read from
	int fileOff; //where to read from
	int chunkSize; //how much to read (could be determined by offset)
	char* arr; //where to store the information
	struct stat fileInf; //file information
	_Bool* writeCheck; //if write is finished
	int index; //where to store parallel bool writecheck at index of arr
} Task;

Task taskQueue[TASKSAVAILABLE];
int taskCount = 0;

pthread_mutex_t jobMutex;
pthread_cond_t jobCondition;

pthread_mutex_t printMutex;
pthread_cond_t printCond;

void addTask(Task task) {
	pthread_mutex_lock(&jobMutex);
	taskQueue[taskCount++] = task;
	pthread_cond_signal(&jobCondition);
	pthread_mutex_unlock(&jobMutex);
}
void* funcStart(void* args) {
	while (true) {
		Task task;
		pthread_mutex_lock(&jobMutex);
		while (taskCount == 0) {
			pthread_cond_wait(&jobCondition, &jobMutex);
		}
		if (taskQueue[0].file != NULL) {
			task = taskQueue[0];
			int i;
			for (i = 0; i < taskCount - 1; i++) {
				taskQueue[i] = taskQueue[i + 1];
			}
			taskCount--;
			pthread_mutex_unlock(&jobMutex);
			encode_File(task.fileInf.st_size, task.file, task.fileOff, task.arr);
			pthread_mutex_lock(&printMutex);
			task.writeCheck[task.index] = true;
			pthread_cond_signal(&printCond);
			pthread_mutex_unlock(&printMutex);
			//fprintf(stderr, "File size: %i\nFile offset:%i\n", task.fileInf.st_size, task.fileOff);

		}
	}
	
	pthread_exit(NULL);
}
//Free memory before exiting
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
	int threadCount = 0; //Default to one thread
	int chunkSize = CHUNK_SIZE; //ASK ABOUT IF ITS 4096
	int fileOffset = 0;
	int ammountOfTasks = 0;
	int countOfChunks = 0;
	char** TempArr = malloc(TASKSAVAILABLE); 
	_Bool* finishedWrite = malloc(TASKSAVAILABLE);
	//Get amount of threads to create and store in variable
	while ((optIndex = getopt(argc, argv, ":j:")) != -1) {
		switch (optIndex)
		{
		case 'j':
			//sscanf(optarg, "%d", &threadCount);
			threadCount = atol(optarg);
		default:
			break;
		}
	}
	//Creating threads
	pthread_t* threadPool = malloc(threadCount * sizeof(pthread_t));
	pthread_mutex_init(&jobMutex, NULL);
	pthread_mutex_init(&printMutex, NULL);
	
	pthread_cond_init(&jobCondition, NULL);
	pthread_cond_init(&printCond, NULL);
	
	for (int i = 0; i < threadCount; i++) {
		if (pthread_create(&threadPool[i], NULL, &funcStart, NULL) != 0) {
			fprintf(stderr,"Could not create thread pool\n");
			failOnExit(TempArr);
		}
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
			fprintf(stderr,"Map failed to open file in virtual memory\n"); //error checking
			failOnExit(TempArr);
		}
		close(fd); //close opened file now that it is in memory

		//Divide file into chunks and submit to task queue
		int tempSize = fileInfo.st_size;
		while (true) {
			ammountOfTasks++;
			TempArr[countOfChunks] = malloc(chunkSize *2);
			if (tempSize - chunkSize > 0) {
				Task temp = {
					.file = fileArr[i],
					.fileOff = fileOffset,
					.arr = TempArr[countOfChunks],
					.fileInf = fileInfo,
					.writeCheck = finishedWrite,
					.index = countOfChunks++
				};
				fileOffset += chunkSize;
				addTask(temp);
				tempSize -= chunkSize;
			}
			else {
				Task temp = {
					.file = fileArr[i],
					.fileOff = fileOffset,
					.arr = TempArr[countOfChunks],
					.fileInf = fileInfo,
					.writeCheck = finishedWrite,
					.index = countOfChunks++
				};
				addTask(temp);
				break;
			}
		}
		fileOffset = 0;
		++i;
	}
	if (threadCount == 0) {
		for (int i = 0; i < taskCount; ++i) {
			encode_File(taskQueue[i].fileInf.st_size, taskQueue[i].file, taskQueue[i].fileOff, taskQueue[i].arr);
			finishedWrite[i] = true;
		}
		taskCount = 0;
	}

	char last;
	char lastNum;
	for (int i = 0; i < ammountOfTasks; ++i) {
		pthread_mutex_lock(&printMutex);
		//while (TempArr[i] == NULL) {
		//	pthread_cond_wait(&printCond, &printMutex); //try test and wait or compare and swap
		//}
		while (finishedWrite[i] == false) {
			pthread_cond_wait(&printCond, &printMutex);
		}
		pthread_mutex_unlock(&printMutex);
		//Only print a task if thread is done, 
		if (TempArr[i] != NULL) {
			if (i+1 == ammountOfTasks) { //last element
				if (last == TempArr[i][0]) {
					printf("%c%c", last, lastNum + TempArr[i][1]);
					printf("%.*s", strlen(TempArr[i]) - 2, TempArr[i] + 2);
				}
				else if(last){
					printf("%c%c", last, lastNum);
					printf("%s", TempArr[i]);
				}
				else
					printf("%s", TempArr[i]);
			}
			else if (last == TempArr[i][0]) { //check if last char is the same
				printf("%c%c", last, lastNum + TempArr[i][1]);
				printf("%.*s", strlen(TempArr[i]) - 4, TempArr[i]+2);
				last = TempArr[i][strlen(TempArr[i]) - 2];
				lastNum = TempArr[i][strlen(TempArr[i]) - 1];
			}
			else { //print and chop off last 2 elements
				if (last && last != TempArr[i][0]) {
					printf("%c%c", last, lastNum);//dont drop last char if chunk boundary is not the same
				}
				printf("%.*s", strlen(TempArr[i]) - 2, TempArr[i]);
				last = TempArr[i][strlen(TempArr[i]) - 2];
				lastNum = TempArr[i][strlen(TempArr[i]) - 1];
			}
		}
		else
			break;
	}
	
	free(fileArr);
	free(fileSizes);
	free(finishedWrite);
	i = 0;
	while (TempArr[i] != NULL) {
		free(TempArr[i]);
		++i;
	}
	free(TempArr);
	return 0;
}