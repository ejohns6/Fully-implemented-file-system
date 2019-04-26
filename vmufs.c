#define FUSE_USE_VERSION 29

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>



static int AmountOfFiles = 0;

struct ROOTStruct{
	short int StartingFatBlock;
	short int SizeOfFat;
	short int LastBlockOfDirectory;
	short int SizeOfDirectory;
	short int SizeOfUserBlocks;
}ROOT = {};

struct FileInfo{
	unsigned char FileType;
	unsigned char CopyProtec;
	unsigned short int FirstBlock;
	char FileNameWithSlash[14];
	//char FileName[13];
	unsigned int Cent;
	unsigned int Year;
	unsigned int Month;
	unsigned int Day;
	unsigned int Hour;
	unsigned int Min;
	unsigned int Sec;
	unsigned int DayOfWeek;
	unsigned short int FileSizeBlock;
	size_t Offset;
    unsigned long A_sec;
	unsigned long M_sec;
	uid_t _uid;
	gid_t _gid;
}FileInfoArray[200] = {	};

static unsigned short int FAT[256];


static int _getattr(const char *path, struct stat *stbuf){
    printf("|-----getattr-----|\n");
	int res = 0;
	printf("Getting attributes of %s requested\n", path);
	memset(stbuf, 0 , sizeof(struct stat));

	if(strcmp(path, "/") == 0){
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		stbuf->st_atim.tv_sec = time(NULL);
        stbuf->st_mtim.tv_sec = time(NULL);
        stbuf->st_mtim.tv_sec = time(NULL);
		return res;
	}else{
		for(int amount_open = 0; amount_open < AmountOfFiles; amount_open++){
			if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) == 0){
				stbuf->st_mode = S_IFREG | 0666;
				stbuf->st_nlink = 1;
				unsigned short int FileSizeBlockCounterThroughFAT = 0;
				unsigned short int CurrentBlock = FileInfoArray[amount_open].FirstBlock;
				printf("First Block: %d\n", CurrentBlock);
				while(CurrentBlock != 0xFFFA){ // makes the size equal to the amount of block from the FAT till FFFA time 512
					CurrentBlock = FAT[CurrentBlock];
					printf("Current Block: %d\n", CurrentBlock);
					FileSizeBlockCounterThroughFAT++;
				}
				if(FileInfoArray[amount_open].FileSizeBlock != FileSizeBlockCounterThroughFAT){
					FileInfoArray[amount_open].FileSizeBlock = FileSizeBlockCounterThroughFAT;
					printf("The FAT and the file description disagree with each other\n");
				}
				stbuf->st_size = FileSizeBlockCounterThroughFAT  * 512;
				printf("St_size: %lu\n",stbuf->st_size);
				stbuf->st_atim.tv_sec = FileInfoArray[amount_open].A_sec;
                stbuf->st_mtim.tv_sec = FileInfoArray[amount_open].M_sec;
                stbuf->st_ctim.tv_sec = FileInfoArray[amount_open].M_sec;
                stbuf->st_gid = FileInfoArray[amount_open]._gid;
                stbuf->st_uid = FileInfoArray[amount_open]._uid;
				return res;
			}
		}
		printf("Did not find file\n");
		return(-ENOENT);
	}

	return res;
}

static int _readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
    printf("|-----readdir-----|\n");
	printf("--> Getting the List of Files of %s\n", path);
	(void) offset;
	(void) fi;
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	for(int amount_read = 0; amount_read < AmountOfFiles; amount_read++){
		filler(buf, FileInfoArray[amount_read].FileNameWithSlash + 1, NULL, 0);
	}
	return(0);

}

static int _open(const char *path, struct fuse_file_info *fi){
    printf("|-----open-----|\n");
	printf("Trying to open \"%s\"\n", path);
	//checks to see if the file already exists
	for(int amount_open = 0; amount_open < AmountOfFiles; amount_open++){
		if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) ==  0){
			//if ((fi->flags & 3) != O_RDONLY){return -EACCES; }
			return(0);
		}
	}
	printf("Error: Open Failure\n");
	return(-ENOENT);
}

static int _read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	printf("|-----read-----|\n");
	printf("Trying to read \"%s\"\n", path);
	(void) fi;
	size_t len = 0;
	for(int amount_open = 0; amount_open < AmountOfFiles; amount_open++){
		if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) == 0){//looks for the file called
			FILE *img;
			img = fopen((const char * restrict)fuse_get_context()->private_data, "r");
			if(img == NULL){//Checks to see if the file is open
				printf("Error File not opened\n");
				return(-ENOENT);
			}
			//offset = FileInfoArray[amount_open].Offset;
			int tempOffset = offset;
			printf("Offset: %lu\n", offset);
			int FileSizeBlock = FileInfoArray[amount_open].FileSizeBlock;

			char selectedTextTemp[FileSizeBlock * 512];
			printf("File Size Block: %d\n", FileSizeBlock);
			int CurrentBlock = FileInfoArray[amount_open].FirstBlock;
			char readingIn;
			int CurrentReadingOffset = (CurrentBlock * 512);
			printf("CurrentBlock: %d\n", CurrentBlock); // reads each char from a block and then will move one to the next
			for(int BlockCounter = 0; BlockCounter < FileSizeBlock; BlockCounter++){//block from the FAT till the write amount of blocks were made

				fseek(img, CurrentReadingOffset, SEEK_SET);//moves the img pointer to the correct size
				for(int ReadCounter = 0; ReadCounter < 512; ReadCounter++){
					readingIn = fgetc(img);
					selectedTextTemp[BlockCounter * 512 + ReadCounter] = readingIn;
					if(tempOffset == 0){
						len += 1;
					}else{
						tempOffset -= 1;
					}
					//printf("%02x", readingIn);
				}
				printf("\n");
				CurrentBlock = FAT[CurrentBlock];
				printf("CurrentBlock: %d\n", CurrentBlock);
				CurrentReadingOffset = CurrentBlock * 512;
			}
			printf("tempOffset: %d\n", tempOffset);
			printf("len: %lu\n", len);
			fclose(img);
			size = size - offset;
			if(offset < len){
				if(offset + size > len){// if the offset and the size is more then the length then the size then made equal to len
					size = len - offset;
				}
				printf("Size: %ld\n", size);
				memcpy(buf, selectedTextTemp + offset, size);
			}else{// If the offset is more then there is no point of memcpy
				size = 0;

			}
			return(size);
		}
	}
	return(-ENOENT);
}


