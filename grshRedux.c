/*
Cameron Brown
March 24, 2019
CSC331
grsh.c
*/
//Special thanks to user Saggarwall9 on github for being a reliable resource for my shell!

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<ctype.h>

//declare variables that the whole program will use
int BSIZE = 512; //size of buffer
int batch = 0; //indicator as the whether an argument is a batch
char *path; //global path variable
int pathChangedFlag = 0; //flag to tell us that the path variable was changed from /bin/
int pathEmptyFlag = 0; //flag for if the path is empty
char multiPath[512][512]; //large array for use if multiple paths are input
int numberMultiPath = 0; //number of paths entered

//a useful function to check for extra spaces in a command
int checkOnlySpace(char* buffer) {
	int flag = 0; //returns whether this method was actually utilized or not
	//cycle through the buffer and trigger if a space is found
	for (int i = 0; i < strlen(buffer); i++) {
		if (isspace(buffer[i]) == 0) {
			flag = 1;
			break;
		}
	}
	return flag;
}

//the requested error printout
void printError() {
	char error_message[30] = "An error has occurred\n"; //error message
	write(STDERR_FILENO, error_message, strlen(error_message));
}

//the prompt for grsh> which should be printed at the beginning of run and after every command, except "exit"
void printPrompt() {
	write(STDOUT_FILENO, "grsh> ", strlen("grsh> "));
}

//this function handles commands which are processes to execute, using fork and exec
int process(char *buffer) {
	int rc; //return code
	//attempt to execute the command if the input is not an empty character or new line
	if (buffer[0] != '\0' && buffer[0] != '\n') {
		//copy the command to this pointer for comparisons
		char *command[sizeof(buffer)];
		command[0] = strtok(buffer, " \t\n");
		//count the number of arguments in the command and store in p
		int p = 0;
		while (command[p] != NULL) {
			p++;
			command[p] = strtok(NULL, " \n\t");
		}
		command[p + 1] = NULL;
		//implementation of "cd" command
		if (strcmp(command[0], "cd") == 0) {
			//cd should only work with 1 argument
			if (p == 2) {
				if (chdir(command[1]) != 0) {
					printError();
				}
			}
			//otherwise, it's an error
			else {
				printError();
			}
		}
		//implementation of "path" command
		else if (strcmp(command[0], "path") == 0) {
			//flag that the path is being changed
			pathChangedFlag = 1;
			//if there are 2 arguments, the second argument is the new path
			if (p == 2) {
				pathEmptyFlag = 0;
				//route the path variable to the argument
				path = strdup(command[1]);
				//if the last symbol of the path is not "/", append "/" to make it a valid path
				if (path[strlen(path) - 1] != '/') {
					strcat(path, "/");
				}
			}
			//if there is one argument, the path is empty
			else if (p == 1) {
				pathEmptyFlag = 1;
			}
			//if there are multiple arguments, there are multiple paths to look at.
			else {
				pathEmptyFlag = 0;
				//iterate through the arguments and add one path at a time
				for (int i = 1; i < p; i++) {
					char *temp = strdup(command[i]);
					if (temp[strlen(temp) - 1] != '/')
						strcat(temp, "/");
					strcpy(multiPath[i - 1], temp);
					numberMultiPath++;
				}
			}
		}
		//implementation of "exit" command
		else if (strcmp(command[0], "exit") == 0) {
			if (p == 1) {
				exit(0);
			}
			else { //exit should be the only argument for this command.
				printError();
			}
		}
		//for everything else!
		else {
			//if we've emptied the path, there is nothing to find...
			if (pathEmptyFlag == 1)
				printError();
			else //time to fork and exec!
				rc = fork();
				if (rc < 0) { //Fork Error
					printError();
					exit(1);
				}
				else if (rc == 0 && pathEmptyFlag != 1) { //Child process
					if (pathChangedFlag == 0) { //by default, the path should start at /bin/
						path = strdup("/bin/");
						path = strcat(path, command[0]);
					}
					//if the path was changed to one path, let's use that one.
					else if (pathChangedFlag == 1 && numberMultiPath == 0) {
						path = strcat(path, command[0]);
					}
					//if there are multiple paths, look through them!
					else {
						for (int x = 0; x < numberMultiPath; x++) {
							strcat(multiPath[x], command[0]);
							strcpy(path, multiPath[x]);
						}
					}
					//time to execv!
					int execvSuccess = execv(path, command);
					if (execvSuccess == -1) {
						printError();
						exit(0);
					}
				}
				//"parent process"
				else {
					int returnStatus = 0;
					wait(&returnStatus); //this call is necessary to wait for the child process to execute fully before continuing. Otherwise, the prompt would occasionally print before the lines from the command were printed.
				}
				return rc;
		}
	}
	return rc;
}

//main method
int main(int argc, char* argv[]) {
	FILE *file = NULL;
	path = (char*)malloc(BSIZE);
	char buffer[BSIZE];
	if (argc == 1) { //Not batch mode
		file = stdin; //Store standard input to the file
		printPrompt();
	}
	else if (argc == 2) { //Batch mode
		char *bFile = strdup(argv[1]);
		file = fopen(bFile, "r");
		if (file == NULL) {
			printError();
		}
		batch = 1;
	}
	else {
		printError();
	}
	while (fgets(buffer, BSIZE, file)) { //Writes from file to buffer
		if (checkOnlySpace(buffer) == 0) { //Checks if the buffer is only space. Goes to the next argument if it's just a space!
			continue;
		}
		if (strstr(buffer, "&") != NULL) {//if the argument is "&" we now have parallel execution.
			int j = 0;
			char *arguments[sizeof(buffer)];
			arguments[0] = strtok(buffer, "\n\t&");
			//cycle through the arguments until complete, counting the arguments
			while (arguments[j] != NULL) {
				j++;
				arguments[j] = strtok(NULL, "\n\t&"); 
			}
			arguments[j + 1] = NULL;
			//We will process one thing at a time in a loop. We will use waitpid to wait for one to complete before going to the next
			int pid[j];
			for (int i = 0; i < j; i++) {
				//process this argument
				pid[i] = process(arguments[i]);
				for (int x = 0; x < j; i++) {
					int returnStatus = 0;
					//wait for this process to complete
					waitpid(pid[x], &returnStatus, 0);
					if (returnStatus == 1)
					{
						printError();
					}
				}
			}
		}
		//if this is just one argument to execute, let's do it!
		else {
			process(buffer);
		}
		//back to the beginning!
		if (argc == 1) {
			printPrompt();
		}
	}
}