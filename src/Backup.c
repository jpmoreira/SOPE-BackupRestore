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
#include <signal.h>

#define TRANSFER_BUF_SIZE 200

int nrSons=0;



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


void waitForSons(){
	while(nrSons--!=0){
		wait();
	}
}

void sigUSR1_Handler(int n){
	printf("called\n");

	waitForSons();
	exit(1);
}

void installSigHandler(){
	struct sigaction mySigAction;
	mySigAction.sa_handler=sigUSR1_Handler;

	sigaction(SIGUSR1,&mySigAction,NULL);
}



char * createPath(char* parentDir,char *name){

	if(parentDir==NULL || name==NULL){return NULL;}

	else if(parentDir[strlen(parentDir)]=='/'){parentDir[strlen(parentDir)]='\0';}//if already has the bar remove it





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

	if(result==0){return true;}
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
	FILE * theFile=fopen(filePath,"r");
	free(filePath);


	if(theFile==NULL){ return -1;}//if we could not open file for some reason

	int time_stamp=-1;
	fscanf(theFile,"%i\n",&time_stamp);
	fclose(theFile);
	return time_stamp;

}

char * getLatestBckpDir(char *destDirPath) {

	DIR * dest = opendir(destDirPath);

	char* complete_dest_path = (char*) canonicalize_file_name(destDirPath);
	struct dirent *dir_entry;
	char* path = NULL;
	int timestamp = 0;


	while ((dir_entry = readdir(dest)) != NULL ) {//while we dont reach end of dir

		bool parentOrSelf=false;
		if(strcmp(dir_entry->d_name,"..")==0 || strcmp(dir_entry->d_name,".")==0){ parentOrSelf=true;}

		if (dir_entry->d_type == DT_DIR && parentOrSelf==false) { //if its a directory that is not our father

			char* tempPath = createPath(complete_dest_path, dir_entry->d_name); //path to the subdir
			int temp = getBckpTimeStamp(tempPath);
			if (temp > timestamp ) { //if we got a new more recent backup

				if (path != NULL ){ free(path);}//if we already have stored one that is worst

				path = tempPath;
				timestamp = temp;
			} else{ free(tempPath);}// if we already have a better one

		}

	}

	free(complete_dest_path);
	closedir(dest);

	return path;

}

int getNumOfFiles(char* dirPath) {
	int nr = 0;
	struct dirent *dir_entry;
	DIR *theDir = opendir(dirPath);

	if(theDir==NULL){return 0;}


	while ((dir_entry = readdir(theDir)) != NULL ) {

		if (dir_entry->d_type == DT_REG){ nr++;}

	}
	closedir(theDir);

	return nr;
}

char** getFileList(char * sourcePath) {
	int nrOfFilesInSource = getNumOfFiles(sourcePath);
	printf("got num files = %d\n",nrOfFilesInSource);
	DIR *source = opendir(sourcePath);
	struct dirent *dir_entry;
	char ** fileList = malloc(nrOfFilesInSource * sizeof(char*) + 1);
	fileList[nrOfFilesInSource] = NULL;

	char * sourceCompletePath = (char*) canonicalize_file_name(sourcePath);

	int index = 0;
	printf("will enter while\n");
	while ((dir_entry = readdir(source)) != NULL ) {

		if (dir_entry->d_type == DT_REG){
			fileList[index++] = createPath(sourceCompletePath,dir_entry->d_name);
			printf("created path %s\n",fileList[index-1]);
		}

	}
	printf("ended\n");

	closedir(source);

	printf("will close\n");
	free(sourceCompletePath);
	printf("will return\n");
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

	FILE *theBckpFile=fopen(backupFilePath, "r");

	char* buf= malloc((PATH_MAX)*sizeof(char));//allocate a buf with maximum path size (PATH_MAX includes null terminator)
	buf[PATH_MAX-1]='\0';
	while(fscanf(theBckpFile,"%[^\n]\n",buf)!=EOF){//read everything but a \n and place it at the buf and then read the \n
		if(strcmp(buf,filePath)==0){

			fclose(theBckpFile);
			free(buf);
			return true;
		}
	}

	//didn't find nothing
	fclose(theBckpFile);
	free(buf);
	return false;


}