//Rename: need to change the img file
static int _rename(const char* from, const char* to){
	printf("|-----rename-----|\n");
	printf("Trying to rename from \"%s\"\n", from);
	printf("From Length: %lu\n", strlen(from));
	printf("Trying to rename to  \"%s\"\n",	to);
	printf("To Length: %lu\n", strlen(to));
	for(int i = 1; i < strlen(to); i++){
        if(to[i] == '/'){
            printf("Rename is invalid: contains / in the new file name");
            return(-EINVAL);
        }
	}
    if(strchr(to + 1, '/') || strcmp(to, ".")  == 0|| strcmp(to, "..")  == 0 || strlen(to) > 13){
        printf("/ might be in: %s\n", strchr(to + 1, '/'));
        printf(". might be in: %s\n", strchr(to, '.'));
        printf("Name trying to rename is invalid\n");
        return(-EINVAL);
    }
	for(int amount_open = 0; amount_open < AmountOfFiles; amount_open++){ //checks to see if the file already exists
		if(strcmp(from, FileInfoArray[amount_open].FileNameWithSlash) ==  0){
            FILE *img;
			img = fopen((const char * restrict)fuse_get_context()->private_data, "r+b");
			if(img == NULL){//Checks to see if the file is open
				printf("Error File not opened\n");
				return(-ENOENT);
			}
            memset(FileInfoArray[amount_open].FileNameWithSlash, 0x0, 13);
            memcpy(FileInfoArray[amount_open].FileNameWithSlash, to, strlen(to));
            unsigned char NewName[12] = { 0x0 };
            memcpy(NewName, to + 1, strlen(to) - 1);
            printf("NewName: %s\n", NewName);


            int CurrBlockFileDir = ROOT.LastBlockOfDirectory;
            int CurrentOffset = CurrBlockFileDir * 512;
            printf("CurrBlockFileDir: %d\n", CurrBlockFileDir);
            printf("CurrentOffset: %d\n", CurrentOffset);
            unsigned char tempChar;
            for(int i = 0; i < ROOT.SizeOfDirectory; i++){
                fseek(img, CurrentOffset, SEEK_SET);
                for(int j = 0; j < 512; j+=32){
                    fseek(img, CurrentOffset + j, SEEK_SET);
                    tempChar = fgetc(img);
                    //printf("TempChar: %02x\n", tempChar);
                    if(tempChar != 0x00){
                        fseek(img, CurrentOffset + j + 4, SEEK_SET);
                        char checkingOldName[12 + 1] = { 0x0 };

                        fgets(checkingOldName, 13, img);
                        //printf("Checking Old Name: %s\n", checkingOldName);
                        if(strcmp(checkingOldName, from + 1) == 0){
                            fseek(img, CurrentOffset + j + 4, SEEK_SET);
                            //strcpy(NewName, to);
                            fwrite(FileInfoArray[amount_open].FileNameWithSlash + 1, 1, 12, img);
                            printf("Found the old file in img\n");
                            fclose(img);
                            return(0);
                        }
                    }
                }
                CurrBlockFileDir = FAT[CurrBlockFileDir];
                CurrentOffset = CurrBlockFileDir * 512;
            }
            fclose(img);
            printf("Could not find the name in the img file\n");
            return(-ENOENT);
		}
	}
	printf("Error: File Does not exsist");
	return(-ENOENT);
}

//mkdir: done?
static int _mkdir(const char* path, mode_t mode){
	printf("|-----mkdir-----|\n");
	return(0);
}


