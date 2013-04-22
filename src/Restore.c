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
#include <sys/wait.h>

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


// A function that returns a string with the name of the latest backup inside a given directory (destPath)
char * getLatestBckpDir(char *destPath);

// Returns a string with the path of a file/directory  given it's parent directory (parentDir) and the file name (name)
char * createPath(char* parentDir,char *name){

	if(parentDir==NULL || name==NULL)return NULL;

	else if(parentDir[strlen(parentDir)]=='/'){parentDir[strlen(parentDir)]='\0';}//if already has the bar remove it





	int size= strlen(parentDir)+strlen(name)+2;//+1 for the null terminator +1 for the /
	char *path =malloc(sizeof(char) *size);
	sprintf(path,"%s/%s",parentDir,name);

	return path;

}

// Verifies if the directory to be backed up is the one where the backup is supposed to go
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

//A function that verifies eventual errors coming from user input
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

//Returns the timestamp of a given Backup info file
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

//Returns the path of the latest backup directory previous to a given timestamp (LimitTimeStamp)
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

//Returns the latest backup directory
char * getLatestBckpDir(char *destPath) {

	return getLatestBckpDirPreviousTo(INT_MAX, destPath);

}

//Returns the number of files inside a given directory
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

//Returns a name of a file given it's path
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

//Returns a bool saying if the file is present in a given backup
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

//Returns the latest backup directory that contains a file which is previous to a given timestamp (limitTimeStamp)
char* getLatestBckpDirContainingFilePreviousTo(int limitTimeStamp,char * fullFilePath, char *destPath) {

	char *bckpDirPath = getLatestBckpDirPreviousTo(limitTimeStamp, destPath);

	if (bckpDirPath == NULL ) {return NULL ;}//if we found nothing return NULL

	int timeStamp = getBckpTimeStamp(bckpDirPath);//get time stamp of this backup

	char *fileName = extractFileName(fullFilePath); //get the File Name

	char *eventualFilePath = createPath(bckpDirPath, fileName);//if the file is here it has this name...

	int fd = open(eventualFilePath, O_RDONLY);//try to open the file

	while (fd == -1 && bckpDirPath != NULL ) {//while we don't have a match but we still have backup directories to search
		free(eventualFilePath);
		free(bckpDirPath);
		bckpDirPath = getLatestBckpDirPreviousTo(timeStamp, destPath);//get latest backup dir with changed time stamp
		timeStamp = getBckpTimeStamp(bckpDirPath);//change timestamp
		eventualFilePath = createPath(bckpDirPath, fileName); //get file path in bckpdir
		fd = open(eventualFilePath, O_RDONLY); //try to open it
	}

	if (fd != -1) {//if we finished the loop because we found the file
		free(eventualFilePath);
		return bckpDirPath;//return the directory path
	} else { //not found
		free(eventualFilePath);
		return NULL ;
	}

}

char * getLatestBckpDirContainingFile(char * fullFilePath, char *destPath) {

	return getLatestBckpDirContainingFilePreviousTo(INT_MAX, fullFilePath,
			destPath);

}


//tells the caller if a given directory entry is a backup directory and returns the timestamp of the backup in the int TS points to. If TS is NULL no timestamp is placed.
bool isBckpDir(struct dirent * dir_entry,int* TS,char *containingDir){
	if(dir_entry->d_type == DT_DIR && strcmp(dir_entry->d_name,"..")!=0 && strcmp(dir_entry->d_name,"..")!=0){//if is a directory and not . or ..

		char* folder=(char*)createPath(containingDir,dir_entry->d_name);
		char* path=createPath(folder,"__bckpinfo__");//eventual path of the backup file
		int fd=open(path,O_RDONLY);
		if(fd!=-1){
			close(fd);
			if(TS!=NULL){(*TS)=getBckpTimeStamp(folder);}//if TS isn't NULL then fill it with the value of the Backup timestamp
			free(path);
			return true;

		}
		free(folder);
	}
	return false;
}

//returns the number of backups in a given backup Directory
int getNumOfSubBckpDirs(char* backupDir){
	int size=0;
	DIR *bckpDir=opendir(backupDir);
	struct dirent* dir_entry;
	while ((dir_entry=readdir(bckpDir)) != NULL ) {//for every entry

		if(isBckpDir(dir_entry,NULL,backupDir)==true){size++;}
}

	return size;

}


