#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <openssl/sha.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <byteswap.h>
#include <openssl/sha.h>

#define SHA_DIGEST_LENGTH 20

#pragma pack(push,1)
typedef struct BootEntry {
	unsigned char  BS_jmpBoot[3];     // Assembly instruction to jump to boot code
	unsigned char  BS_OEMName[8];     // OEM Name in ASCII
	unsigned short BPB_BytsPerSec;    // Bytes per sector. Allowed values include 512, 1024, 2048, and 4096 *******IMPORTANT*******
	unsigned char  BPB_SecPerClus;    // Sectors per cluster (data unit). Allowed values are powers of 2, but the cluster size must be 32KB or smaller *******IMPORTANT*******
	unsigned short BPB_RsvdSecCnt;    // Size in sectors of the reserved area *******IMPORTANT*******
	unsigned char  BPB_NumFATs;       // Number of FATs *******IMPORTANT*******
	unsigned short BPB_RootEntCnt;    // Maximum number of files in the root directory for FAT12 and FAT16. This is 0 for FAT32
	unsigned short BPB_TotSec16;      // 16-bit value of number of sectors in file system *******IMPORTANT*******
	unsigned char  BPB_Media;         // Media type
	unsigned short BPB_FATSz16;       // 16-bit size in sectors of each FAT for FAT12 and FAT16. For FAT32, this field is 0
	unsigned short BPB_SecPerTrk;     // Sectors per track of storage device
	unsigned short BPB_NumHeads;      // Number of heads in storage device
	unsigned int   BPB_HiddSec;       // Number of sectors before the start of partition
	unsigned int   BPB_TotSec32;      // 32-bit value of number of sectors in file system. Either this value or the 16-bit value above must be 0 *******IMPORTANT*******
	unsigned int   BPB_FATSz32;       // 32-bit size in sectors of one FAT *******IMPORTANT*******
	unsigned short BPB_ExtFlags;      // A flag for FAT
	unsigned short BPB_FSVer;         // The major and minor version number
	unsigned int   BPB_RootClus;      // Cluster where the root directory can be found *******IMPORTANT*******
	unsigned short BPB_FSInfo;        // Sector where FSINFO structure can be found
	unsigned short BPB_BkBootSec;     // Sector where backup copy of boot sector is located
	unsigned char  BPB_Reserved[12];  // Reserved
	unsigned char  BS_DrvNum;         // BIOS INT13h drive number
	unsigned char  BS_Reserved1;      // Not used
	unsigned char  BS_BootSig;        // Extended boot signature to identify if the next three values are valid
	unsigned int   BS_VolID;          // Volume serial number
	unsigned char  BS_VolLab[11];     // Volume label in ASCII. User defines when creating the file system
	unsigned char  BS_FilSysType[8];  // File system type label in ASCII
} BootEntry;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct DirEntry {
	unsigned char  DIR_Name[11];      // File name *******IMPORTANT*******
	unsigned char  DIR_Attr;          // File attributes *******IMPORTANT*******
	unsigned char  DIR_NTRes;         // Reserved
	unsigned char  DIR_CrtTimeTenth;  // Created time (tenths of second)
	unsigned short DIR_CrtTime;       // Created time (hours, minutes, seconds)
	unsigned short DIR_CrtDate;       // Created day
	unsigned short DIR_LstAccDate;    // Accessed day
	unsigned short DIR_FstClusHI;     // High 2 bytes of the first cluster address *******IMPORTANT*******
	unsigned short DIR_WrtTime;       // Written time (hours, minutes, seconds
	unsigned short DIR_WrtDate;       // Written day
	unsigned short DIR_FstClusLO;     // Low 2 bytes of the first cluster address *******IMPORTANT*******
	unsigned int   DIR_FileSize;      // File size in bytes. (0 for directories) *******IMPORTANT*******
	unsigned int   DIR_ActualStart;   // Actual byte start of root directory entry
} DirEntry;
#pragma pack(pop)