//Turncate: need to affect img file
static int _truncate(const char* path, off_t size){
	printf("|-----truncate-----|\n");
	printf("Path: %s\n", path);
	printf("Size: %lu\n", size);
	unsigned char NukeString[512] = { 0x00 };
	for(int amount_open = 0; amount_open < AmountOfFiles; amount_open++){ //checks to see if the file already exists
		if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) ==  0 && size == 0){
			//nuke incoming
			int FATOffset = ROOT.StartingFatBlock;
			FILE *img;
			img = fopen((const char * restrict)fuse_get_context()->private_data, "r+b");
			if(img == NULL){//Checks to see if the file is open
				printf("Error: IMG File not opened\n");
				return(-ENOENT);
			}
			int FATfileOffset = FileInfoArray[amount_open].FirstBlock;
            printf("First Block: %d\n", FATfileOffset);
			int FATArrayClear;
			FileInfoArray[amount_open].FirstBlock = 0xfffa;
			unsigned char freeMarked[2];
            freeMarked[0] = 0xfc;
            freeMarked[1] = 0xff;
			//need to check
			for(int i = 0; i < FileInfoArray[amount_open].FileSizeBlock; i++){
                fseek(img, FATfileOffset * 512, SEEK_SET);
                fwrite(NukeString, 1, 512, img);
				fseek(img, FATOffset * 512 + FATfileOffset * 2, SEEK_SET);
                fwrite(freeMarked, 1, 2, img);//
				FATArrayClear = FATfileOffset;
				FATfileOffset = FAT[FATfileOffset];
				printf("FAT where it points: %d\n", FATfileOffset);
				FAT[FATArrayClear] = 0xfffc;
			}
			FileInfoArray[amount_open].FileSizeBlock = 0;
            fclose(img);
            printf("Finished\n");
			return(0);
		}else if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) ==  0 && size <= FileInfoArray[amount_open].FileSizeBlock * 512){
		//For when size is not zero but is shortened
			//nuke incoming
			int FATOffset = ROOT.StartingFatBlock;
			FILE *img;
			img = fopen((const char * restrict)fuse_get_context()->private_data, "r+b");
			if(img == NULL){//Checks to see if the file is open
				printf("Error: IMG File not opened\n");
				return(-ENOENT);
			}
			int FATfileOffset = FileInfoArray[amount_open].FirstBlock;
            printf("First Block: %d\n", FATfileOffset);
			int FATArrayClear;
			int tempSize = size;
			unsigned char freeMarked[2];
            freeMarked[0] = 0xfc;
            freeMarked[1] = 0xff;
			//need to check
			for(int i = 0; i < FileInfoArray[amount_open].FileSizeBlock; i++){
				if(tempSize == 0){
                    fseek(img, FATfileOffset * 512, SEEK_SET);
                    fwrite(NukeString, 1, 512, img);
                    fseek(img, FATOffset * 512 + FATfileOffset * 2, SEEK_SET);
                    fwrite(freeMarked, 1, 2, img);//
                    FATArrayClear = FATfileOffset;
                    FATfileOffset = FAT[FATfileOffset];
                    printf("FAT where it points: %d\n", FATfileOffset);
                    FAT[FATArrayClear] = 0xfffc;
                }else{
                    if(tempSize >= 512){
                        tempSize -= 512;
                    }else{
                        fseek(img, FATfileOffset * 512 + tempSize, SEEK_SET);
                        fwrite(NukeString, 1, (512 - tempSize), img);
                        fseek(img, FATOffset * 512 + FATfileOffset * 2, SEEK_SET);
                        freeMarked[0] = 0xfa;
                        fwrite(freeMarked, 1, 2, img);
                        freeMarked[0] =
                        FATArrayClear = FATfileOffset;
                        FATfileOffset = FAT[FATfileOffset];
                        printf("FAT where it points: %d\n", FATfileOffset);
                        FAT[FATArrayClear] = 0xfffa;
                        tempSize -= tempSize;
                    }
                }
			}
			FileInfoArray[amount_open].FileSizeBlock = 0;
            fclose(img);
            printf("Finished\n");
			return(0);
		}else if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) ==  0 && size <= FileInfoArray[amount_open].FileSizeBlock * 512){
		//For when size is not zero but is extened
            FILE *img;
			img = fopen((const char * restrict)fuse_get_context()->private_data, "r+b");
			if(img == NULL){//Checks to see if the file is open
				printf("Error: IMG File not opened\n");
				return(-ENOENT);
			}

            int newBlocks = 0;
            int tempNewSize = size - FileInfoArray[amount_open].FileSizeBlock;
            while(tempNewSize != 0){
                if(tempNewSize > 512){
                    newBlocks += 1;
                    tempNewSize -= 512;
                }else{
                    newBlocks += 1;
                    tempNewSize  = 0;
                }

            }
            printf("NewBlocks: %d\n", newBlocks);
            printf("tempNewSize: %d\n", tempNewSize);
            FileInfoArray[amount_open].FileSizeBlock += newBlocks;

            //edits img description of Files Block Size
            int CurrBlockFileDir = ROOT.LastBlockOfDirectory;
            int CurrentOffset = CurrBlockFileDir * 512;
            unsigned char tempChar;
            for(int i = 0; i < ROOT.SizeOfDirectory; i++){
                fseek(img, CurrentOffset, SEEK_SET);
                for(int j = 0; j < 512; j+=32){
                    fseek(img, CurrentOffset + j, SEEK_SET);
                    tempChar = fgetc(img);
                    //printf("TempChar: %02x\n", tempChar);
                    if(tempChar != 0x00){
                        fseek(img, CurrentOffset + j + 4, SEEK_SET);
                        char checkingOldName[12 + 1] = { 0x0 };
                        fgets(checkingOldName, 13, img);
                        if(strcmp(checkingOldName, path + 1) == 0){
                            fseek(img, CurrentOffset + j + 24, SEEK_SET);
                            char tempCharsTwo[2];
                            tempCharsTwo[1] = FileInfoArray[amount_open].FileSizeBlock;
                            tempCharsTwo[0] = (FileInfoArray[amount_open].FileSizeBlock >> 8);
                            printf("j = %d\n", FileInfoArray[amount_open].FileSizeBlock);
                            printf("File Size Block = %s\n", tempCharsTwo);
                            putc(tempCharsTwo[1], img);
                            putc(tempCharsTwo[0], img);
                            printf("Found the old file in img\n");
                        }
                    }
                }
                CurrBlockFileDir = FAT[CurrBlockFileDir];
                CurrentOffset = CurrBlockFileDir * 512;
            }

            unsigned short int currentBlock = FileInfoArray[amount_open].FirstBlock;
            unsigned short int lastBlock;
            int FATOffset = ROOT.StartingFatBlock * 512;
            unsigned char FATSlotPointer[2];
            //points x new blocks to be add onto the files fat path
            printf("First Block: %d\n", currentBlock);
            for(int i = 0; i < FileInfoArray[amount_open].FileSizeBlock; i++){//need to check logic
                int FATPointerAssigned = 0;
                if(FileInfoArray[amount_open].FirstBlock == 0xfffa){ // should only happen if first block is not done
                    printf("R1\n");
                    for(unsigned short int j = 0; j < 200; j++){
                        if(FAT[j] == 0xfffc){
                            FileInfoArray[amount_open].FirstBlock = j;
                            FATSlotPointer[1] = j;
                            FATSlotPointer[0] = (j >> 8);

                            // Updates file description for first block in the img file
                            CurrBlockFileDir = ROOT.LastBlockOfDirectory;
                            CurrentOffset = CurrBlockFileDir * 512;
                            printf("CurrBlockFileDir: %d\n", CurrBlockFileDir);
                            printf("CurrentOffset: %d\n", CurrentOffset);
                            for(int i = 0; i < ROOT.SizeOfDirectory; i++){
                                fseek(img, CurrentOffset, SEEK_SET);
                                for(int j = 0; j < 512; j+=32){
                                    fseek(img, CurrentOffset + j, SEEK_SET);
                                    tempChar = fgetc(img);
                                    //printf("TempChar: %02x\n", tempChar);
                                    if(tempChar != 0x00){
                                        fseek(img, CurrentOffset + j + 4, SEEK_SET);
                                        char checkingOldName[12 + 1] = { 0x0 };

                                        fgets(checkingOldName, 13, img);
                                        //printf("Checking Old Name: %s\n", checkingOldName);
                                        if(strcmp(checkingOldName, path + 1) == 0){
                                            fseek(img, CurrentOffset + j + 2, SEEK_SET);
                                            printf("j = %d\n", j);
                                            printf("FATSlotPointer = %s\n", FATSlotPointer);
                                            putc(FATSlotPointer[1], img);
                                            putc(FATSlotPointer[0], img);
                                            printf("Found the old file in img\n");
                                        }
                                    }
                                }
                                CurrBlockFileDir = FAT[CurrBlockFileDir];
                                CurrentOffset = CurrBlockFileDir * 512;
                            }


                            fseek(img, FATOffset + j * 2, SEEK_SET);
                            FATSlotPointer[1] = 0xfa;
                            FATSlotPointer[0] = 0xff;
                            putc(FATSlotPointer[1], img);
                            putc(FATSlotPointer[0], img);
                            FAT[j] = 0xfffa;
                            lastBlock = j;
                            currentBlock = 0xfffa;
                            FATPointerAssigned = 1;
                            break;
                        }
                    }
                    if(FATPointerAssigned == 0){
                        fclose(img);
                        return(-ENOSPC);
                    }
                }else if(currentBlock == 0xfffa){ // add block to list in fat
                    printf("R2\n");
                    for(unsigned short int j = 0; j < 200; j++){
                        if(FAT[j] == 0xfffc){
                            FAT[lastBlock] = j;
                            fseek(img, FATOffset + lastBlock * 2, SEEK_SET);
                            FATSlotPointer[1] = j;
                            FATSlotPointer[0] = (j >> 8);
                            printf("j = %d\n", j);
                            printf("FATSlotPointer = %s\n", FATSlotPointer);
                            putc(FATSlotPointer[1], img);
                            putc(FATSlotPointer[0], img);
                            fseek(img, FATOffset + j * 2, SEEK_SET);
                            FATSlotPointer[1] = 0xfa;
                            FATSlotPointer[0] = 0xff;
                            printf("FATSlotPointer = %s\n", FATSlotPointer);
                            putc(FATSlotPointer[1], img);
                            putc(FATSlotPointer[0], img);
                            FAT[j] = 0xfffa;
                            lastBlock = j;
                            currentBlock = 0xfffa;
                            FATPointerAssigned = 1;
                            break;
                        }
                    }
                    if(FATPointerAssigned == 0){
                        fclose(img);
                        return(-ENOSPC);
                    }
                }else{
                    printf("R3\n");
                    lastBlock = currentBlock;
                    currentBlock = FAT[currentBlock];


                }
                printf("Assigned Block Number: %d\n", i+1);
            }
		}
	}
	return(-ENOENT);
}