//given a backup directory path it returns a sorted (by timestamp) list of every backup and their directory
char** getPossibleBckpList(char* backupDir) {

	int size=getNumOfSubBckpDirs(backupDir);
	DIR * dir=opendir(backupDir);
	struct dirent *dir_entry;
	char **list=malloc((size+1)*sizeof(char*));//a list to hold the file names
	int * TSlist=malloc(size*sizeof(int));//a list to hoid the file timestamps (for sorting)
	list[size]=NULL;

	int i=0;
	while((dir_entry=readdir(dir))!=NULL){//for every entry in

		if(isBckpDir(dir_entry,&TSlist[i],backupDir)==true){
			list[i]=createPath(backupDir,dir_entry->d_name);

			i++;
		}

	}



	//bubble-sort it
	i=0;
	int f=0;
	for(;i<size;i++){
		for(f=1;f<size-i;f++){
			if(TSlist[f-1]>TSlist[f]){//if previous item has a bigger timestamp
				//switch positions
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

//Returns the number of files listen in a given backup info file
int getNrOfFilesListedInBckpFile(char * backupFile){
	int nr=0;

	FILE *theBckpFile=fopen(backupFile, "r");

	char* buf= malloc((PATH_MAX)*sizeof(char));//allocate a buf with maximum path size (PATH_MAX includes null terminator)

	buf[PATH_MAX-1]='\0';

	fscanf(theBckpFile,"%[^\n]\n",buf);//read first line that is timestamp (doesn't matter here)

	while(fscanf(theBckpFile,"%[^\n]\n",buf)!=EOF){//read everything but a \n and place it at the buf and then read the \n
		nr++;
	}
	return nr;
}

//Returns a list of the files listed in a given backup info file
char ** getFilesListedInBckpFile(char * backupFile){

	if(backupFile==NULL){return NULL;}

	int size=getNrOfFilesListedInBckpFile(backupFile);
	char**list=malloc((size+1)*sizeof(char*));//allocate space for the list

	FILE *theBckpFile=fopen(backupFile, "r");
	int i=0;
	char* buf= malloc((PATH_MAX)*sizeof(char));//allocate a buf with maximum path size (PATH_MAX includes null terminator)
	buf[PATH_MAX-1]='\0';

	fscanf(theBckpFile,"%[^\n]\n",buf);//read first line that is timestamp (doesn't matter here)

	while(fscanf(theBckpFile,"%[^\n]\n",buf)!=EOF){//read everything but a \n and place it at the buf and then read the \n
		list[i++]=buf;//assign it to the list
		buf= malloc((PATH_MAX)*sizeof(char));//alloc new buffer
		buf[PATH_MAX-1]='\0';
	}
	list[size]=NULL;//last element is NULL
	free(buf);//free last allocated buf that was not used

	//close file
	fclose(theBckpFile);
	return list;


}

//frees memory from an entire array of char*
void freeList(char **list){
	int i=0;
	while(list[i]!=NULL){
		free(list[i++]);
	}
	free(list);

}

//Returns a bool saying if a given file is present in a given backup info file
bool isFilePresentInBckpFile(char * backupFile, char * fileName){

	char**list=getFilesListedInBckpFile(backupFile);
	int i=0;
	while(list[i]!=NULL){//for every file inside the list
		if(strcmp(fileName,list[i])==0){//if it has the same name
			freeList(list);
			return true;
		}
		i++;
	}
	freeList(list);
	return false;

}

//given a backup directory name it print's it's creation date
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



//Returns a bool saying if a given file (filename) is inside a given directory (dir)
bool isFileInsideDir(char* fileName,char* dir){
	DIR *dirPtr=opendir(dir);
	struct dirent * dir_entry;

	while((dir_entry=readdir(dirPtr))!=NULL){//for every entry in this directory
		if(strcmp(dir_entry->d_name,fileName)==0 && dir_entry->d_type==DT_REG){//if has the same name and is a file
			closedir(dirPtr);
			return true;
		}
	}
	closedir(dirPtr);
	return false;

}

//Displays the data to be restored to the user in a simple manner.
void displayRestoreDataToUser(char ** bckpDirList){

	printf("\n###Possible Backups###\n");
	int i=0;
	char *actualDir=bckpDirList[i];
	char *actualBackupFilePath=createPath(actualDir,"__bckpinfo__");//get path to bckp file
	char **actualDirContainingFiles=getFilesListedInBckpFile(actualBackupFilePath);//get list of files in the backup dir

	char *previousBckpFilePath=actualBackupFilePath;
	char **previousDirContainingFiles=actualDirContainingFiles;

	printf("(0)->");
	printBckpDirDate(actualDir);
	int f=0;
	while(actualDirContainingFiles[f]!=NULL){//for every file listed in this backup (first backup)
		printf("%s (created)\n",actualDirContainingFiles[f++]);
	}



	i++;//go to next



	//change actual backup directory
	actualDir=bckpDirList[i];
	actualBackupFilePath=createPath(actualDir,"__bckpinfo__");
	actualDirContainingFiles=getFilesListedInBckpFile(actualBackupFilePath);

	while(actualDir!=NULL){//while we don't read all dirs

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
		while(actualDirContainingFiles[f]!=NULL){//for every file in this directory
			char* file=actualDirContainingFiles[f];
			bool inPrevious=isFilePresentInBckpFile(previousBckpFilePath,file);
			char*name=extractFileName(file);
			if(inPrevious==false){//if it was not present in previous one
				printf("%s (created)\n",file);//it was created
			}
			else if(isFileInsideDir(name,actualDir)==true){//if it was in previous and was backedup
				printf("%s (modified)\n",file);//then it was only modified
			}
			f++;
		}




		i++;

		free(previousBckpFilePath);
		freeList(previousDirContainingFiles);

		previousBckpFilePath = actualBackupFilePath;
		previousDirContainingFiles = actualDirContainingFiles;


		//change actual directory
		actualDir = bckpDirList[i];
		actualBackupFilePath = createPath(actualDir, "__bckpinfo__");
		actualDirContainingFiles = getFilesListedInBckpFile(actualBackupFilePath);



	}

}

//Returns the size of a given list of names following the convention used (last element=NULL)
int getListSize(char **list){
	int i=0;
	while(list[i++]!=NULL);
	return i;
}

//A function that restores a file in with a given path (filePath) to a given directory (destDir)
void restoreFile(char *filePath,char* destDir,char *bckpContainingDir,char *bckpEntryDir){
	char* fileName=extractFileName(filePath);
	char* newFilePath=createPath(destDir,fileName);

	int flag=O_RDWR|O_TRUNC|O_CREAT;//create file if it doesn't exist . If it exists replace whats there
	int premissions=S_IRWXG|S_IRWXU|S_IRWXO;//restore file with all permissions
	int fd=open(newFilePath,flag,premissions);

	if(fd==-1){//if could not open
		int size=strlen("Can't Backup File Named %s :")+strlen(fileName)-1;//dont need the %s char (-2) but need the null terminator (+1) resulting in -2+1=-1;
		char* msg=malloc(size*sizeof(char));
		sprintf(msg,"Can't Backup File Named %s :",fileName);
		perror(msg);
		free(newFilePath);
		free(msg);
		return;
	}

	//if we could open

	int TS=getBckpTimeStamp(bckpEntryDir)+1;
	char *source=getLatestBckpDirContainingFilePreviousTo(TS,filePath,bckpContainingDir);

	char *originPath=createPath(source,fileName);
	free(source);
	int fdOrigin=open(originPath,O_RDONLY);//open backup file
	free(originPath);

	char buf[200];
	int readRet;

	while((readRet=read(fdOrigin,buf,200))!=0){//while not at end of file
		write(fd,buf,readRet);//write what you've read
	}
	close(fdOrigin);//close backup file
	close(fd);//close newly created file










}

//the main program function that calls fork for every file to be restored and restores them
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
		restoreFile(listOfFiles[i],destDir,bckpContainingDir,bckpDir);
		exit(0);//son is done
	}
	else{//if father
		while(i!=0){
			wait(NULL);
			i--;
		}
	}

	freeList(listOfFiles);

}



int main(int argc, char **argv) {

	ErrorCode verificationRet = verifyErrors(argc, argv); //verify common errors

	if (verificationRet != NO_ERR) {//if error check found something
		printf("Terminating Program!\n");
		return verificationRet;
	}

	char ** possibleBckpsList = getPossibleBckpList(argv[1]);
	int size=getListSize(possibleBckpsList);


	displayRestoreDataToUser(possibleBckpsList);
	printf("\n** Choose theBackup to Restore: ");
	int index=INT_MAX;

	while(index>=size){

		int value=scanf("%d",&index);
		if(index>=size || value!=1){
			char a;

			do{scanf("%c",&a);}while(a != '\n');//read whats left on the buffer

			printf("You typed in a non existing backup number.\nPlease enter a valid number: ");

		}
	}
	printf("Will start Restore from Backup nr %d\n",index);


	restore(possibleBckpsList[index],argv[2],argv[1]);

	return 0;

}
