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
		//if (file[i] == 'b' && i > 244000)
			//fprintf(stderr, "Value of count is: %i\nValue of i is: %i\n", count, i);
		while (file[i] == file[i + 1]) {
			if (i >= offset+CHUNK_SIZE -1)
				break;
			count++;
			i++;
			//if (file[i] == 'b' && i > 244000)
				//fprintf(stderr, "Value of count is: %i\nValue of i is: %i Value of offset is: %i\n", count, i,offset);
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
_Bool lastThread = false;

_Bool shouldContinue = true;
void addTask(Task task) {
	pthread_mutex_lock(&jobMutex);
	taskQueue[taskCount++] = task;
	pthread_mutex_unlock(&jobMutex);
	pthread_cond_signal(&jobCondition);
}
void* funcStart(void* args) {
	while (shouldContinue) {
		//fprintf(stderr, "task running\n");
		Task task;
		pthread_mutex_lock(&jobMutex);
		//fprintf(stderr, "task running but with mutex\n");

		while (taskCount == 0) {
			pthread_cond_wait(&jobCondition, &jobMutex);
			if (shouldContinue == false)
				break;
			//fprintf(stderr, "task paused on job wait\n");
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
			//fprintf(stderr, "File size: %i\nFile offset:%i\n", task.fileInf.st_size, task.fileOff);

		}
	}
	//fprintf(stderr, "Trying to grab mutex\n");
	pthread_mutex_lock(&counting);
	threadsStillRunning--;
	if (threadsStillRunning == 0) {
		lastThread = true;
	}
	pthread_mutex_unlock(&counting);
	//fprintf(stderr, "Released Thread Mutex\n");
	//if (threadsStillRunning == 0) {
	//	pthread_cond_signal(&countSignal);
	//	fprintf(stderr, "Last Thread\n");
	//
	//}
	pthread_exit(NULL);
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
	int threadCount = 0; //Default to one thread
	int chunkSize = CHUNK_SIZE; //ASK ABOUT IF ITS 4096
	int fileOffset = 0;
	int ammountOfTasks = 0;
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
			fprintf(stderr,"Could not create thread pool\n");
			failOnExit(TempArr);
		}
		//pthread_mutex_lock(&counting);
		threadsStillRunning++;
		//pthread_mutex_unlock(&counting);
	}
	//fprintf(stderr, "threads created: %i\n",threadsStillRunning);

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
		//fprintf(stderr, "About to chunk up data\n");

		while (true) {
			ammountOfTasks++;
			TempArr[countOfChunks] = malloc(chunkSize *2);
			if (tempSize - chunkSize > 0) {
				//fprintf(stderr, "Chunking data Size: %i\n",tempSize);
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
				//fprintf(stderr, "Chunked last data\n");
				break;
			}
		}
		fileOffset = 0;
		++i;
	}
	if (threadCount == 0) {
		for (int i = 0; i < taskCount; ++i) {
			encode_File(taskQueue[i].fileInf.st_size, taskQueue[i].file, taskQueue[i].fileOff, taskQueue[i].arr);
		}
		taskCount = 0;
	}
	//Stop all running threads
	//fprintf(stderr, "Test for jobs\n");

	while (taskCount != 0){}
	shouldContinue = false;
	pthread_cond_broadcast(&jobCondition);
	//fprintf(stderr, "Broadcast sent\n");

	if (threadsStillRunning > 0) { pthread_cond_broadcast(&jobCondition); }
	//fprintf(stderr, "tasks completed\n");
	
	//Destroying threads and mutex
	for (int i = 0; i < threadCount; i++) {
		//fprintf(stderr, "threadcount is: %i\n", threadCount);
		if (pthread_join(threadPool[i], NULL) != 0) {
			perror("Could not join thread");
			failOnExit(TempArr);
		}
	}
	
	free(fileArr);
	free(fileSizes);
	pthread_mutex_destroy(&jobMutex);
	pthread_mutex_destroy(&counting);
	pthread_cond_destroy(&jobCondition);
	pthread_cond_destroy(&countSignal);

	char last;
	char lastNum;
	for (int i = 0; i < ammountOfTasks; ++i) {
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


	i = 0;
	while (TempArr[i] != NULL) {
		free(TempArr[i]);
		++i;
	}
	free(TempArr);
	return 0;
}