static int _write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	printf("|-----write-----|\n");
	printf("Writing to \"%s\"\n", path);
	printf("Size: %lu\n", size);
	printf("Offset: %lu\n", offset);
    (void) fi;
	size_t len = 0;
	for(int amount_open = 0; amount_open < AmountOfFiles; amount_open++){
		if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) == 0){//looks for the file called
			printf("The path: %s\n", FileInfoArray[amount_open].FileNameWithSlash);
			FILE *img;
			img = fopen((const char * restrict)fuse_get_context()->private_data, "r+b");
			if(img == NULL){//Checks to see if the file is open
				printf("Error File not opened\n");
				return(-ENOENT);
			}
			unsigned short int currentBlock;
			unsigned char tempChar;
			int FATOffset = ROOT.StartingFatBlock * 512;
            if(FileInfoArray[amount_open].FileSizeBlock * 512 < size + offset){
                int newBlocks = 0;
                int tempToGetNewBlocks = size;
                int tempOffestToGetNewBlocks = offset%512;
                if(tempOffestToGetNewBlocks != 0){ //This is so we can continue writing in a block
                    tempOffestToGetNewBlocks -= (512 -tempOffestToGetNewBlocks); //The rest of the space left in a file
                }
                while(tempToGetNewBlocks != 0){
                    if(tempToGetNewBlocks > 512){
                        newBlocks += 1;
                        tempToGetNewBlocks -= 512;
                    }else{
                        newBlocks += 1;
                        tempToGetNewBlocks  = 0;
                    }

                }
                printf("NewBlocks: %d\n", newBlocks);
                printf("tempToGetNewBlocks: %d\n", tempToGetNewBlocks);
                FileInfoArray[amount_open].FileSizeBlock += newBlocks;

                //edits img description of Files Block Size
                int CurrBlockFileDir = ROOT.LastBlockOfDirectory;
                int CurrentOffset = CurrBlockFileDir * 512;

                for(int i = 0; i < ROOT.SizeOfDirectory; i++){
                    fseek(img, CurrentOffset, SEEK_SET);
                    for(int j = 0; j < 512; j+=32){
                        fseek(img, CurrentOffset + j, SEEK_SET);
                        tempChar = fgetc(img);
                        //printf("TempChar: %02x\n", tempChar);
                        if(tempChar != 0x00){
                            fseek(img, CurrentOffset + j + 4, SEEK_SET);
                            char checkingOldName[12 + 1] = { 0x0 };
                            fgets(checkingOldName, 13, img);
                            if(strcmp(checkingOldName, path + 1) == 0){
                                fseek(img, CurrentOffset + j + 24, SEEK_SET);
                                char tempCharsTwo[2];
                                tempCharsTwo[1] = FileInfoArray[amount_open].FileSizeBlock;
                                tempCharsTwo[0] = (FileInfoArray[amount_open].FileSizeBlock >> 8);
                                printf("j = %d\n", FileInfoArray[amount_open].FileSizeBlock);
                                printf("File Size Block = %s\n", tempCharsTwo);
                                putc(tempCharsTwo[1], img);
                                putc(tempCharsTwo[0], img);
                                printf("Found the old file in img\n");
                            }
                        }
                    }
                    CurrBlockFileDir = FAT[CurrBlockFileDir];
                    CurrentOffset = CurrBlockFileDir * 512;
                }


                currentBlock = FileInfoArray[amount_open].FirstBlock;
                unsigned short int lastBlock;

                unsigned char FATSlotPointer[2];
                //points x new blocks to be add onto the files fat path
                printf("First Block: %d\n", currentBlock);
                for(int i = 0; i < FileInfoArray[amount_open].FileSizeBlock; i++){//need to check logic
                    int FATPointerAssigned = 0;
                    if(FileInfoArray[amount_open].FirstBlock == 0xfffa){ // should only happen if first block is not done
                        //printf("R1\n");
                        for(unsigned short int j = 0; j < 200; j++){
                            if(FAT[j] == 0xfffc){
                                FileInfoArray[amount_open].FirstBlock = j;
                                FATSlotPointer[1] = j;
                                FATSlotPointer[0] = (j >> 8);

                                // Updates file description for first block in the img file
                                CurrBlockFileDir = ROOT.LastBlockOfDirectory;
                                CurrentOffset = CurrBlockFileDir * 512;
                                printf("CurrBlockFileDir: %d\n", CurrBlockFileDir);
                                printf("CurrentOffset: %d\n", CurrentOffset);
                                for(int i = 0; i < ROOT.SizeOfDirectory; i++){
                                    fseek(img, CurrentOffset, SEEK_SET);
                                    for(int j = 0; j < 512; j+=32){
                                        fseek(img, CurrentOffset + j, SEEK_SET);
                                        tempChar = fgetc(img);
                                        //printf("TempChar: %02x\n", tempChar);
                                        if(tempChar != 0x00){
                                            fseek(img, CurrentOffset + j + 4, SEEK_SET);
                                            char checkingOldName[12 + 1] = { 0x0 };

                                            fgets(checkingOldName, 13, img);
                                            //printf("Checking Old Name: %s\n", checkingOldName);
                                            if(strcmp(checkingOldName, path + 1) == 0){
                                                fseek(img, CurrentOffset + j + 2, SEEK_SET);
                                                //printf("j = %d\n", j);
                                                //printf("FATSlotPointer = %s\n", FATSlotPointer);
                                                putc(FATSlotPointer[1], img);
                                                putc(FATSlotPointer[0], img);
                                                printf("Found the old file in img\n");
                                            }
                                        }
                                    }
                                    CurrBlockFileDir = FAT[CurrBlockFileDir];
                                    CurrentOffset = CurrBlockFileDir * 512;
                                }


                                fseek(img, FATOffset + j * 2, SEEK_SET);
                                FATSlotPointer[1] = 0xfa;
                                FATSlotPointer[0] = 0xff;
                                putc(FATSlotPointer[1], img);
                                putc(FATSlotPointer[0], img);
                                FAT[j] = 0xfffa;
                                lastBlock = j;
                                currentBlock = 0xfffa;
                                FATPointerAssigned = 1;
                                break;
                            }
                        }
                        if(FATPointerAssigned == 0){
                            fclose(img);
                            for(int slot = 0; slot < 200; slot++){
                                printf("FAT[%d]: %d\n", slot, FAT[slot]);
                            }
                            printf("Could not find slot\n");
                            return(-ENOSPC);
                        }
                    }else if(currentBlock == 0xfffa){ // add block to list in fat
                        //printf("R2\n");
                        for(unsigned short int j = 0; j < 200; j++){
                            if(FAT[j] == 0xfffc){
                                FAT[lastBlock] = j;
                                fseek(img, FATOffset + lastBlock * 2, SEEK_SET);
                                FATSlotPointer[1] = j;
                                FATSlotPointer[0] = (j >> 8);
                                //printf("j = %d\n", j);
                                //printf("FATSlotPointer = %s\n", FATSlotPointer);
                                putc(FATSlotPointer[1], img);
                                putc(FATSlotPointer[0], img);
                                fseek(img, FATOffset + j * 2, SEEK_SET);
                                FATSlotPointer[1] = 0xfa;
                                FATSlotPointer[0] = 0xff;
                                //printf("FATSlotPointer = %s\n", FATSlotPointer);
                                putc(FATSlotPointer[1], img);
                                putc(FATSlotPointer[0], img);
                                FAT[j] = 0xfffa;
                                lastBlock = j;
                                currentBlock = 0xfffa;
                                FATPointerAssigned = 1;
                                break;
                            }
                        }
                        if(FATPointerAssigned == 0){
                            fclose(img);
                            for(int slot = 0; slot < 200; slot++){
                                printf("FAT[%d]: %d\n", slot, FAT[slot]);
                            }
                            printf("Could not find slot\n");
                            return(-ENOSPC);
                        }
                    }else{
                        //printf("R3\n");
                        lastBlock = currentBlock;
                        currentBlock = FAT[currentBlock];


                    }
                    printf("Assigned Block Number: %d\n", i+1);
                }
            }
            int tempOffset = 0;
			printf("Offset: %lu\n", offset);
			int FileSizeBlock = FileInfoArray[amount_open].FileSizeBlock;




            int BlocksWriten = 0;
			printf("File Size Block: %d\n", FileSizeBlock);
			currentBlock = FileInfoArray[amount_open].FirstBlock;
			//char readingIn;
			int CurrentReadingOffset = (currentBlock * 512);
			printf("CurrentBlock: %d\n", currentBlock); // reads each char from a block and then will move one to the next
			for(int bufPointer = 0; bufPointer < size;){
                fseek(img, CurrentReadingOffset, SEEK_SET);//moves the img pointer to the correct size
				for(int i = 0; i < 512; i++){
                    if(tempOffset == offset){
                        if(bufPointer < size){
                            tempChar = buf[bufPointer];
                            putc(tempChar, img);
                            bufPointer += 1;
                            len += 1;
                        }else{
                            break;
                        }
                    }else{
                        tempOffset += 1; //moves pointer
                        tempChar = fgetc(img);
                    }
				}
				currentBlock = FAT[currentBlock];
				printf("CurrentBlockWriting: %d\n", currentBlock);
				CurrentReadingOffset = currentBlock * 512;
			}
			/*
			for(int BlockCounter = 0; BlockCounter < FileSizeBlock; BlockCounter++){//block from the FAT till the write amount of blocks were made

				if(tempOffset == 0 && len >= 512){
                    char selectedTextTemp[512];
                    memcpy(selectedTextTemp, buf + BlocksWriten * 512, 512);
                    fwrite(selectedTextTemp, 1, 512, img);
                    len -= 512;
                    BlocksWriten += 1;
				}else if(tempOffset == 0 && len < 512){
                    char selectedTextTemp[len];
                    memcpy(selectedTextTemp, buf + BlocksWriten * 512, len);
                    fwrite(selectedTextTemp, 1, len, img);
                    for(int slot = len; slot < 512; slot++){ //zeros out/pads
                        tempChar = 0x00;
                        putc(tempChar, img);
                    }
                    len -= len;
                    BlocksWriten += 1;
				}else{
                    tempOffset -= 512;
				}
				currentBlock = FAT[currentBlock];
				printf("CurrentBlockWriting: %d\n", currentBlock);
				CurrentReadingOffset = currentBlock * 512;
			}
            */

			printf("tempOffset: %d\n", tempOffset);
			printf("len: %lu\n", len);

			if(size - len == 0){
                printf("Success\n");
                fclose(img);
				return(size);
            }else{ //shouldn't really get here tbh more of just a just in case
                printf("Size: %lu\n", size);
                printf("BlocksWriten: %d\n", BlocksWriten);
                printf("BlocksWriten * 512: %d\n", (BlocksWriten * 512));
                printf("Size - BlocksWriten * 512 = %lu\n", (size - BlocksWriten * 512));
                int FATfileOffset = FileInfoArray[amount_open].FirstBlock;
                printf("First Block: %d\n", FATfileOffset);
                int FATArrayClear;
                FileInfoArray[amount_open].FirstBlock = 0xfffa;
                unsigned char freeMarked[2];
                freeMarked[0] = 0xfc;
                freeMarked[1] = 0xff;
                //need to check
                for(int i = 0; i < FileInfoArray[amount_open].FileSizeBlock; i++){
                    fseek(img, FATOffset * 512 + FATfileOffset * 2, SEEK_SET);
                    fwrite(freeMarked, 1, 2, img);// Deletes all the FAT pointer
                    FATArrayClear = FATfileOffset;
                    FATfileOffset = FAT[FATfileOffset];
                    printf("FAT where it points for deletion: %d\n", FATfileOffset);
                    FAT[FATArrayClear] = 0xfffc;
                }
                FileInfoArray[amount_open].FileSizeBlock = 0;
                fclose(img);
                return(size - len);
			}
		}
	}
	return(-ENOENT);
}


