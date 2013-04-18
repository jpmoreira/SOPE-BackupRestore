/*
 ============================================================================
 Name        : Backup.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>

//#define FTW_SKIP_SIBLINGS 3;// not defined I think it's 3!!!
typedef enum {
	NO_ERR,
	WRONG_ARG_ERR,
	ON_BACKUP_ERR,
	CREATE_BACKUP_DIR_ERR,
	CREATE_BACKUP_FILE_ERR,
	UNKNOWN_ERR

} ErrorCode;

typedef enum {
	false, true
} bool;


char * createPath(char* parentDir,char *name){

	if(parentDir==NULL || name==NULL)return NULL;

	else if(parentDir[strlen(parentDir)]=='/')parentDir[strlen(parentDir)]='\0';//if already has the bar remove it





	int size= strlen(parentDir)+strlen(name)+2;//+1 for the null terminator +1 for the /
	char *path =malloc(sizeof(char) *size);
	sprintf(path,"%s/%s",parentDir,name);

	return path;

}

bool SourceIsDest(char* sourcePath, char* destPath) {
	char *fullSourcePath = (char*) canonicalize_file_name(sourcePath);
	char *fullDestPath = (char*) canonicalize_file_name(destPath);

	int result=strcmp(fullSourcePath,fullDestPath);
	free(fullSourcePath);
	free(fullDestPath);

	if(result==0)return true;
	return false;

}

ErrorCode verifyErrors(int argc, char **argv, DIR **source, DIR **dest) {

	if (argc != 4) { //verify number of args
		printf("Wrong Arguments\nUsage: bckp <source directory> <destination directory> <time interval between backups>\n");
		return WRONG_ARG_ERR;
	}

	if (((*source) = opendir(argv[1])) == NULL ) { //verify if source can be opened
		perror("Cannot open source directory: ");
		return WRONG_ARG_ERR;
	}

	if (((*dest) = opendir(argv[2])) == NULL ) { //verify if dest can be opened
		perror("Cannot open destination directory: ");
		return WRONG_ARG_ERR;

	}

	if (SourceIsDest(argv[1], argv[2])) { //assert if source and dest ar the same directory
		printf("Can't backup from the Source to itself\n");
		return WRONG_ARG_ERR;
	}

	int timeInterval = atoi(argv[3]);

	if (timeInterval <= 0) {
		printf("Invalid time Interval\n");
		return WRONG_ARG_ERR;
	}

	return NO_ERR;

}

int getBckpTimeStamp(char* dirPath) {
	char* filePath = createPath(dirPath, "__bckpinfo__");
//	int fd = open(filePath, O_RDONLY);
//	free(filePath);
//
//	if (fd == -1) return -1;//if could not open file
//
//	else {//TODO maybe use an fscanf with fopen??
//		char buf[11];//get a buffer
//		buf[10] = '\0';//
//		if (read(fd, buf, 10) != -1) { //if we can read
//			int timestamp = atoi(buf);
//			close(fd);
//			return timestamp;
//		}
//
//	}
//
//	return -1;

	FILE * theFile=fopen(filePath,"r");
	free(filePath);

}

char * getLatestBckpDir(char *destPath) {//TODO review this

	DIR * dest = opendir(destPath);

	char* complete_dest_path = (char*) canonicalize_file_name(destPath);
	struct dirent *dir_entry;
	char* path = NULL;
	int timestamp = 0;

	dir_entry = readdir(dest);

	while (dir_entry != NULL ) {
		if (dir_entry->d_type == DT_DIR && strcmp(dir_entry->d_name,"..")!=0) { //if its a directory that is not our father
			char* tempPath = createPath(complete_dest_path, dir_entry->d_name); //path to the subdir
			int temp = getBckpTimeStamp(tempPath);
			if (temp > timestamp ) { //if we got a new more recent backup
				if (path != NULL ) { //if we already got a a previous path stored
					free(path);
				}
				path = tempPath;
				timestamp = temp;
			} else { //if doesnt fit
				free(tempPath);
			}

		}

		dir_entry = readdir(dest);
	}

	closedir(dest);

	return path;

}

int getNumOfFiles(char* dirPath) {
	int nr = 0;
	struct dirent *dir_entry = NULL;
	DIR *theDir = opendir(dirPath);

	if(theDir==NULL)return 0;


	dir_entry = readdir(theDir);
	while (dir_entry != NULL ) {
		if (dir_entry->d_type == DT_REG) nr++;
		dir_entry = readdir(theDir);
	}
	closedir(theDir);

	return nr;
}

char** getFileList(char * sourcePath) {
	int nrOfFilesInSource = getNumOfFiles(sourcePath);
	DIR *source = opendir(sourcePath);
	struct dirent *dir_entry = readdir(source);
	char ** fileList = malloc(nrOfFilesInSource * sizeof(char*) + 1);
	fileList[nrOfFilesInSource] = NULL;

	char * sourceCompletePath = (char*) canonicalize_file_name(sourcePath);

	int index = 0;
	while (dir_entry != NULL ) {

		if (dir_entry->d_type == DT_REG) fileList[index++] = createPath(sourceCompletePath,dir_entry->d_name);

		dir_entry = readdir(source);
	}

	closedir(source);

	free(sourceCompletePath);
	return fileList;
}

int getFileModificationTS(char* filePath) {
	struct stat buf;
	stat(filePath, &buf);
	return buf.st_mtime;
}

char * extractFileName(char * path) {

	char *namePtr = strchr(path, '/');//find first /
	char *lastnamePtr = namePtr;
	while (namePtr != NULL ) {//until we don't reach the end of the string
		namePtr++;//advance one position
		lastnamePtr = namePtr;//the last is this
		namePtr = strchr(namePtr, '/');//find next
	}
	return lastnamePtr;
}

bool filePresentInBckp(char* filePath, char * backupFilePath) {

	int fd = open(backupFilePath, O_RDONLY);
	int filePathSize = strlen(filePath);
	char* buf = malloc((2 * filePathSize + 1) * sizeof(char));//create a buffer at least two times the size of the file name we're searching for
	buf[2 * filePathSize] = '\0';

	int readRet = read(fd, buf, 2 * filePathSize);//fill the buffer

	while (readRet >= filePathSize / 2) {//while we read at least nameoffile/2 characters keep on going
		if (strstr(buf, filePath) != NULL ) {//if we do have a match
			close(fd);
			free(buf);
			return true;
		}
		lseek(fd,-2*filePathSize+filePathSize/2,SEEK_CUR);//go 3/4 of the read lenght back
		readRet = read(fd, buf, 2*filePathSize);//fill the hole buffer again

	}
	if (strstr(buf, filePath) != NULL ) {//now that we ended the loop verify if last read has a match
		close(fd);
		free(buf);
		return true;
	}
	return false;


}
//given a list of file paths returns a list of bools of the same size stating if a file is to be backed up or not
bool * getUpdateArray(char **list, char* bckpFolder) {

	int size = 0;
	while (list[size] != NULL ) { //get list Size
		size++;
	}
	bool *updateArray = malloc(size * sizeof(bool));//TODO maybe do this using realloc and spare the first cicle??
	char *bckpFilePath = createPath(bckpFolder, "__bckpinfo__");

	int backupTimeStamp=getFileModificationTS(bckpFilePath);


	int i = 0;
	for (; i < size; i++) {
		int fileTS=getFileModificationTS(list[i]);
		if (fileTS > backupTimeStamp|| filePresentInBckp(list[i], bckpFilePath) == false) updateArray[i] = true;
		else updateArray[i] = false;
	}
	free(bckpFilePath);


	return updateArray;

}

ErrorCode generateBackpFile(char ** fileList, char *destPath) {

	char* filePath = createPath(destPath, "__bckpinfo__");

	int fd = open(filePath, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO ); //TODO change permissions here see necessary mask on linux.die

	if (fd == -1) {
		perror("Impossible to create Backup File: ");
		free(filePath);
		return CREATE_BACKUP_FILE_ERR;
	}

	time_t rawtime;
	time(&rawtime);//fill with current timestamp
	char buf[12];
	sprintf(buf, "%d\n", (int) rawtime);
	int retWrite = write(fd, buf, 11);

	if (retWrite == -1) { //could not write
		close(fd);
		unlink(filePath);//TODO wtf is unlink doing here? what does it do?
		free(filePath);
		return CREATE_BACKUP_FILE_ERR;
	}

	int i = 0;
	for (; fileList[i] != NULL ; i++) {
		retWrite = write(fd, fileList[i], strlen(fileList[i]));
		write(fd, "\n", 1);
	}

	close(fd);

	free(filePath);
	return NO_ERR;

}

char* createNewBackupDir(char* destPath) {
	time_t rawtime;
	struct tm * timeinfo;//TODO check if this should be freed
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	char dirName[20];
	sprintf(dirName, "%d_%d_%d_%d_%d_%d", timeinfo->tm_year + 1900,
			timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour,
			timeinfo->tm_min, timeinfo->tm_sec);

	char* fullDestPath = (char*) canonicalize_file_name(destPath);
	char* dirPath = createPath(fullDestPath, dirName);
	free(fullDestPath);

	int ret = mkdir(dirPath, S_IRWXU | S_IRWXG | S_IRWXO); //TODO change permissions

	if (ret != 0) {
		perror("Problem Creating New Backup Directory: ");
		return NULL ;
	}
	return dirPath;

}

void printContentToBeUpdated(char** list, bool *mask) {
	int i = -1;//-1 to increase at every iteration
	printf("\n######Files to Backup:#######\n");


	while (list[++i] != NULL ) {
		if (mask[i] == true) printf("-->%s\n", list[i]);
	}
}

void makeBackup(char* sourceFilePath, char* destPath) {

	int originFD = open(sourceFilePath, O_RDONLY);
	char*fileName = extractFileName(sourceFilePath);
	char*completeFilePath = createPath(destPath, fileName);
	int destFD = open(completeFilePath, O_RDWR | O_CREAT,
			S_IRWXG | S_IRWXU | S_IRWXG); //TODO change this premissions maybe use the origin files one??

	char buf[200];
	int readRet;

	while ((readRet=read(originFD, buf, 200)) == 200) write(destFD, buf, readRet);//write what you've just read

	write(destFD, buf, readRet);//write whats left on the buffer


	close(originFD);
	close(destFD);
	free(completeFilePath);
}

void backupLoop(char **list, bool *mask, char* source) {

	int pid;
	int i = 0;

	for (; list[i] != NULL ; i++) {
		if (mask[i] == true) {
			pid = fork();
			if (pid == 0) break;
		}

	}

	if (list[i] != NULL ) {
		char* newLatestBckPath = getLatestBckpDir(source);
		makeBackup(list[i], newLatestBckPath);
		free(newLatestBckPath);
	} else {
		while (i-- != 0) wait();
	}

}

int main(int argc, char **argv) {

	DIR *source;
	DIR *dest;
	time_t actualTime;
	time_t nextUpdateTime;
	int timeInterval;
	time(&actualTime); //get actual time

	ErrorCode verificationRet = verifyErrors(argc, argv, &source, &dest); //verify common errors

	if (verificationRet != NO_ERR) {
		printf("Terminating Program!\n");
		return verificationRet;
	}
	timeInterval=atoi(argv[3]);
	nextUpdateTime = actualTime + timeInterval;

	while (1) { //endless loop

		time(&actualTime);
		if (actualTime >= nextUpdateTime) {
			//update nextUpdateTime
			nextUpdateTime=actualTime+timeInterval;

			char * latestBackupFilePath = getLatestBckpDir(argv[2]);

			char **fileList = getFileList(argv[1]);
			bool *updateMask = getUpdateArray(fileList, latestBackupFilePath);

			printContentToBeUpdated(fileList, updateMask);

			char* backupDest = createNewBackupDir(argv[2]);

			if (backupDest == NULL ) { //if we could not create the dir

				printf("Terminating program\n");

				if (latestBackupFilePath) free(latestBackupFilePath);

				return CREATE_BACKUP_DIR_ERR;
			}

			printf("\nBackup Directory: %s\n", backupDest);

			generateBackpFile(fileList, backupDest);

			backupLoop(fileList, updateMask, argv[2]);


			if (latestBackupFilePath != NULL )free(latestBackupFilePath);

			free(backupDest);
			free(fileList);//TODO not tested
			time(&actualTime);
			sleep(nextUpdateTime-actualTime-1);
		}


	}
	return NO_ERR;

}