void fail() {
	printf("Usage: ./nyufile disk <options>\n  -i                     Print the file system information.\n  -l                     List the root directory.\n  -r filename [-s sha1]  Recover a contiguous file.\n  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
	exit(1);
}
//Custom write to disk function that writes any unsigned int to little endian format
void writeIntToDisk(unsigned int Num, char* diskToWrite, int byteOffset) {

	unsigned char num[8];
	unsigned char nums[4];
	for (int i = 0; i < 8; ++i)
		num[i] = 0;
	int count = 0;
	while (true) {
		num[count++] = Num % 16;
		if (Num / 16 == 0)
			break;
		Num = Num / 16;
	}
	nums[0] = num[1] * 16 + num[0];
	nums[1] = num[3] * 16 + num[2];
	nums[2] = num[5] * 16 + num[4];
	nums[3] = num[7] * 16 + num[6];
	for (int i = 0; i < 4; ++i) {
		diskToWrite[byteOffset + i] = nums[i];
	}
}

//u is starting cluster, and can then go up to 11
void DFS(unsigned int u, unsigned int* path, char* checkedCluster, _Bool* visted, int pathIndex, char* fatDiskCpy, BootEntry Boot, DirEntry** Directory, unsigned int dataStart, int indexOfFile, unsigned char sha1Val[], unsigned int* path2, unsigned int firstFatStart, unsigned int fileSize, char* fileToRec, _Bool *pF) {
	visted[u] = true;
	path[pathIndex++] = u;
	//add u to the checkcluster array
	unsigned int clusterStart = (u)*Boot.BPB_SecPerClus * Boot.BPB_BytsPerSec;
	strncat(checkedCluster, fatDiskCpy + dataStart + clusterStart, Boot.BPB_SecPerClus * Boot.BPB_BytsPerSec);
	//check if sha1sum matches
	unsigned char sha1Sum[20];
	SHA1(checkedCluster, Directory[indexOfFile]->DIR_FileSize, sha1Sum);

	if (strncmp(sha1Sum, sha1Val, 20) == 0) {
		_Bool found = true;
		unsigned int checkNum = 0;

		for (int i = 0; i < fileSize; ++i) {
			checkNum = fatDiskCpy[firstFatStart + ((path[i] + 2) * 4)] + (fatDiskCpy[firstFatStart + ((path[i] + 2) * 4) + 1] << 8) + (fatDiskCpy[firstFatStart + ((path[i] + 2) * 4) + 2] << 16) + (fatDiskCpy[firstFatStart + ((path[i] + 2) * 4) + 3] << 24);
			if (checkNum != 0)
				found = false;
		}
		
		if (found) {
			//Recover the file
			fatDiskCpy[Directory[indexOfFile]->DIR_ActualStart] = fileToRec[0];
			for (int i = 0; i < Boot.BPB_NumFATs; ++i) {
				//do this for each fat
				for (int k = 0; k < fileSize; ++k) {
					if (k + 1 == fileSize)
						writeIntToDisk(268435455, fatDiskCpy, (i * Boot.BPB_FATSz32 * Boot.BPB_BytsPerSec) + firstFatStart + ((path[k] + 2) * 4));
					else
						writeIntToDisk((path[k+1]+2), fatDiskCpy, (i * Boot.BPB_FATSz32 * Boot.BPB_BytsPerSec) + firstFatStart + ((path[k] + 2) * 4));
				}
			}
			*pF = true;
			printf("%s: successfully recovered with SHA-1\n", fileToRec);
		}
	}
	else {
		for (int i = 0; i < 10; i++) {
			if (visted[i] != true)
				DFS(i, path, checkedCluster, visted, pathIndex, fatDiskCpy, Boot, Directory, dataStart, indexOfFile, sha1Val, path2, firstFatStart, fileSize, fileToRec, pF);
		}
	}
	//You have to remove the index from the visited list to check all possible paths
	visted[u] = false;
	//move pointer of checked cluster back
	int size = pathIndex * Boot.BPB_SecPerClus * Boot.BPB_BytsPerSec;
	path[pathIndex] = 0;
	pathIndex--;
	checkedCluster[size - Boot.BPB_SecPerClus * Boot.BPB_BytsPerSec] = '\0';
}
//Only outside reference
//Reference: https://stackoverflow.com/questions/25748791/how-to-form-an-asciihex-number-using-2-chars
//Reference only for how to hex decode a string literal
char hex2byte(char* hs)
{
	char b = 0;
	char nibbles[2];
	int i;

	for (i = 0; i < 2; i++) {
		if ((hs[i] >= '0') && (hs[i] <= '9'))
			nibbles[i] = hs[i] - '0';
		else if ((hs[i] >= 'A') && (hs[i] <= 'F'))
			nibbles[i] = (hs[i] - 'A') + 10;
		else if ((hs[i] >= 'a') && (hs[i] <= 'f'))
			nibbles[i] = (hs[i] - 'a') + 10;
		else
			return 0;
	}

	b = (nibbles[0] << 4) | nibbles[1];
	return b;
}//END Reference

int main(int argc, char* argv[]) {
	_Bool noOption = true;

	_Bool infoChosen = false;
	_Bool listChosen = false;
	_Bool recContigChosen = false;
	_Bool recNonContigChosen = false;
	_Bool sha1Chosen = false;
	unsigned char* sha1Value;
	char* fileToRecover;
	if (argc >= 3) {
		if (argv[2][0] != '-') {
			//second argument is not an arg
			fail();
		}
	}
	char* diskName = argv[1];
	char* nextInd;
	int ch;
	while ((ch = getopt(argc, argv, "ilr:R:s:")) != -1) {
		if (optind == 2)
			fail();
		nextInd = argv[optind];
		noOption = false;
		switch (ch)
		{
		case 'i':
			infoChosen = true;
			break;
		case 'l':
			listChosen = true;
			break;
		case 'r':
			fileToRecover = *&optarg;
			recContigChosen = true;
			break;
		case 'R':
			fileToRecover = *&optarg;
			recNonContigChosen = true;
			break;
		case 's':
			sha1Value = *&optarg;
			sha1Chosen = true;
			if (strlen(sha1Value) % 40 != 0) {
				printf("%s: file not found\n", fileToRecover);
				exit(1);
			}
			break;
		default:
			//Print usage and exit
			fail();
		}
	}
	if (recNonContigChosen && !sha1Chosen)
		fail();
	if (noOption || nextInd != NULL) {
		fail();
	}
	if ((infoChosen && listChosen) || (infoChosen && recContigChosen) || (infoChosen && recNonContigChosen) || (listChosen && recContigChosen) || (listChosen && recNonContigChosen))
		fail();
	//Open FAT Disk Image
	int fd = open(diskName, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "File open failed on: %s \n", diskName);
		exit(1);
	}
	struct stat fileInfo;
	fstat(fd, &fileInfo);
	char* fatDisk = mmap(NULL, fileInfo.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	BootEntry BootInfo;
	//Grab boot info for milestone 2
	BootInfo.BPB_NumFATs = fatDisk[16];
	BootInfo.BPB_BytsPerSec = (fatDisk[12] * 256) + fatDisk[11];
	BootInfo.BPB_SecPerClus = fatDisk[13];
	BootInfo.BPB_RsvdSecCnt = (fatDisk[15] * 256) + fatDisk[14];

	if (infoChosen) {
		printf("Number of FATs = %i\n", BootInfo.BPB_NumFATs);
		printf("Number of bytes per sector = %i\n", BootInfo.BPB_BytsPerSec);
		printf("Number of sectors per cluster = %i\n", BootInfo.BPB_SecPerClus);
		printf("Number of reserved sectors = %i\n", BootInfo.BPB_RsvdSecCnt);
	}

	//To get to data area, it is right after the last fat. First fat is located after reserved area. So to get to data area
	//add reserved area*(bytespersec) + ((numFAT * sizeOfFatinsectors)*bytespersec)

	//Set root cluster from fatdisk[44] -> fatdisk[47]
	//in order of 45 44 47 46

	BootInfo.BPB_RootClus = fatDisk[44] + (fatDisk[45] << 8) + (fatDisk[46] << 16) + (fatDisk[47] << 24);
	BootInfo.BPB_FATSz32 = fatDisk[36] + (fatDisk[37] << 8) + (fatDisk[38] << 16) + (fatDisk[39] << 24);
	unsigned int dataArea = (BootInfo.BPB_RsvdSecCnt * BootInfo.BPB_BytsPerSec) + ((BootInfo.BPB_NumFATs * BootInfo.BPB_FATSz32) * BootInfo.BPB_BytsPerSec);
	//Do some math to find where root directory is
	unsigned int rootDirData = ((BootInfo.BPB_RootClus - 2) * BootInfo.BPB_SecPerClus) * BootInfo.BPB_BytsPerSec + dataArea;
	//First fat is after reserved data (every other fat is nth FAT(FAT SIZE) + 1st FAT start plus
	unsigned int FirstFatStart = (BootInfo.BPB_RsvdSecCnt * BootInfo.BPB_BytsPerSec);
	//Size of a cluster in bytes is:
	int clusterSizeInBytes = BootInfo.BPB_SecPerClus * BootInfo.BPB_BytsPerSec;
	//Read directory 
	//Loop through directory and find number of entries
	unsigned int i = rootDirData;
	int numEntries = (BootInfo.BPB_FATSz32 * BootInfo.BPB_BytsPerSec) / 32; //WILL INCLUDE "DELETED" FILES
	DirEntry** RootDir = malloc(sizeof(DirEntry*) * numEntries);
	int j = 0;
	unsigned int previousCluster = BootInfo.BPB_RootClus;
	int numLessThanClusterSize = 0;
	numEntries = 0;
	while (numLessThanClusterSize < clusterSizeInBytes + 1) { //Goes till end of cluster
		if (fatDisk[i] == 0) {
			i += 32; //if hole, break
			numLessThanClusterSize += 32;
			break;
		}
		if (numLessThanClusterSize >= clusterSizeInBytes) { //if root dir spans across multiple clusters, go back to fat disk and make i the new offest
			//Check if previous clustersize is FFFFFF0F
			//go back to FAT to get next cluster
			unsigned int k = FirstFatStart + (previousCluster * 4);
			unsigned int nextCluster = fatDisk[k] + (fatDisk[k + 1] << 8) + (fatDisk[k + 2] << 16) + (fatDisk[k + 3] << 24);
			if (nextCluster >= 250000000)
				break;
			previousCluster = nextCluster;
			//set i to where next cluster is
			i = ((nextCluster - 2) * BootInfo.BPB_SecPerClus) * BootInfo.BPB_BytsPerSec + dataArea;
			numLessThanClusterSize = 0;
		}
		RootDir[j] = malloc(sizeof(DirEntry));
		memcpy(&RootDir[j]->DIR_Name, &fatDisk[i], 11);
		RootDir[j]->DIR_Attr = fatDisk[i + 11];
		RootDir[j]->DIR_FileSize = fatDisk[i + 28] + (fatDisk[i + 29] << 8) + (fatDisk[i + 30] << 16) + (fatDisk[i + 31] << 24);
		RootDir[j]->DIR_FstClusLO = fatDisk[i + 26] + (fatDisk[i + 27] << 8);
		RootDir[j]->DIR_FstClusHI = fatDisk[i + 20] + (fatDisk[i + 21] << 8);
		RootDir[j]->DIR_ActualStart = i; //where i is the actual byte location of each directory entry starting point !!!Useful for changing e5 to letter!!!
		j++;
		i += 32;
		numEntries++;
		numLessThanClusterSize += 32;
	}
	int numEntriesDeleted = 0;
	if (listChosen) {
		for (int i = 0; i < numEntries; ++i) {
			if (RootDir[i]->DIR_Name[0] == 229) {
				numEntriesDeleted++;
				continue; //Do not print deleted files
			}
			//Find if you need to truncate the file name
			int amountToChop = 0;
			int amountToChopExt = 0;
			for (int j = 7; j > 0; --j) {
				if (RootDir[i]->DIR_Name[j] == 32) {
					++amountToChop;
				}
				else if (RootDir[i]->DIR_Name[j] != 32)
					break;
			}
			for (int j = 10; j > 7; --j) {
				if (RootDir[i]->DIR_Name[j] == 32) {
					++amountToChopExt;
				}
				else if (RootDir[i]->DIR_Name[j] != 32)
					break;
			}
			int startingCluster = RootDir[i]->DIR_FstClusLO + (RootDir[i]->DIR_FstClusHI << 16);
			if (RootDir[i]->DIR_Attr == 16) {
				//Directory found
				if (amountToChopExt < 3) {
					printf("%.*s.%.*s/ (size = %i, starting cluster = %i)\n", 8 - amountToChop, RootDir[i]->DIR_Name, 3 - amountToChopExt, RootDir[i]->DIR_Name + 8, RootDir[i]->DIR_FileSize, startingCluster);
				}
				else {
					printf("%.*s/ (size = %i, starting cluster = %i)\n", 8 - amountToChop, RootDir[i]->DIR_Name, RootDir[i]->DIR_FileSize, startingCluster);
				}
			}
			else if (amountToChopExt < 3) {
				//if file extension is not empty, print dot
				printf("%.*s.%.*s (size = %i, starting cluster = %i)\n", 8 - amountToChop, RootDir[i]->DIR_Name, 3 - amountToChopExt, RootDir[i]->DIR_Name + 8, RootDir[i]->DIR_FileSize, startingCluster);
			}
			else
				printf("%.*s%.*s (size = %i, starting cluster = %i)\n", 8 - amountToChop, RootDir[i]->DIR_Name, 3 - amountToChopExt, RootDir[i]->DIR_Name + 8, RootDir[i]->DIR_FileSize, startingCluster);
		}
		printf("Total number of entries = %i\n", numEntries - numEntriesDeleted);
	}
	unsigned char sha1ValHex[20];
	if (sha1Chosen) {
		//Decode ascii hex string into hex array of 2 ascii letters for 1 byte
		for (int i = 0, k = 0; k < 40; k += 2, ++i) {
			sha1ValHex[i] = hex2byte(sha1Value + k);
		}
	}
	if (recContigChosen || recNonContigChosen) {
		//Recover a contiguously allocated file when sha1 has not been supplied
		//format fileToRecover to be in structure of directory entry
		unsigned char recoverFile[11];
		memset(recoverFile, ' ', 11);
		//Check if fileToRecover has a dot ext
		int indexOfDot;
		_Bool indexFound = false;
		for (int i = 0; i < strlen(fileToRecover); ++i) {
			if (fileToRecover[i] == '.') {
				indexOfDot = i;
				indexFound = true;
				break;
			}
		}
		if (indexFound && strlen(fileToRecover) - indexOfDot > 4) {
			printf("%s: file not found\n", fileToRecover);
			exit(1);
		}
		if (indexFound) {
			//fileToRecover has an extension
			for (int i = 0; i < indexOfDot; ++i) {
				recoverFile[i] = fileToRecover[i];
			}
			int j = 0;
			for (int i = indexOfDot + 1; i < indexOfDot + 4; ++i) {
				recoverFile[8 + j] = fileToRecover[i];
				++j;
			}
		}
		else {
			memcpy(recoverFile, fileToRecover, 11);
			//Pad with zeros
			for (int i = strlen(fileToRecover); i < 11; ++i) {
				recoverFile[i] = ' ';
			}
		}
		//go through root directory and see if file can be found
		int indexOfFileToRecover;
		int countOfFilesMatched = 0;
		for (int i = 0; i < numEntries; ++i) {
			if (RootDir[i]->DIR_Name[0] != 229)
				continue;
			if (strncmp(RootDir[i]->DIR_Name + 1, recoverFile + 1, 10) == 0) {
				indexOfFileToRecover = i;
				countOfFilesMatched++;
			}
		}
		if (countOfFilesMatched > 1) {
			if (!sha1Chosen) {
				printf("%s: multiple candidates found\n", fileToRecover);
				exit(1);
			}
		}
		else if (countOfFilesMatched < 1) {
			printf("%s: file not found\n", fileToRecover);
			exit(1);
		}

		//Very poorly written unoptimized code
		//if sha1 has been entered:
			//Go through the directory again and search the actual file(sector) that the sha1 matches. 
				//Brute force every ambiguous file -> This will catch both single and contiguous cluster files
			//If one matches, set the indexOfFileToRecover to the matching file.
			//If none match, return failure and exit. 
		if (sha1Chosen && !recNonContigChosen) {
			_Bool matchFound = false;
			for (int i = 0; i < numEntries; ++i) {
				if (RootDir[i]->DIR_Name[0] != 229)
					continue;
				if (strncmp(RootDir[i]->DIR_Name + 1, recoverFile + 1, 10) == 0) {
					//Found an ambiguous file
					//Compute size of bytes to read.
					int clustersOcc = (RootDir[i]->DIR_FileSize + ((BootInfo.BPB_BytsPerSec * BootInfo.BPB_SecPerClus) - 1)) / (BootInfo.BPB_BytsPerSec * BootInfo.BPB_SecPerClus); //is this needed?
					unsigned char sha1Sum[20];
					//Calculate where cluster data is in bytes
					int startClus = RootDir[i]->DIR_FstClusLO + (RootDir[i]->DIR_FstClusHI << 16);
					SHA1(fatDisk + ((startClus - 2) * BootInfo.BPB_SecPerClus * BootInfo.BPB_BytsPerSec) + dataArea, RootDir[i]->DIR_FileSize, sha1Sum);
					//Check if entry is the one specified
					if (strncmp(sha1Sum, sha1ValHex, 20) == 0)
						matchFound = true;
					if (matchFound) {
						indexOfFileToRecover = i;
						break;
					}
				}
			}
			if (!matchFound) {
				printf("%s: file not found\n", fileToRecover);
				exit(1);
			}
		}
		int startingCluster = RootDir[indexOfFileToRecover]->DIR_FstClusLO + (RootDir[indexOfFileToRecover]->DIR_FstClusHI << 16);
		int clustersOccupied = (RootDir[indexOfFileToRecover]->DIR_FileSize + ((BootInfo.BPB_BytsPerSec * BootInfo.BPB_SecPerClus) - 1)) / (BootInfo.BPB_BytsPerSec * BootInfo.BPB_SecPerClus);
		if (recNonContigChosen) {
			//MILESTONE 8
			//BIG BOI
			//Call dfs from 2 -> 11, with starting cluster being 2 -> 11
			unsigned int path[10];
			char* fileToCheck = malloc(clusterSizeInBytes * 10);
			unsigned int pathFound[10];
			_Bool pFound = false;
			_Bool visitedPath[10];
			int l = 0;
			//Call DFS
			if (clustersOccupied != 0) {
				DFS(startingCluster - 2, path, fileToCheck, visitedPath, l, fatDisk, BootInfo, RootDir, dataArea, indexOfFileToRecover, sha1ValHex, pathFound, FirstFatStart, clustersOccupied, fileToRecover, &pFound);
				if (!pFound)
					printf("%s: file not found\n", fileToRecover);
			}
			else {
				//cluster size 0 and file found, just change dir entry
				fatDisk[RootDir[indexOfFileToRecover]->DIR_ActualStart] = fileToRecover[0];
				printf("%s: successfully recovered with SHA-1\n", fileToRecover);
			}
			free(fileToCheck);
		}
		else {
			//Change e5 to first letter
			fatDisk[RootDir[indexOfFileToRecover]->DIR_ActualStart] = fileToRecover[0];
			//File found now, recover it
			//To recover a small file that spans the size of one cluster change starting cluster to EOC
			//First 8 bytes are reserved? Multiply cluster number by 4 to get byte location in FAT
			if (startingCluster != 0) {
				//If there is a starting cluster, update FATs, if not dont update and return file update successful. DIR entry updated
				//IF FILE IS ONLY 1 CLUSTER LONG DO THIS
				if (clustersOccupied == 1) {
					unsigned int bytesToChange = startingCluster * 4;
					for (int j = 0; j < BootInfo.BPB_NumFATs; ++j) {
						for (int i = 0; i < 3; ++i) {
							fatDisk[(j * BootInfo.BPB_FATSz32 * BootInfo.BPB_BytsPerSec) + FirstFatStart + bytesToChange + i] = 255;
						}
						fatDisk[(j * BootInfo.BPB_FATSz32 * BootInfo.BPB_BytsPerSec) + FirstFatStart + bytesToChange + 3] = 15;
					}
				}
				else {
					//FILE IS MORE THAN ONE CLUSTER!!
							//How many clusters does this thing span?
							//Divide file size by bytes per cluster (cluster may have more than one sector)
							//Go through FATs and change cluster sequences
					for (int j = 0; j < BootInfo.BPB_NumFATs; ++j) {
						for (int k = 0; k < clustersOccupied; ++k) {
							//Change each cluster in a FAT
							//Convert next cluster number to little endian format
							if (k + 1 == clustersOccupied)
								writeIntToDisk(268435455, fatDisk, (j * BootInfo.BPB_FATSz32 * BootInfo.BPB_BytsPerSec) + FirstFatStart + (startingCluster + k) * 4);
							else
								writeIntToDisk(startingCluster + k + 1, fatDisk, (j * BootInfo.BPB_FATSz32 * BootInfo.BPB_BytsPerSec) + FirstFatStart + (startingCluster + k) * 4);
						}
					}
				}
			}
			if (sha1Chosen) {
				printf("%s: successfully recovered with SHA-1\n", fileToRecover);
			}
			else
				printf("%s: successfully recovered\n", fileToRecover);
		}
	}
	if (recNonContigChosen) {
	}
	return 0;
}