//Need to affect img file
static int _utimens(const char* path, const struct timespec ts[2]){
    printf("|-----utimens-----|\n");
    printf("Path: %s\n", path);
    for(int amount_open = 0; amount_open < AmountOfFiles; amount_open++){
		if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) ==  0){
			//if ((fi->flags & 3) != O_RDONLY){return -EACCES; }
			FileInfoArray[amount_open].A_sec = ts[0].tv_sec;
			FileInfoArray[amount_open].M_sec = ts[1].tv_sec;
			time_t current_time;
            time(&current_time);
            struct tm *current_time_struct;
            current_time_struct = localtime(&current_time);
            unsigned char BCD[8];
            //century
            int cen = (current_time_struct->tm_year/100 + 19);
            FileInfoArray[amount_open].Cent = cen;
            BCD[0] = ((cen/10) << 4) | (cen%10);
            //year
            int year = (current_time_struct->tm_year%100);
            FileInfoArray[amount_open].Year = year;
            BCD[1] = ((year/10) << 4) | (year%10);
            //Month
            int Month = (current_time_struct->tm_mon + 1);
            FileInfoArray[amount_open].Month = Month;
            BCD[2] = ((Month/10) << 4) | (Month%10);
            //Day
            int Day = (current_time_struct->tm_mday);
            FileInfoArray[amount_open].Day = Day;
            BCD[3] = ((Day/10) << 4) | (Day%10);
            //Hour
            int Hour = (current_time_struct->tm_hour);
            if(Hour == 0){
                Hour = 6;
            }else{
                Hour = Hour -1;
            }
            FileInfoArray[amount_open].Hour = Hour;
            BCD[4] = ((Hour/10) << 4) | (Hour%10);
            //Minute
            int Min = (current_time_struct->tm_min);
            FileInfoArray[amount_open].Min = Min;
            BCD[5] = ((Min/10) << 4) | (Min%10);
            //Second
            int Sec = (current_time_struct->tm_sec);
            FileInfoArray[amount_open].Sec = Sec;
            BCD[6] = ((Sec/10) << 4) | (Sec%10);
            //Day of week
            int whatDay = (current_time_struct->tm_wday);
            if(whatDay == 0){
                whatDay = 6;
            }else{
                whatDay -= 1;
            }
            FileInfoArray[amount_open].DayOfWeek = whatDay;
            BCD[7] = ((whatDay/10) << 4) | (whatDay%10);
            FILE *img;
            img = fopen((const char * restrict)fuse_get_context()->private_data, "r+b");
            if(img == NULL){//Checks to see if the file is open
                printf("Error File not opened\n");
                return(-ENOENT);
            }
            int CurrBlockFileDir = ROOT.LastBlockOfDirectory;
            int CurrentOffset = CurrBlockFileDir * 512;
            unsigned char tempChar;
            for(int i = 0; i < ROOT.SizeOfDirectory; i++){
                fseek(img, CurrentOffset, SEEK_SET);
                for(int j = 0; j < 512; j+=32){
                    fseek(img, CurrentOffset + j, SEEK_SET);
                    tempChar = fgetc(img);
                    //printf("TempChar: %02x\n", tempChar);
                    if(tempChar != 0x00){
                        fseek(img, CurrentOffset + j + 4, SEEK_SET);
                        char checkingName[12 + 1] = { 0x0 };

                        fgets(checkingName, 13, img);
                        //printf("Checking Old Name: %s\n", checkingName);
                        if(strcmp(checkingName, path + 1) == 0){
                            fseek(img, CurrentOffset + j + 16, SEEK_SET);
                            fwrite(BCD, 1, 8, img); // check
                            printf("Found the old file in img\n");
                            fclose(img);
                            return(0);
                        }
                    }
                }
                CurrBlockFileDir = FAT[CurrBlockFileDir];
                CurrentOffset = CurrBlockFileDir * 512;
            }
            fclose(img);
            printf("Could not find the name in the img file\n");
            return(-ENOENT);


		}
	}
    printf("File does not exists\n");
    return(-ENOENT);
}