//given a list of file paths returns a list of bools of the same size stating if a file is to be backed up or not
bool * getUpdateArray(char **list, char* bckpFolder) {

	int size = 0;
	while (list[size] != NULL ) {size++;}//find out list size

	bool *updateArray = malloc(size * sizeof(bool));

	char *bckpFilePath = createPath(bckpFolder, "__bckpinfo__");

	int backupTimeStamp=getFileModificationTS(bckpFilePath);

	int i = 0;
	for (; i < size; i++) {//for every file in the list
		printf("for file %s\n",list[i]);
		int fileTS=getFileModificationTS(list[i]);

		if (fileTS > backupTimeStamp|| (filePresentInBckp(list[i], bckpFilePath) == false)){updateArray[i] = true;}//if it was changed or is not even present at last backup we should backup it

		else {updateArray[i] = false;}
	}

	free(bckpFilePath);
	return updateArray;

}

ErrorCode generateBackpFile(char ** fileList, char *destPath) {//generates backup file with necessary information

	char* filePath = createPath(destPath, "__bckpinfo__");

	int fd = open(filePath, O_RDWR | O_CREAT, S_IRUSR ); //only read permissions (can't be changed)

	if (fd == -1) {
		perror("Impossible to create Backup File: ");
		free(filePath);
		return CREATE_BACKUP_FILE_ERR;
	}

	time_t rawtime;
	time(&rawtime);//fill with current timestamp

	char buf[12];
	sprintf(buf, "%d\n", (int) rawtime);
	int retWrite = write(fd, buf, 11);//write current timestamp

	if (retWrite == -1) { //could not write
		close(fd);
		unlink(filePath);//delete file
		free(filePath);
		return CREATE_BACKUP_FILE_ERR;
	}

	int i = 0;
	for (; fileList[i] != NULL ; i++) {//for every file in the fileList
		retWrite = write(fd, fileList[i], strlen(fileList[i]));//write path to bkcp file
		write(fd, "\n", 1);//write \n at end
	}

	close(fd);

	free(filePath);
	return NO_ERR;

}

char* createNewBackupDir(char* destPath) {
	time_t rawtime;
	struct tm * timeinfo;//dont free it
	time(&rawtime);
	timeinfo = localtime(&rawtime);



	char dirName[20];
	sprintf(dirName, "%d_%d_%d_%d_%d_%d", timeinfo->tm_year + 1900,
			timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour,
			timeinfo->tm_min, timeinfo->tm_sec);

	char* fullDestPath = (char*) canonicalize_file_name(destPath);
	char* dirPath = createPath(fullDestPath, dirName);
	free(fullDestPath);

	int ret = mkdir(dirPath, S_IRWXU); //read write and execute code in this dir is possible

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

	int originFD = open(sourceFilePath, O_RDONLY);//open file to be backed up

	char*fileName = extractFileName(sourceFilePath);//extract file name from the path

	char*completeFilePath = createPath(destPath, fileName);//append it to the directory path

	int destFD = open(completeFilePath, O_RDWR | O_CREAT,S_IRUSR); //create a new read only file

	char buf[TRANSFER_BUF_SIZE];
	int readRet;

	while ((readRet=read(originFD, buf, TRANSFER_BUF_SIZE)) == TRANSFER_BUF_SIZE){
		write(destFD, buf, readRet); //write what you've just read
	}

	write(destFD, buf, readRet);//write what's left on the buffer


	//close both files
	close(originFD);
	close(destFD);

	free(completeFilePath);
}

void backupLoop(char **list, bool *mask, char* source) {

	int pid;
	int i = 0;

	for (; list[i] != NULL ; i++) {

		if (mask[i] == true) {//for each file in need to be backed up

			pid = fork();//create a child process

			if (pid == 0){ break;}//if it's a child break from this loop and start doing backup
			nrSons++;
		}

	}

	if (list[i] != NULL ) {//if you're not at last list element(meaning you're not father)

		char* latestBckPath = getLatestBckpDir(source);//grab latest backup dir
		makeBackup(list[i], latestBckPath);//do the backup

		free(latestBckPath);

		exit(0);//you've done your backup you're done
	} else {//if you're father
		waitForSons();
	}

}

int main(int argc, char **argv) {

	installSigHandler();

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
		//printf("starting loop\n");

		time(&actualTime);
		if (actualTime >= nextUpdateTime) {
			printf("will update\n");

			//update nextUpdateTime
			nextUpdateTime=actualTime+timeInterval;

			char * latestBackupFilePath = getLatestBckpDir(argv[2]);

			printf("got latest Backup File Path\n");
			char **fileList = getFileList(argv[1]);
			printf("got file list\n");
			bool *updateMask = getUpdateArray(fileList, latestBackupFilePath);
			printf("got update array\n");
			printContentToBeUpdated(fileList, updateMask);
			//printf("will create new Backup dir at %s\n",argv[2]);
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
			free(fileList);
			time(&actualTime);
			sleep(nextUpdateTime-actualTime-1);
		}


	}
	return NO_ERR;

}

