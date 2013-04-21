/*
 ============================================================================
 Name        : Restore.c
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

char * getLatestBckpDir(char *destPath);

char * createPath(char* parentDir,char *name){

	if(parentDir==NULL || name==NULL)return NULL;

	else if(parentDir[strlen(parentDir)]=='/'){parentDir[strlen(parentDir)]='\0';}//if already has the bar remove it





	int size= strlen(parentDir)+strlen(name)+2;//+1 for the null terminator +1 for the /
	char *path =malloc(sizeof(char) *size);
	sprintf(path,"%s/%s",parentDir,name);

	return path;

}

bool SourceIsDest(char* sourcePath, char* destPath) {
	char *fullSourcePath = (char*) canonicalize_file_name(sourcePath);
	char *fullDestPath = (char*) canonicalize_file_name(destPath);

	int i = 0;
	while (fullSourcePath[i] != '\0' && fullDestPath[i] != '\0') {
		if (fullDestPath[i] != fullSourcePath[i]) {break;}//if path is different
		i++;
	}
	if (fullDestPath[i] == fullSourcePath[i]) {
		free(fullDestPath);
		free(fullSourcePath);
		return true;
	}

	free(fullDestPath);
	free(fullSourcePath);

	return false;

}

ErrorCode verifyErrors(int argc, char **argv) {

	DIR *source;
	DIR *dest;
	if (argc != 3) { //verify number of args
		printf(
				"Wrong Arguments\nUsage: rstr <backup directory> <restore directory> \n");
		return WRONG_ARG_ERR;
	}

	if ((source = opendir(argv[1])) == NULL ) { //verify if source can be opened
		perror("Cannot open source directory: ");
		return WRONG_ARG_ERR;
	} else {
		closedir(source);
	}

	if ((dest = opendir(argv[2])) == NULL ) { //verify if dest can be opened
		perror("Cannot open destination directory: ");
		return WRONG_ARG_ERR;

	} else {
		closedir(dest);
	}

	if (SourceIsDest(argv[1], argv[2])) { //assert if source and dest ar the same directory
		printf("Can't restore from a directory to itself\n");
		return WRONG_ARG_ERR;
	}

	char *path = getLatestBckpDir(argv[1]);
	if (path == NULL ) {
		printf("The origin directory ins't a valid one\n");
		return WRONG_ARG_ERR;
	} else {
		free(path);
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

char * getLatestBckpDirPreviousTo(int LimitTimeStamp, char *destPath) {

	DIR * dest = opendir(destPath);

	char* complete_dest_path = (char*) canonicalize_file_name(destPath);
	struct dirent *dir_entry;
	char* path = NULL;
	int timestamp = 0;

	while ((dir_entry=readdir(dest)) != NULL ) {

		bool parentOrSelf=false;
		if(strcmp(dir_entry->d_name,"..")==0 || strcmp(dir_entry->d_name,".")==0){ parentOrSelf=true;}


		if (dir_entry->d_type == DT_DIR && parentOrSelf==false) { //if its a directory which is not our father

			char* tempPath = createPath(complete_dest_path, dir_entry->d_name); //path to the subdir

			int temp = getBckpTimeStamp(tempPath);

			if (temp > timestamp && temp < LimitTimeStamp) { //if we got a new more recent backup

				if (path != NULL ) { free(path);}//if we already got a a previous path stored
				path = tempPath;
				timestamp = temp;
			}

			else { free(tempPath);} //if doesnt fit

		}

	}

	free(complete_dest_path);
	closedir(dest);

	return path;

}

char * getLatestBckpDir(char *destPath) {

	return getLatestBckpDirPreviousTo(INT_MAX, destPath);

}

int getNumOfFiles(char* dirPath) {
	int nr = 0;
	struct dirent *dir_entry = NULL;
	DIR *theDir = opendir(dirPath);

	dir_entry = readdir(theDir);
	while (dir_entry != NULL ) {
		if (dir_entry->d_type == DT_REG) {
			nr++;
		}
		dir_entry = readdir(theDir);
	}
	closedir(theDir);
	return nr;
}

char * extractFileName(char * fileName) {
	char *namePtr = strchr(fileName, '/');
	char *lastnamePtr = namePtr;
	while (namePtr != NULL ) {
		namePtr++;
		lastnamePtr = namePtr;
		namePtr = strchr(namePtr, '/');
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

char* getLatestBckpDirContainingFilePreviousTo(int limitTimeStamp,
		char * fullFilePath, char *destPath) {
	char *bckpDirPath = getLatestBckpDirPreviousTo(limitTimeStamp, destPath);
	printf("originaly got= %s\n", bckpDirPath);
	if (bckpDirPath == NULL ) {
		return NULL ;
	}
	int timeStamp = getBckpTimeStamp(bckpDirPath);

	char *fileName = extractFileName(fullFilePath); //get the File Name

	char *eventualFilePath = createPath(bckpDirPath, fileName);

	int fd = open(eventualFilePath, O_RDONLY);

	while (fd == -1 && bckpDirPath != NULL ) {
		free(eventualFilePath);
		free(bckpDirPath);
		bckpDirPath = getLatestBckpDirPreviousTo(timeStamp, destPath);
		timeStamp = getBckpTimeStamp(bckpDirPath);
		eventualFilePath = createPath(bckpDirPath, fileName); //get file path in bckpdir
		fd = open(eventualFilePath, O_RDONLY); //try to open it
	}

	if (fd != -1) {
		free(eventualFilePath);
		return bckpDirPath;
	} else { //not found
		free(eventualFilePath);
		return NULL ;
	}

}

char * getLatestBckpDirContainingFile(char * fullFilePath, char *destPath) {

	return getLatestBckpDirContainingFilePreviousTo(INT_MAX, fullFilePath,
			destPath);

}


bool isBckpDir(struct dirent * dir_entry,int* TS,char *containingDir){
	if(dir_entry->d_type == DT_DIR && strcmp(dir_entry->d_name,"..")!=0 && strcmp(dir_entry->d_name,"..")!=0){//if is a directory and not . or ..

		char* folder=(char*)createPath(containingDir,dir_entry->d_name);
		char* path=createPath(folder,"__bckpinfo__");
		int fd=open(path,O_RDONLY);
		if(fd!=-1){
			close(fd);
			if(TS!=NULL){(*TS)=getBckpTimeStamp(folder);}
			free(path);
			return true;

		}
		free(folder);
	}
	return false;
}

int getNumOfSubBckpDirs(char* backupDir){
	int size=0;
	DIR *bckpDir=opendir(backupDir);
	struct dirent* dir_entry = readdir(bckpDir);
	while (dir_entry != NULL ) {
		if(isBckpDir(dir_entry,NULL,backupDir)==true){
			size++;
		}
		dir_entry = readdir(bckpDir);
		}

	return size;

}



char** getPossibleBckpList(char* backupDir) {

	int size=getNumOfSubBckpDirs(backupDir);
	DIR * dir=opendir(backupDir);
	struct dirent *dir_entry=readdir(dir);
	char **list=malloc((size+1)*sizeof(char*));
	int * TSlist=malloc(size*sizeof(int));
	list[size]=NULL;

	int i=0;
	while(dir_entry!=NULL){

		if(isBckpDir(dir_entry,&TSlist[i],backupDir)==true){
			list[i]=createPath(backupDir,dir_entry->d_name);

			i++;
		}

		dir_entry=readdir(dir);

	}

	//order




	i=0;
	int f=0;
	for(;i<size;i++){
		for(f=0;f<size-i;f++){
			if(TSlist[f-1]>TSlist[f]){
				int tempInt=TSlist[f-1];
				TSlist[f-1]=TSlist[f];
				TSlist[f]=tempInt;
				char *tempStr=list[f-1];
				list[f-1]=list[f];
				list[f]=tempStr;
			}
		}

	}





	return list;

}

int getNrOfFilesListedInBckpFile(char * backupFile){
	int nr=0;

	int fd=open(backupFile,O_RDONLY);
	char buf[201];
	int readret=read(fd,buf,200);
	buf[readret]='\0';//limit buffer to the size of the read bytes(might be less than 200)
	char *newLine=strchr(buf,'\n');
	while(readret==200){
		while(newLine!=NULL){
			nr++;
			newLine=strchr(++newLine,'\n');

		}
		readret=read(fd,buf,200);
		buf[readret]='\0';
		newLine=strchr(buf,'\n');

	}

	//parse last bunch of files
	while(newLine!=NULL){
		nr++;
		newLine=strchr(++newLine,'\n');

	}



	close(fd);
	return --nr;//first new line was due to timestamp not a file
}

char ** getFilesListedInBckpFile(char * backupFile){
	if(backupFile==NULL){
		return NULL;
	}
	int fd=open(backupFile,O_RDONLY);
	int size=getNrOfFilesListedInBckpFile(backupFile);
	char**list=malloc((size+1)*sizeof(char*));
	list[size]=NULL;

	char buf[201];
	int readret=read(fd,buf,200);
	buf[readret]='\0';
	char *startMarker=strchr(buf,'\n');
	startMarker++;
	char *endMarker=strchr(startMarker,'\n');
	int i=0;
	while(readret==200){
		while(startMarker!=NULL && endMarker!=NULL){
			int nrOfChars=strlen(startMarker)-strlen(endMarker);
			char *file=malloc((nrOfChars+1)*sizeof(char));
			file[nrOfChars]='\0';
			*endMarker='\0';
			strcpy(file,startMarker);
			list[i]=file;
			startMarker=endMarker;
			startMarker++;
			endMarker=strchr(startMarker,'\n');
			i++;
		}
		lseek(fd,-strlen(startMarker)-1,SEEK_CUR);//place reading at last newLine(last file name may not be complete)
		readret=read(fd,buf,200);//read again
		buf[readret]='\0';//limit buf to the size of the read value;
		startMarker=strchr(buf,'\n');
		startMarker++;//place startMarker
		endMarker=strchr(startMarker,'\n');//place endMarker
	}
	while (startMarker != NULL && endMarker != NULL ) {//do one last iteration
		int nrOfChars = strlen(startMarker) - strlen(endMarker);
		char *file = malloc((nrOfChars + 1) * sizeof(char));
		file[nrOfChars] = '\0';
		*endMarker = '\0';
		strcpy(file, startMarker);
		list[i] = file;
		startMarker = endMarker;
		startMarker++;
		endMarker = strchr(startMarker, '\n');
		i++;
	}

	close(fd);

	return list;


}

bool isFilePresentInBckpFile(char * backupFile, char * fileName){

	char**list=getFilesListedInBckpFile(backupFile);
	int i=0;
	while(list[i]!=NULL){
		if(strcmp(fileName,list[i])==0){
			free(list);
			return true;
		}
		i++;
	}
	free(list);
	return false;

}
void printBckpDirDate(char* dirName){

	char* name=extractFileName(dirName);
	char *ptr=strchr(name,'_');
	int ano=atoi(name);
	name=ptr;
	name++;
	ptr=strchr(name,'_');
	int mes=atoi(name);
	name=ptr;
	name++;
	ptr=strchr(name,'_');
	int dia=atoi(name);

	name=ptr;
	name++;
	ptr=strchr(name,'_');
	int hora=atoi(name);

	name=ptr;
	name++;
	ptr=strchr(name,'_');
	int min=atoi(name);

	name=ptr;
	name++;
	int seg=atoi(name);
	printf("%d-%d-%d at %d:%d:%d    :\n",dia,mes,ano,hora,min,seg);
}

void freeList(char **list){
	int i=0;
	while(list[i]!=NULL){
		free(list[i++]);
	}
	free(list);

}


bool isFileInsideDir(char* fileName,char* dir){
	DIR *dirPtr=opendir(dir);
	struct dirent * dir_entry=readdir(dirPtr);

	while(dir_entry!=NULL){
		if(strcmp(dir_entry->d_name,fileName)==0){
			closedir(dirPtr);
			return true;
		}
		dir_entry=readdir(dirPtr);
	}
	closedir(dirPtr);
	return false;

}
void displayRestoreDataToUser(char ** bckpDirList){
	printf("\n###Possible Backups###\n");
	int i=0;
	char *actualDir=bckpDirList[i];
	char *actualBackupFilePath=createPath(actualDir,"__bckpinfo__");
	char **actualDirContainingFiles=getFilesListedInBckpFile(actualBackupFilePath);

	char *previousBckpFilePath=actualBackupFilePath;
	char **previousDirContainingFiles=actualDirContainingFiles;

	printf("(0)->");
	printBckpDirDate(actualDir);
	int f=0;
	while(actualDirContainingFiles[f]!=NULL){
		printf("%s (created)\n",actualDirContainingFiles[f++]);
	}



	i++;

	//free(previousBckpFilePath);
	//freeList(previousDirContainingFiles);

	previousBckpFilePath=actualBackupFilePath;
	previousDirContainingFiles=actualDirContainingFiles;

	actualDir=bckpDirList[i];
	actualBackupFilePath=createPath(actualDir,"__bckpinfo__");
	actualDirContainingFiles=getFilesListedInBckpFile(actualBackupFilePath);

	while(actualDir!=NULL){

		printf("(%d)->",i);
		printBckpDirDate(actualDir);

		int f=0;
		while(previousDirContainingFiles[f]!=NULL){//for every file in previous bckp

			char* file=previousDirContainingFiles[f];
			bool inActual=isFilePresentInBckpFile(actualBackupFilePath,file);
			if(inActual==false){//if its now missing
				printf("%s (deleted)\n",file);//was deleted
			}
			f++;
		}


		f=0;
		while(actualDirContainingFiles[f]!=NULL){
			char* file=actualDirContainingFiles[f];
			bool inPrevious=isFilePresentInBckpFile(previousBckpFilePath,file);
			char*name=extractFileName(file);
			if(inPrevious==false){//if it was not present in previous one
				printf("%s (created)\n",file);
			}
			else if(isFileInsideDir(name,actualDir)==true){//if it was in previous and was backedup
				printf("%s (modified)\n",file);
			}
			f++;
		}




		i++;

		free(previousBckpFilePath);
		freeList(previousDirContainingFiles);

		previousBckpFilePath = actualBackupFilePath;
		previousDirContainingFiles = actualDirContainingFiles;

		actualDir = bckpDirList[i];
		actualBackupFilePath = createPath(actualDir, "__bckpinfo__");
		actualDirContainingFiles = getFilesListedInBckpFile(actualBackupFilePath);



	}

}
int getListSize(char **list){
	int i=0;
	while(list[i]!=NULL){
		i++;
	}
	return i;
}

void restore(char* bckpDir,char *destDir, char* bckpContainingDir){
	char *bckpFile=createPath(bckpDir,"__bckpinfo__");
	char **listOfFiles=getFilesListedInBckpFile(bckpFile);
	free(bckpFile);

	int i=0;
	int pid=0;
	while(listOfFiles[i]!=NULL){
		pid=fork();
		if(pid==0){//son
			break;
		}

		i++;
	}
	if(listOfFiles[i]!=NULL){//son
		printf("doing it for i= %d\n",i);
		restoreFile(listOfFiles[i],destDir,bckpContainingDir,bckpDir);
	}
	else{//if father
		while(i!=0){
			wait();
			i--;
		}
	}
	freeList(listOfFiles);

}

void restoreFile(char *filePath,char* destDir,char *bckpContainingDir,char *bckpEntryDir){
	char* fileName=extractFileName(filePath);
	char* newFilePath=createPath(destDir,fileName);

	int flag=O_RDWR|O_TRUNC|O_CREAT;
	int premissions=S_IRWXG|S_IRWXU|S_IRWXO;//TODO eventually set premissions
	int fd=open(newFilePath,flag,premissions);
	if(fd==-1){
		int size=strlen("Can't Backup File Named %s :")+strlen(fileName)-1;//dont need the %s char (-2) but need the null terminator (+1) resulting in -2+1=-1;
		char* msg=malloc(size*sizeof(char));
		sprintf(msg,"Can't Backup File Named %s :",fileName);
		perror(msg);
		free(newFilePath);
		return;
	}

	int TS=getBckpTimeStamp(bckpEntryDir)+1;
	char *source=getLatestBckpDirContainingFilePreviousTo(TS,filePath,bckpContainingDir);
	char *originPath=createPath(source,fileName);
	free(source);
	int fdOrigin=open(originPath,O_RDONLY);
	free(originPath);
	char buf[200];
	int readRet=read(fdOrigin,buf,200);
	write(fd,buf,readRet);
	while(readRet==200){
		readRet=read(fdOrigin,buf,200);
		write(fd,buf,readRet);
	}
	close(fdOrigin);
	close(fd);










}

int main(int argc, char **argv) {

	ErrorCode verificationRet = verifyErrors(argc, argv); //verify common errors

	if (verificationRet != NO_ERR) {
		printf("Terminating Program!\n");
		return verificationRet;
	}

	char ** possibleBckpsList = getPossibleBckpList(argv[1]);
	int size=getListSize(possibleBckpsList);


	displayRestoreDataToUser(possibleBckpsList);
	printf("\n** Choose theBackup to Restore: ");
	int index=INT_MAX;

	while(index>=size){
		scanf("%d",&index);
		if(index>=size){
			printf("You typed in a non existing backup number. Please try again\n ");
		}
	}
	printf("Will start Restore from Backup nr %d\n",index);


	restore(possibleBckpsList[index],argv[2],argv[1]);

}