static int _create(const char *path, mode_t  mode,struct fuse_file_info *fi){
    printf("|-----create-----|\n");
    printf("Path: %s\n", path);
    int amount_open;
    for(amount_open = 0; amount_open < AmountOfFiles; amount_open++){
		if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) ==  0){
            return(-EINVAL);
		}
	}
    if(strchr(path + 1, '/') || strcmp(path, ".")  == 0|| strcmp(path, "..")  == 0 || strlen(path) > 13){
        printf("/ might be in: %s\n", strchr(path + 1, '/'));
        printf(". might be in: %s\n", strchr(path, '.'));
        printf("Size of name: %lu\n", strlen(path));
        printf("Name trying to rename is invalid\n");
        return(-EINVAL);
    }
    memset(FileInfoArray[amount_open].FileNameWithSlash, 0x0, 13);
    memcpy(FileInfoArray[amount_open].FileNameWithSlash, path, strlen(path));
    FileInfoArray[amount_open].FileType = 0x33;
    FileInfoArray[amount_open].CopyProtec = 0x00;
    FileInfoArray[amount_open].FirstBlock = 0xfffa;
    //if ((fi->flags & 3) != O_RDONLY){return -EACCES; }
    FileInfoArray[amount_open].A_sec = time(NULL);
    FileInfoArray[amount_open].M_sec = time(NULL);
    time_t current_time;
    time(&current_time);
    struct tm *current_time_struct;
    current_time_struct = localtime(&current_time);
    unsigned char BCD[8];
    //century
    int cen = (current_time_struct->tm_year/100 + 19);
    FileInfoArray[amount_open].Cent = cen;
    BCD[0] = ((cen/10) << 4) | (cen%10);
    //year
    int year = (current_time_struct->tm_year%100);
    FileInfoArray[amount_open].Year = year;
    BCD[1] = ((year/10) << 4) | (year%10);
    //Month
    int Month = (current_time_struct->tm_mon + 1);
    FileInfoArray[amount_open].Month = Month;
    BCD[2] = ((Month/10) << 4) | (Month%10);
    //Day
    int Day = (current_time_struct->tm_mday);
    FileInfoArray[amount_open].Day = Day;
    BCD[3] = ((Day/10) << 4) | (Day%10);
    //Hour
    int Hour = (current_time_struct->tm_hour);
    if(Hour == 0){
        Hour = 6;
    }else{
        Hour = Hour -1;
    }
    FileInfoArray[amount_open].Hour = Hour;
    BCD[4] = ((Hour/10) << 4) | (Hour%10);
    //Minute
    int Min = (current_time_struct->tm_min);
    FileInfoArray[amount_open].Min = Min;
    BCD[5] = ((Min/10) << 4) | (Min%10);
    //Second
    int Sec = (current_time_struct->tm_sec);
    FileInfoArray[amount_open].Sec = Sec;
    BCD[6] = ((Sec/10) << 4) | (Sec%10);
    //Day of week
    int whatDay = (current_time_struct->tm_wday);
    if(whatDay == 0){
        whatDay = 6;
    }else{
        whatDay -= 1;
    }
    FileInfoArray[amount_open].DayOfWeek = whatDay;
    BCD[7] = ((whatDay/10) << 4) | (whatDay%10);
    FILE *img;
    img = fopen((const char * restrict)fuse_get_context()->private_data, "r+b");
    if(img == NULL){//Checks to see if the file is open
        printf("Error File not opened\n");
        return(-ENOENT);
    }
    FileInfoArray[amount_open].Offset = 0;
    FileInfoArray[amount_open].FileSizeBlock = 0;

    int CurrBlockFileDir = ROOT.LastBlockOfDirectory;
    int CurrentOffset = CurrBlockFileDir * 512;

    unsigned char tempChar;
    int WroteFileDesc = 0;
    for(int i = 0; i < ROOT.SizeOfDirectory; i++){
        fseek(img, CurrentOffset, SEEK_SET);
        for(int j = 0; j < 512; j+=32){
            fseek(img, CurrentOffset + j, SEEK_SET);
            tempChar = fgetc(img);
            //printf("TempChar: %02x\n", tempChar);
            if(tempChar == 0x00){
                fseek(img, CurrentOffset + j, SEEK_SET);
                fputc(FileInfoArray[amount_open].FileType, img);
                fputc(FileInfoArray[amount_open].CopyProtec, img);
                tempChar = 0xfa;
                fputc(tempChar, img); //first block
                tempChar = 0xff;
                fputc(tempChar, img);
                fwrite(FileInfoArray[amount_open].FileNameWithSlash + 1, 1, 12, img);
                fwrite(BCD, 1, 8, img);
                tempChar = 0x00;
                fputc(tempChar, img); //File size
                fputc(tempChar, img);
                fputc(tempChar, img); //Header
                fputc(tempChar, img);
                fputc(tempChar, img); // Unused
                fputc(tempChar, img);
                fputc(tempChar, img);
                fputc(tempChar, img);
                WroteFileDesc = 1;
                printf("File has been written\n");
                i += ROOT.SizeOfDirectory;
                break;
            }
        }
        CurrBlockFileDir = FAT[CurrBlockFileDir];
        CurrentOffset = CurrBlockFileDir * 512;
    }
    if(WroteFileDesc == 0){
        fclose(img);
        return(-ENOSPC);
    }
    AmountOfFiles += 1;
    fclose(img);




    //checks to see if the file already exists
	for(amount_open = 0; amount_open < AmountOfFiles; amount_open++){
		if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) ==  0){
			//if ((fi->flags & 3) != O_RDONLY){return -EACCES; }
			return(0);
		}
	}
	printf("Error: Open Failure\n");
	return(-ENOENT);
}

static int _chmod(const char* path, mode_t mode){
    printf("|-----chmod-----|\n");
    printf("Path: %s\n", path);
    return(0);
}

static int _chown(const char* path, uid_t uid, gid_t gid){
    for(int amount_open = 0; amount_open < AmountOfFiles; amount_open++){
		if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) ==  0){
            FileInfoArray[amount_open]._gid = gid;
            FileInfoArray[amount_open]._uid = uid;
            return(0);
		}
	}
	return(-ENOENT);
}


