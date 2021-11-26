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
} DirEntry;
#pragma pack(pop)

void fail() {
	printf("Usage: ./nyufile disk <options>\n	- i                     Print the file system information.\n	- l                     List the root directory.\n	- r filename [-s sha1]  Recover a contiguous file.\n	- R filename - s sha1    Recover a possibly non - contiguous file.\n");
	exit(1);
}
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

//Reference: https://stackoverflow.com/questions/25748791/how-to-form-an-asciihex-number-using-2-chars
//Reference only for hex_decode function taken from website
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
}

int main(int argc, char* argv[]) {
	_Bool noOption = true;

	_Bool infoChosen = false;
	_Bool listChosen = false;
	_Bool recContigChosen = false;
	_Bool recNonContigChosen = false;
	_Bool sha1Chosen = false;
	char* sha1Value;
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
	//printf("root cluster is at: %i\n", BootInfo.BPB_RootClus);
	BootInfo.BPB_FATSz32 = fatDisk[36] + (fatDisk[37] << 8) + (fatDisk[38] << 16) + (fatDisk[39] << 24);
	//printf("Fat size in sectors: %i\n", BootInfo.BPB_FATSz32);
	int dataArea = (BootInfo.BPB_RsvdSecCnt * BootInfo.BPB_BytsPerSec) + ((BootInfo.BPB_NumFATs * BootInfo.BPB_FATSz32) * BootInfo.BPB_BytsPerSec);
	//printf("Data area starts at byte: %i\n", dataArea);
	//Do some math to find where root directory is
	int rootDirData = ((BootInfo.BPB_RootClus - 2) * BootInfo.BPB_SecPerClus) * BootInfo.BPB_BytsPerSec + dataArea;
	//printf("Root directory starts at: %i\n", rootDirData);
	//Read directory 
	//Loop through directory and find number of entries
	int i = rootDirData;
	int numEntries = 0; //WILL INCLUDE "DELETED" FILES
	while (fatDisk[i] != 0) {
		numEntries++;
		i += 32;
	}
	DirEntry** RootDir = malloc(sizeof(DirEntry*) * numEntries);
	i = rootDirData;
	int j = 0;
	while (fatDisk[i] != 0) {
		RootDir[j] = malloc(sizeof(DirEntry));
		memcpy(&RootDir[j]->DIR_Name, &fatDisk[i], 11);
		RootDir[j]->DIR_Attr = fatDisk[i + 11];
		RootDir[j]->DIR_FileSize = fatDisk[i + 28] + (fatDisk[i + 29] << 8) + (fatDisk[i + 30] << 16) + (fatDisk[i + 31] << 24);
		RootDir[j]->DIR_FstClusLO = fatDisk[i + 26] + (fatDisk[i + 27] << 8);
		RootDir[j]->DIR_FstClusHI = fatDisk[i + 20] + (fatDisk[i + 21] << 8);
		//int startingCluster = fatDisk[i + 26] + (fatDisk[i + 27] << 8) + (fatDisk[i + 20] << 16) + (fatDisk[i + 21] << 24);
		j++;
		i += 32;
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
				printf("%.*s/ (size = %i, starting cluster = %i)\n", 11 - amountToChop - amountToChopExt, RootDir[i]->DIR_Name, RootDir[i]->DIR_FileSize, startingCluster);
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

	if (recContigChosen) {
		//Recover a contiguously allocated file when sha1 has not been supplied
		//format fileToRecover to be in structure of directory entry
		char recoverFile[11];
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
			if (strncmp(RootDir[i]->DIR_Name + 1, recoverFile + 1,10) == 0) {
				//printf("File found\n");
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
		if (sha1Chosen) {
			_Bool matchFound = false;
			for (int i = 0; i < numEntries; ++i) {
				if (strncmp(RootDir[i]->DIR_Name + 1, recoverFile + 1, 10) == 0) {
					//Found an ambiguous file
					//Compute size of bytes to read.
					int clustersOcc = (RootDir[i]->DIR_FileSize + ((BootInfo.BPB_BytsPerSec * BootInfo.BPB_SecPerClus) -1 )) / (BootInfo.BPB_BytsPerSec * BootInfo.BPB_SecPerClus); //is this needed?
					char sha1Sum[20];
					//Calculate where cluster data is in bytes
					int startClus = RootDir[i]->DIR_FstClusLO + (RootDir[i]->DIR_FstClusHI << 16);
					SHA1(fatDisk + ((startClus - 2) * BootInfo.BPB_SecPerClus * BootInfo.BPB_BytsPerSec) + dataArea, RootDir[i]->DIR_FileSize, sha1Sum);

					char sha1ValHex[20];
					//Decode ascii hex string into hex array of 2 ascii letters for 1 byte
					for (int i = 0, k = 0; k < 40; k += 2, ++i) {
						sha1ValHex[i] = hex2byte(sha1Value + k);
					}
					//Check if entry is the one specified
					//for (int j = 0; j < 20; ++j) {
					//	if (sha1Sum[j] != sha1ValHex[j]) {
					//		printf("Orig: %c Decoded %c\n", sha1Sum[j], sha1ValHex[j]);
					//		matchFound = false;
					//	}
					//}
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

		//Change e5 to first letter
		fatDisk[rootDirData + (32*indexOfFileToRecover)] = fileToRecover[0];
		//File found now, recover it
		//To recover a small file that spans the size of one cluster change starting cluster to EOC
		//First fat is after reserved data (every other fat is nth FAT(FAT SIZE) + 1st FAT start plus
		int FirstFatStart = (BootInfo.BPB_RsvdSecCnt * BootInfo.BPB_BytsPerSec);
		//First 12 bytes are reserved? Multiply cluster number by 4 to get byte location in FAT
		int startingCluster = RootDir[indexOfFileToRecover]->DIR_FstClusLO + (RootDir[indexOfFileToRecover]->DIR_FstClusHI << 16);

		if(startingCluster != 0){
		//If there is a starting cluster, update FATs, if not dont update and return file update successful. DIR entry updated
		
		int clustersOccupied = (RootDir[indexOfFileToRecover]->DIR_FileSize + ((BootInfo.BPB_BytsPerSec * BootInfo.BPB_SecPerClus) - 1)) / (BootInfo.BPB_BytsPerSec * BootInfo.BPB_SecPerClus);
		
		//IF FILE IS ONLY 1 CLUSTER LONG DO THIS
			if (clustersOccupied == 1) {
				int bytesToChange = startingCluster * 4;
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
						if(k+1 == clustersOccupied)
							writeIntToDisk(268435455, fatDisk, (j * BootInfo.BPB_FATSz32 * BootInfo.BPB_BytsPerSec) + FirstFatStart + (startingCluster+k) * 4);
						else
							writeIntToDisk(startingCluster + k+1, fatDisk, (j* BootInfo.BPB_FATSz32* BootInfo.BPB_BytsPerSec) + FirstFatStart + (startingCluster+k)*4);
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


	return 0;
}