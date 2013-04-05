#include "disk-array.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

int MAXSIZE = 35;
int verbose = 0; 
char * buffer;
int level;
int strip;
int ndisks;
int nblocks; 
char * trace; 
int opt;
int * hasFailed;
int numDisksFailed;
disk_array_t da;

void readRaid0 (int LBA, int SIZE);
void writeRaid0 (int LBA, int SIZE, int VALUE);

void readRaid10 (int LBA, int SIZE);
void writeRaid10 (int LBA, int SIZE, int VALUE);
void recoverRaid10 (int DISK);

void readRaid4 (int LBA, int SIZE);
void writeRaid4 (int LBA, int SIZE, int VALUE);
void recoverRaid4 (int DISK);

void readRaid5 (int LBA, int SIZE);
void writeRaid5 (int LBA, int SIZE, int VALUE);
void recoverRaid5 (int DISK);

void intToBlock(int VALUE, char * buffer);
int  blockToInt(char *block);

void print_usage();

int main(int argc, char * argv[]) {

    if (argc < 11 || argc > 12) {
        exit(1);
    }

    static struct option long_options[] = {
        {"level", required_argument, 0, 'l'},
        {"strip", required_argument, 0, 's'},
        {"disks", required_argument, 0, 'd'},
        {"size", required_argument, 0, 'z'},
        {"trace", required_argument, 0, 't'},
        {"verbose", optional_argument, 0, 'v'}
    };

    //printf("Going to test command line inputs.\n");
    int long_index = 0; 
    while ((opt = getopt_long_only(argc, argv, "", long_options,
                    &long_index)) != -1) 
    {
        switch(opt) {
            case 'l':
                level = atoi(optarg);
                break;
            case 's':
                strip = atoi(optarg);
                break;
            case 'd':
                ndisks = atoi(optarg);
                break;
            case 'z':
                nblocks = atoi(optarg);
                break;
            case 't':
                trace = strdup(optarg); 	
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                print_usage();
                exit(1); 
        }
    }	

    //Could make sure blocks per strip is a multiple of blocks per disk, but not required
    if((level != 0 && level != 10 && level != 4 && level != 5) ||
            (level == 10 && ndisks % 2 != 0) ||
            ((level == 4 || level == 5) && ndisks <= 2)){  
        print_usage();
        exit(1); 
    }

    da = disk_array_create("myvirtualdisk", ndisks, nblocks);

    //May technically be unnecessary as read/write returns -1 if disk has failed. However, it also returns -1 on other failures, but we probably don't have to worry about those.
    //Remove if it causes problems
    if ((hasFailed = malloc(sizeof(int)*ndisks)) == NULL)  exit(1);
    memset(hasFailed, 0, sizeof(int) *ndisks);

    FILE * fd = fopen(trace, "r");
    if (fd == NULL) {
        print_usage();
        exit(1);
    }

    char * line;
    if ((line = malloc(sizeof(char)*MAXSIZE)) == NULL)  exit(1);

    if ((buffer = malloc(sizeof(char)*nblocks)) == NULL)  exit(1);

    char * cmd; 
    int shouldExit = 0; 

    while (fgets(line, MAXSIZE, fd) != NULL && !shouldExit) {
        if (line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = 0; 

        printf("%s\n", line);

        cmd = strtok(line, " ");

        if (strcmp(cmd, "READ") == 0) 
        {
            int LBA = atoi(strtok(NULL, " "));
            int SIZE = atoi(strtok(NULL, " "));

            switch(level){
                case 0:
                    readRaid0(LBA, SIZE);
                    break;
                case 4:
                    readRaid4(LBA, SIZE);
                    break;
                case 5:
                    readRaid5(LBA, SIZE);
                    break;
                case 10:
                    readRaid10(LBA, SIZE);
                    break;
                default:
                    printf("ERROR: Reached impossible default block of read switch statement");
            }

        } 
        else if (strcmp(cmd, "WRITE") == 0) 
        {
            int LBA = atoi(strtok(NULL, " "));
            int SIZE = atoi(strtok(NULL, " "));
            int VALUE = atoi(strtok(NULL, " "));

            switch(level){
                case 0:
                    writeRaid0(LBA, SIZE, VALUE);
                    break;
                case 4:
                    writeRaid4(LBA, SIZE, VALUE);
                    break;
                case 5:
                    writeRaid5(LBA, SIZE, VALUE);
                    break;
                case 10:
                    writeRaid10(LBA, SIZE, VALUE);
                    break;
                default:
                    printf("ERROR: Reached impossible default block of write switch statement");
            }

        } 
        else if (strcmp(cmd, "FAIL") == 0) 
        {
            int DISK = atoi(strtok(NULL, " "));
            disk_array_fail_disk(da,DISK);
            hasFailed[DISK] = 1;
            ++numDisksFailed;
        } 
        else if (strcmp(cmd, "RECOVER") == 0) 
        {
            int DISK = atoi(strtok(NULL, " "));
            disk_array_recover_disk(da,DISK);

            switch(level){
                case 0:
                    break;
                case 4:
                    if(numDisksFailed == 1) recoverRaid4(DISK);
                    break;
                case 5:
                    if(numDisksFailed == 1) recoverRaid5(DISK);
                    break;
                case 10:
                    recoverRaid10(DISK);
                    break;
                default:
                    printf("ERROR: Reached impossible default block of recover switch statement");
            }
            if(hasFailed[DISK] != 0) --numDisksFailed;
            hasFailed[DISK] = 0;

        } 
        else if (strcmp(cmd, "END") == 0) {
            disk_array_print_stats(da);
            shouldExit = 1;
        }
    }
    free(line);
    free(hasFailed);
    free(buffer);
    return 0; 
}

void readRaid0 (int LBA, int SIZE){

    int i;
    for(i = LBA; i < LBA + SIZE; ++i)
    {
        int currentDisk = (i % (ndisks*strip))/strip;
        int currentBlock = (i/(strip * ndisks)) * strip + i % strip;
        if(disk_array_read(da, currentDisk, currentBlock, buffer) != 0) printf("ERROR");
        else printf("%s", buffer);        
        printf(" ");
    }
    printf("\n");
}

void writeRaid0 (int LBA, int SIZE, int VALUE){

    intToBlock(VALUE, buffer);

    int i;
    int writeHasFailed = 0;
    for(i = LBA; i < LBA + SIZE; ++i)
    {
        int currentDisk = (i % (ndisks*strip))/strip;
        int currentBlock = (i/(strip * ndisks)) * strip + i % strip;
        if(disk_array_write(da, currentDisk, currentBlock, buffer) != 0) writeHasFailed = 1; 
    }
    if(writeHasFailed != 0) printf("ERROR\n");
} 

void readRaid10 (int LBA, int SIZE){

    int usedDisks = ndisks/2;

    int i;
    for(i = LBA; i < LBA + SIZE; ++i)
    {
        int currentDisk = ((i % (usedDisks*strip))/strip) * 2;
        int currentBlock = (i/(strip * usedDisks)) * strip + i % strip;

        if(disk_array_read(da, currentDisk, currentBlock, buffer) != 0) {
            if(disk_array_read(da, currentDisk + 1, currentBlock, buffer) != 0) printf("ERROR");
            else printf("%s", buffer);
        }
        else printf("%s", buffer);
        printf(" ");
    }
    printf("\n");
}

void writeRaid10 (int LBA, int SIZE, int VALUE){

    intToBlock(VALUE, buffer);

    int usedDisks = ndisks/2;
    int writeHasFailedA = 0;
    int writeHasFailedB = 0;
    int i;
    for(i = LBA; i < LBA + SIZE; ++i)
    {
        int currentDisk = ((i % (usedDisks*strip))/strip) * 2;
        int currentBlock = (i/(strip * usedDisks)) * strip + i % strip;

        if(disk_array_write(da, currentDisk, currentBlock, buffer) != 0) writeHasFailedA = 1; 
        if(disk_array_write(da, currentDisk + 1, currentBlock, buffer) != 0) writeHasFailedB = 1; 

    }
    if(writeHasFailedA != 0 && writeHasFailedB != 0) printf("ERROR\n");
}

void recoverRaid10 (int DISK){
    int recoveryDisk = DISK + ((DISK+1)%2) - (DISK%2);

    if(hasFailed[recoveryDisk] == 0){
        int i;
        for(i = 0; i < nblocks; ++i)
        {
            disk_array_read(da, recoveryDisk, i, buffer);
            disk_array_write(da, DISK, i, buffer);
        }
    }
}

void readRaid4 (int LBA, int SIZE){

    int readDisks = ndisks - 1;

    int i;
    for(i = LBA; i < LBA + SIZE; ++i)
    {
        int currentDisk = (i % (readDisks*strip))/strip;
        int currentBlock = (i/(strip * readDisks)) * strip + i % strip;
        if(disk_array_read(da, currentDisk, currentBlock, buffer) != 0) printf("ERROR");
        else printf("%s", buffer);        
        printf(" ");
    }
    printf("\n");

}

void writeRaid4 (int LBA, int SIZE, int VALUE){

    intToBlock(VALUE, buffer);

    int usedDisks = ndisks - 1; //also is last disk in array (size - 1)
    int writeHasFailed = 0;
    int i;
    for(i = LBA; i < LBA + SIZE; ++i)
    {
        int currentDisk = ((i % (usedDisks*strip))/strip);
        int currentBlock = (i/(strip * usedDisks)) * strip + i % strip;
        if(disk_array_write(da, currentDisk, currentBlock, buffer) != 0) writeHasFailed = 1;

        if((currentDisk + 1) == usedDisks){ 
            int result = 0;
            char mem[nblocks];
            int current;
            currentDisk = 0;
            if(!(LBA <= i - (i % (strip * usedDisks)))){
                if(disk_array_read(da, currentDisk, currentBlock, mem) != 0) writeHasFailed = 1;
                current = blockToInt(mem);
                while(current != VALUE){
                    result ^= current;
                    ++currentDisk;
                    if(disk_array_read(da, currentDisk, currentBlock, mem) != 0) writeHasFailed = 1;
                    current = blockToInt(mem);
                }
            }
            if((usedDisks-currentDisk) % 2 == 0) result ^= VALUE; //This has to do with the property of XOR-ing the same value, if it is even number of XORs XOR ^ XOR with the same value equals 0, else an odd number of XXORS just results in the same result as a single XOR

            intToBlock(result, mem);
            if(disk_array_write(da, usedDisks, currentBlock, mem) != 0) writeHasFailed = 1;
        }
        else if(i + strip >= LBA + SIZE){
            int result = 0;
            char mem[nblocks];
            int current;
            if((currentDisk % usedDisks) % 2 == 0) result ^= VALUE; 
            ++currentDisk; 
            while(currentDisk != usedDisks){
                if(disk_array_read(da, currentDisk, currentBlock, mem) != 0) writeHasFailed = 1;
                current = blockToInt(mem);
                result ^= current;
                ++currentDisk;
            }
            intToBlock(result, mem);
            if(disk_array_write(da, usedDisks, currentBlock, mem) != 0) writeHasFailed = 1;
        }
    }
    if(writeHasFailed != 0) printf("ERROR\n");

}

void recoverRaid4 (int DISK){
    int usedDisks = ndisks - 1; //also is last disk in array (size - 1)

    int i;
    for(i = 0; i < nblocks; ++i){
        int currentBlock = (i/(strip * usedDisks)) * strip + i % strip;

        int result = 0;
        char mem[nblocks];
        int current;
        int currentDisk;
        for(currentDisk = 0; currentDisk < ndisks; ++currentDisk){
            if(currentDisk != DISK){
                disk_array_read(da, currentDisk, currentBlock, mem);
                current = blockToInt(mem);
                result ^= current;
            }
        }

        intToBlock(result, mem);
        disk_array_write(da, DISK, currentBlock, mem); 
    }

}

void readRaid5 (int LBA, int SIZE) {

    int readDisks = ndisks - 1;

    int i;
    for(i = LBA; i < LBA + SIZE; ++i)
    {
        int currentDisk = (i % (readDisks*strip))/strip;
        int parityDisk  = (i/(readDisks*strip))%readDisks;
        if(currentDisk >= parityDisk) ++currentDisk;
        int currentBlock = (i/(strip * readDisks)) * strip + i % strip;
        if(disk_array_read(da, currentDisk, currentBlock, buffer) != 0) printf("ERROR");
        else printf("%s", buffer);        
        printf(" ");
    }
    printf("\n");

}

void writeRaid5 (int LBA, int SIZE, int VALUE){

    intToBlock(VALUE, buffer);

    int usedDisks = ndisks - 1; //also is last disk in array (size - 1)
    int writeHasFailed = 0;
    int i;
    for(i = LBA; i < LBA + SIZE; ++i)
    {
        int currentDisk = ((i % (usedDisks*strip))/strip);
        int parityDisk = (i/(usedDisks*strip))%usedDisks;
        int currentBlock = (i/(strip * usedDisks)) * strip + i % strip;
        if(currentDisk != parityDisk && disk_array_write(da, currentDisk, currentBlock, buffer) != 0) writeHasFailed = 1;

        if(currentDisk == usedDisks || (currentDisk == (usedDisks-1) && parityDisk == usedDisks)){ 
            int result = 0;
            char mem[nblocks];
            int current;
            currentDisk = 0;
            if(currentDisk == parityDisk) ++currentDisk;
            if(!(LBA <= i - (i % (strip * usedDisks)))){
                if(disk_array_read(da, currentDisk, currentBlock, mem) != 0) writeHasFailed = 1;
                current = blockToInt(mem);
                while(current != VALUE){
                    result ^= current;
                    ++currentDisk;
                    if(currentDisk == parityDisk) ++currentDisk;
                    if(disk_array_read(da, currentDisk, currentBlock, mem) != 0) writeHasFailed = 1;
                    current = blockToInt(mem);
                }
            }
            int nonParityDisksLeftToXor = usedDisks-currentDisk;
            if(parityDisk > currentDisk) --nonParityDisksLeftToXor; 
            if((nonParityDisksLeftToXor) % 2 == 0) result ^= VALUE;
            intToBlock(result, mem);
            if(disk_array_write(da, parityDisk, currentBlock, mem) != 0) writeHasFailed = 1;
        }
        else if(i + strip >= LBA + SIZE){
            int result = 0;                       
            char mem[nblocks];
            int current;
            int nDisksUsingValue = currentDisk + 1; //Calculate the number of disks using VALUE, if it is even XOR ^ XOR with the same value equals 0, else an odd number of XXORS just results in the same result as a single XOR

            if(currentDisk >= parityDisk) --nDisksUsingValue;
            if(nDisksUsingValue % 2 == 1) result ^= VALUE;
            ++currentDisk; 
            if(currentDisk == parityDisk) ++currentDisk;
            while(currentDisk != ndisks){
                disk_array_read(da, currentDisk, currentBlock, mem);
                current = blockToInt(mem);
                result ^= current;
                ++currentDisk;
                if(currentDisk == parityDisk) ++currentDisk;
            }
            intToBlock(result, mem);
            if(disk_array_write(da, parityDisk, currentBlock, mem) != 0) writeHasFailed = 1;
        }
    }
    if(writeHasFailed != 0) printf("ERROR\n");

}

void recoverRaid5 (int DISK){

    int usedDisks = ndisks - 1; //also is last disk in array (size - 1)

    int i;
    for(i = 0; i < nblocks; ++i){
        int currentBlock = (i/(strip * usedDisks)) * strip + i % strip;

        int result = 0;
        char mem[nblocks];
        int current;
        int currentDisk;
        for(currentDisk = 0; currentDisk < ndisks; ++currentDisk){
            if(currentDisk != DISK){
                disk_array_read(da, currentDisk, currentBlock, mem);
                current = blockToInt(mem);
                result ^= current;
            }
        }

        intToBlock(result, mem);
        disk_array_write(da, DISK, currentBlock, mem); 
    }

}

void intToBlock(int VALUE, char * buffer){
    char *convertable = (char *) &VALUE;

    int i;
    for(i = 0; i < nblocks; ++i){
        buffer[i] = convertable[i%4];
    }
}

int blockToInt(char *block) {
    char convertable[4];
    convertable[0] = block[0];
    convertable[1] = block[1];
    convertable[2] = block[2];
    convertable[3] = block[3];

    return *(int *) convertable;
}

void print_usage() {
    printf("use: raidsim -level [0|10|4|5] -strip num -disks num -size num -trace filename [-verbose]\n");
}