//Need to test
static int _unlink(const char* path){ //Does everything for destrying file
    printf("|-----unlink-----|\n");
    printf("Path: %s\n", path);
    for(int amount_open = 0; amount_open < AmountOfFiles; amount_open++){
		if(strcmp(path, FileInfoArray[amount_open].FileNameWithSlash) ==  0){
            //nuke incoming
			int FATOffset = ROOT.StartingFatBlock;
			FILE *img;
			img = fopen((const char * restrict)fuse_get_context()->private_data, "r+b");
			if(img == NULL){//Checks to see if the file is open
				printf("Error: IMG File not opened\n");
				return(-ENOENT);
			}

			//Nukes FAT table info about file
			int FATfileOffset = FileInfoArray[amount_open].FirstBlock;
			printf("First Block: %d\n", FATfileOffset);
			int FATArrayClear;
			FileInfoArray[amount_open].FirstBlock = 0xfffa;
			unsigned char freeMarked[2];
		    freeMarked[0] = 0xfc;
		    freeMarked[1] = 0xff;
		    unsigned char NukeString[512] = { 0x00 };
			//need to check
			for(int i = 0; i < FileInfoArray[amount_open].FileSizeBlock; i++){
                fseek(img, FATfileOffset * 512, SEEK_SET);
                fwrite(NukeString, 1, 512, img);
                fseek(img, FATOffset * 512 + FATfileOffset * 2, SEEK_SET);
                fwrite(freeMarked, 1, 2, img);
				FATArrayClear = FATfileOffset;
				FATfileOffset = FAT[FATfileOffset];
				printf("FAT where it points: %d\n", FATfileOffset);
				FAT[FATArrayClear] = 0xfffc;
			}

			//Nukes File Discription in File Directory
            int CurrBlockFileDir = ROOT.LastBlockOfDirectory;
            int CurrentOffset = CurrBlockFileDir * 512;
            printf("CurrBlockFileDir: %d\n", CurrBlockFileDir);
            printf("CurrentOffset: %d\n", CurrentOffset);
            unsigned char tempChar;
            for(int i = 0; i < ROOT.SizeOfDirectory; i++){
                fseek(img, CurrentOffset, SEEK_SET);
                for(int j = 0; j < 512; j+=32){
                    fseek(img, CurrentOffset + j, SEEK_SET);
                    tempChar = fgetc(img);
                    //printf("TempChar: %02x\n", tempChar);
                    if(tempChar != 0x00){
                        fseek(img, CurrentOffset + j + 4, SEEK_SET);
                        char checkingOldName[12 + 1] = { 0x0 };
                        char NukeDescription[32] = {0x0};
                        fgets(checkingOldName, 13, img);
                        //printf("Checking Old Name: %s\n", checkingOldName);
                        if(strcmp(checkingOldName, path + 1) == 0){
                            fseek(img, CurrentOffset + j, SEEK_SET);
                            fwrite(NukeDescription, 1, 32, img);
                            printf("Checking old name: %s\n", checkingOldName);
                            printf("Current Path: %s\n", path + 1);
                            printf("Found the old file in img\n");
                            i += ROOT.SizeOfDirectory;
                            break;
                        }
                    }
                }
                CurrBlockFileDir = FAT[CurrBlockFileDir];
                CurrentOffset = CurrBlockFileDir * 512;
            }
            fclose(img);
            int TempAmountOfFiles = AmountOfFiles;
            if(TempAmountOfFiles == 200){
                TempAmountOfFiles -= 1;
            }
            for(; amount_open < TempAmountOfFiles; amount_open++){
                FileInfoArray[amount_open].Cent = FileInfoArray[amount_open + 1].Cent;
                FileInfoArray[amount_open].CopyProtec = FileInfoArray[amount_open + 1].CopyProtec;
                FileInfoArray[amount_open].Day = FileInfoArray[amount_open + 1].Day;
                FileInfoArray[amount_open].DayOfWeek = FileInfoArray[amount_open + 1].DayOfWeek;
                memcpy(FileInfoArray[amount_open].FileNameWithSlash, FileInfoArray[amount_open + 1].FileNameWithSlash, 13);
                FileInfoArray[amount_open].FileSizeBlock = FileInfoArray[amount_open + 1].FileSizeBlock;
                FileInfoArray[amount_open].FileType = FileInfoArray[amount_open + 1].FileType;
                FileInfoArray[amount_open].FirstBlock = FileInfoArray[amount_open + 1].FirstBlock;
                FileInfoArray[amount_open].Hour =  FileInfoArray[amount_open + 1].Hour;
                FileInfoArray[amount_open].Min = FileInfoArray[amount_open + 1].Min;
                FileInfoArray[amount_open].Month = FileInfoArray[amount_open + 1].Month;
                FileInfoArray[amount_open].M_sec = FileInfoArray[amount_open + 1].M_sec;
                FileInfoArray[amount_open].Offset = FileInfoArray[amount_open + 1].Offset;
                FileInfoArray[amount_open].Sec = FileInfoArray[amount_open + 1].Sec;
                FileInfoArray[amount_open]._gid = FileInfoArray[amount_open + 1]._gid;
                FileInfoArray[amount_open]._uid = FileInfoArray[amount_open + 1]._uid;
            }
            FileInfoArray[amount_open].Cent = 0;
            FileInfoArray[amount_open].CopyProtec = 0;
            FileInfoArray[amount_open].Day = 0;
            FileInfoArray[amount_open].DayOfWeek = 0;
            unsigned char nukeNameWithSlash[13] = {0x0};
            memcpy(FileInfoArray[amount_open].FileNameWithSlash, nukeNameWithSlash, 13);
            FileInfoArray[amount_open].FileSizeBlock = 0;
            FileInfoArray[amount_open].FileType = 0;
            FileInfoArray[amount_open].FirstBlock = 0;
            FileInfoArray[amount_open].Hour =  0;
            FileInfoArray[amount_open].Min = 0;
            FileInfoArray[amount_open].Month = 0;
            FileInfoArray[amount_open].M_sec = 0;
            FileInfoArray[amount_open].Offset = 0;
            FileInfoArray[amount_open].Sec = 0;
            FileInfoArray[amount_open]._gid = 0;
            FileInfoArray[amount_open]._uid = 0;



            return(0);
		}
	}
	return(-ENOENT);

}


static struct fuse_operations oper = {
	.getattr = _getattr,
	.readdir = _readdir,
	.open = _open,
	.read = _read,
	.rename = _rename, //
	.mkdir = _mkdir,
	.truncate = _truncate, //
	.write = _write, //
	.utimens = _utimens, //
	.create = _create, //
	.chmod = _chmod,
	.chown = _chown, //
    .unlink = _unlink, //
};

