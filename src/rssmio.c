#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "rssmio.h"

int printtime(FILE* p) {
	//get rawtime
	time_t rawtime;
	time(&rawtime);
	
	//Get string from time
	char* time = asctime(localtime(&rawtime));
	
	//Now we need to remove all \n's from the time
	int i, j=strlen(time);
	//Because strlen ignores \0, we can be sure time[i] won't reach a \0
	for (i=0; i<j; i++)
		if (time[i] == '\n')
			time[i] = ' ';
	
	return fprintf(p, "[%s] ", time);
}

int makeDir(const char* path, FILE* log, int v) {
	if (v) {
		printtime(log);
		fprintf(log, "Checking if directory exists...\n");
	}
	
	struct stat st = {0};
	
	//If stat returns correctly then the directory is there
	if (stat(path, &st) == 0) {
		if (v) {
			printtime(log);
			fprintf(log, "Directory %s exists!\n", path);
		}
		
		return 0;
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "Directory %s doesn't exist, creating!\n", path);
	}
	
	return mkdir(path, S_IRWXU);
}

int makeFifo(const char* path, FILE* log, int v) {
	if (v) {
		printtime(log);
		fprintf(log, "Checking that fifo %s exists...\n", path);
	}
	
	struct stat st = {0};
	
	//Get stat for the path
	//If file doesn't exist, make a fifo there (directory is ensured by prior call to makeDir)
	if (stat(path, &st) != 0) {
		if (v) {
			printtime(log);
			fprintf(log, "%s does not exist, creating!\n", path);
		}
		
		return mkfifo(path, S_IRWXU);
	}
	
	//Check if the file is a fifo...
	if (v) {
		printtime(log);
		fprintf(log, "%s exists! Checking if it is a fifo...\n", path);
	}
	
	if (S_ISFIFO(st.st_mode) ) {
		if (v) {
			printtime(log);
			fprintf(log, "%s is a fifo! Returning...\n", path);
		}
		
		return 0;
	}
	
	if (v) {
		printtime(log);
		fprintf(log, "%s is not a fifo! Deleting and making it a fifo...\n", path);
	}
	
	if (remove(path) < 0) {
		printtime(log);
		fprintf(log, "Error removing non-fifo file %s . Exiting.\n", path);
		return -1;
	}
	
	return mkfifo(path, S_IRWXU);
}