int main(int argc, char *argv[]){
	if(argc < 3){
		printf("ERROR: Missing Arguments\n");
		return(0);
	}
	//Checking to see if the directory is opened
	DIR *CheckDir = opendir(argv[argc - 1]);
	if(CheckDir){
		closedir(CheckDir);
	}else{
		mkdir(argv[argc - 1], 0777);
		CheckDir = opendir(argv[argc - 1]);
		if(CheckDir){
			closedir(CheckDir);
		}else{
			printf("Directory was failed to be made\n");
			return(-ENOENT);
		}
	}
	FILE *img;
	img = fopen(argv[argc - 2], "r");
	if(img == NULL){
		printf("Error File not opened\n");
		return(-ENOENT);
	}
	fseek(img, -512, SEEK_END);
	unsigned char TwoChar[2];
	for(int i = 0; i < 511; i+=2){
		TwoChar[1] = fgetc(img);
		TwoChar[0] = fgetc(img);
		if(i < 16){
			if(TwoChar[0] !=  0x55 || TwoChar[1] != 0x55){
				printf("Invalid file signature\n");
				return(-EINVAL);
			}
		}else if(i == 70){//Starting Block of FAT
			ROOT.StartingFatBlock = (TwoChar[0] << 8) | TwoChar[1];
		}else if(i == 72){//Size of Block
			ROOT.SizeOfFat = (TwoChar[0] << 8) | TwoChar[1];
		}else if(i == 74){//Last Block of Directory
			ROOT.LastBlockOfDirectory = (TwoChar[0] << 8) | TwoChar[1];
		}else if(i == 76){//Size of Directory
			ROOT.SizeOfDirectory = (TwoChar[0] << 8) | TwoChar[1];
		}else if(i == 80){//NumberOfUserBlocks
			ROOT.SizeOfUserBlocks = (TwoChar[0] << 8) | TwoChar[1];
		}
	}
	printf("ROOT.StartingFatBlock: %d\n", ROOT.StartingFatBlock);
	printf("ROOT.SizeOfFat: %d\n", ROOT.SizeOfFat);
	printf("ROOT.LastBlockOfDirectory: %d\n", ROOT.LastBlockOfDirectory);
	printf("ROOT.SizeOfDirectory: %d\n", ROOT.SizeOfDirectory);

	int CurrentOffset = ROOT.StartingFatBlock * 512;
	int FatCounter = 0;
	fseek(img, CurrentOffset, SEEK_SET);
	for(int j = 0; j < 512; j+=2){
		TwoChar[1] = fgetc(img);
		TwoChar[0] = fgetc(img);
		FAT[FatCounter] = (TwoChar[0] << 8) | TwoChar[1];
		if(FatCounter == FAT[FatCounter]){
			printf("Error: FAT was not set up correctly point to the blocks\n");
			return(-EBADSLT);
		}
		FatCounter++;
	}
	for(int i = 0; i < 256; i++){
		if(FAT[i] == 0xFFFF){
			printf("Error: The FAT was not initilized fully\n");
			return(-EBADSLT);
		}
	}

	int CurrBlockFileDir = ROOT.LastBlockOfDirectory;
	CurrentOffset = CurrBlockFileDir * 512;
	unsigned char tempChar;

	for(int i = 0; i < ROOT.SizeOfDirectory; i++){
		fseek(img, CurrentOffset, SEEK_SET);
		for(int j = 0; j < 512; j+=32){
			tempChar = fgetc(img); //1
			if(tempChar != 0x00){
				FileInfoArray[AmountOfFiles].FileType = tempChar;
				tempChar = fgetc(img);//2
				FileInfoArray[AmountOfFiles].CopyProtec = tempChar;
				TwoChar[1] = fgetc(img);//3
				TwoChar[0] = fgetc(img);//4
				FileInfoArray[AmountOfFiles].FirstBlock = (TwoChar[0] << 8) | TwoChar[1];
				if(FileInfoArray[AmountOfFiles].FirstBlock >= 255 && FileInfoArray[AmountOfFiles].FirstBlock == ROOT.StartingFatBlock){
					printf("Error on a file starting Block\n");
					AmountOfFiles -=1;
				}
				FileInfoArray[AmountOfFiles].FileNameWithSlash[0] = '/';
				int i = 0;
				for(i = 0; i <  12; i++){
					tempChar = fgetc(img); //Get the char
					//FileInfoArray[AmountOfFiles].FileName[i] = tempChar;
					FileInfoArray[AmountOfFiles].FileNameWithSlash[i+1] = tempChar;
				}
				FileInfoArray[AmountOfFiles].FileNameWithSlash[13] = '\0';
				tempChar = fgetc(img);
				FileInfoArray[AmountOfFiles].Cent = ((unsigned int) tempChar / 16 * 10) + ((unsigned int) tempChar % 16);
				tempChar = fgetc(img);
				FileInfoArray[AmountOfFiles].Year = ((unsigned int) tempChar / 16 * 10) + ((unsigned int) tempChar % 16);
				tempChar = fgetc(img);
                if(FileInfoArray[AmountOfFiles].Cent >= 20){
                    FileInfoArray[AmountOfFiles].M_sec = (((unsigned long)FileInfoArray[AmountOfFiles].Cent) - 20) * 3155695200;
                    FileInfoArray[AmountOfFiles].M_sec += (((unsigned long)FileInfoArray[AmountOfFiles].Year) + 30) * 31556952;
				}else{
                    FileInfoArray[AmountOfFiles].M_sec = (((unsigned long)FileInfoArray[AmountOfFiles].Year) - 70) * 31556952;
				}
				FileInfoArray[AmountOfFiles].Month = ((unsigned int) tempChar / 16 * 10) + ((unsigned int) tempChar % 16);
				FileInfoArray[AmountOfFiles].M_sec += ((unsigned long)FileInfoArray[AmountOfFiles].Month - 1) * 2592000;
				tempChar = fgetc(img);
				FileInfoArray[AmountOfFiles].Day = ((unsigned int) tempChar / 16 * 10) + ((unsigned int) tempChar % 16);
                FileInfoArray[AmountOfFiles].M_sec += ((unsigned long)FileInfoArray[AmountOfFiles].Day - 1) * 86400;
				tempChar = fgetc(img);
				FileInfoArray[AmountOfFiles].Hour = ((unsigned int) tempChar / 16 * 10) + ((unsigned int) tempChar % 16);
				FileInfoArray[AmountOfFiles].M_sec += ((unsigned long)FileInfoArray[AmountOfFiles].Hour) * 3600;
				tempChar = fgetc(img);
				FileInfoArray[AmountOfFiles].Min = ((unsigned int) tempChar / 16 * 10) + ((unsigned int) tempChar % 16);
				FileInfoArray[AmountOfFiles].M_sec += ((unsigned long)FileInfoArray[AmountOfFiles].Min) * 60;
				tempChar = fgetc(img);
				FileInfoArray[AmountOfFiles].Sec = ((unsigned int) tempChar / 16 * 10) + ((unsigned int) tempChar % 16);
				FileInfoArray[AmountOfFiles].M_sec += ((unsigned long)FileInfoArray[AmountOfFiles].Sec);
				FileInfoArray[AmountOfFiles].A_sec = FileInfoArray[AmountOfFiles].M_sec;
                tempChar = fgetc(img);
				FileInfoArray[AmountOfFiles].DayOfWeek = tempChar;
				TwoChar[1] = fgetc(img);
				TwoChar[0] = fgetc(img);
				FileInfoArray[AmountOfFiles].FileSizeBlock = (TwoChar[0] << 8) | TwoChar[1];
				//printf("FileSize: %d\n", FileInfoArray[AmountOfFiles].FileSizeBlock);
				TwoChar[1] = fgetc(img);
				TwoChar[0] = fgetc(img);
				FileInfoArray[AmountOfFiles].Offset = (TwoChar[0] << 8) | TwoChar[1];
				tempChar = fgetc(img);
				tempChar = fgetc(img);
				tempChar = fgetc(img);
				tempChar = fgetc(img);
				FileInfoArray[AmountOfFiles]._uid = getuid();
				FileInfoArray[AmountOfFiles]._gid = getgid();
				AmountOfFiles += 1;
			}else{
				for(int k = 0; k < 31; k++){
					tempChar = fgetc(img);
				}
			}
		}
		if(FAT[CurrBlockFileDir] == 0xFFFA){
			if(i != ROOT.SizeOfDirectory - 1){
				printf("Error: FAT says one or more of the blocks the directory are not set up correct\n");
				return(-EBADSLT);
			}
		}else if(FAT[CurrBlockFileDir] == ROOT.StartingFatBlock){
			printf("Error: FAT says block is where the ROOT should be: %d\n", CurrBlockFileDir);
			return(-EBADSLT);
		}else{
			CurrBlockFileDir = FAT[CurrBlockFileDir];
			CurrentOffset = CurrBlockFileDir * 512;
		}
	}


	fclose(img);


    char buf[PATH_MAX + 1];
	char *currentPath = realpath(argv[argc - 2], buf);
	printf("This source is at %s\n", currentPath);
	char* parsedChar [argc - 1];
	for(int i = 0; i < argc; i++){
		if(i < argc - 2){
			parsedChar[i] = argv[i];
		}else if(i >= argc - 1){
			parsedChar[i - 1] = argv[i];
		}
	}

	return fuse_main(argc - 1, parsedChar, &oper,currentPath);
}
