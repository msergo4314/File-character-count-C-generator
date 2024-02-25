#define _POSIX_C_SOURCE 200809L
#define MAX_FILES 50
#define MAX_FILENAME_LEN 100
#include <stdarg.h> // for variable # of function inputs
#include <unistd.h> // unix stuff like open and close
#include <signal.h> // needed for access to signal stuff
#include <sys/wait.h> // only for linux
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // for malloc and free
#include <fcntl.h>
#include <ctype.h> // for "toupper"

void generateLetterCount(char* givenString, int givenIndex);
void freeAllGlobalVariables();
void closeAllPipes();
void perrorFlush(const char *string, ...); // perror but flush (not needed)
void printFlush(const char *string, ...);
int countInvoked;

// set to NULL so they cannot accidentally be freed
pid_t *globalArr = NULL; // stores the process IDs
// read from [][0] write to [][1]
int pipes[MAX_FILES][2]; // for n files, need n pipes that go from parent to children. Each pipe is an array of ints, size 2
int **histogramNums = NULL; // wll store the arrays of 26 integers
char **fileNames = NULL;

void sigHandler(int sigNum, siginfo_t *sigInfo, void *context) {
    int childStatus;
    int pid;
    int sender = sigInfo->si_pid;
    countInvoked++;
    // printFlush("in sighandler. Sender is pid: %d\n", sender);
    // printFlush("Receiver is pid: %d\n", getpid());
    while ((pid = waitpid(-1, &childStatus, WNOHANG)) > 0) { // wait for any child to finish and do not block
        if (sigNum == SIGCHLD) { // should always execute
            if (WIFSIGNALED(childStatus)) {
                // if(pid != sender) {
                //     printFlush("Child process %d terminated by signal %d (\"%s\") sent by process %d\n", pid, WTERMSIG(childStatus), strsignal(WTERMSIG(childStatus)), sender);
                // }
                // else {
                printFlush("Child process %d terminated by signal %d (\"%s\")\n", pid, WTERMSIG(childStatus), strsignal(WTERMSIG(childStatus)));
                // }
                return;
            } else if (WIFEXITED(childStatus)) {
                printFlush("child process %d exited with status %d\n", pid, WEXITSTATUS(childStatus));
            }
            int index = -1;
            for(int i = 0; i < MAX_FILES; i++) {
                if (pid == globalArr[i]) {
                    index = i;
                    break;
                }
            }
            if(index < 0) {
                perrorFlush("Index was not found!\n");
                return;
            }
            // printFlush("parent with pid %d recieved a SIGCHLD signal from process %d\n", getpid(), sender);
            close(pipes[index][1]); // do not need to write to any pipes
            int readInts[26];
            if (read(pipes[index][0], readInts, sizeof(int) * 26) != 26 * sizeof(int)) {
                perrorFlush("read the wrong number of bytes from the pipe upon SIGCHLD\n");
                return;
            }
            close(pipes[index][0]); // close all pipes
            char newFile[MAX_FILENAME_LEN + 1];
            sprintf(newFile, "file%d.hist", sender); // sprintf has variable # of parameters
            // owner has read and write permissions,
            // and everyone else (group and others) has only read permission
            int fd = open(newFile, O_WRONLY | O_CREAT | O_TRUNC, 0644); // should succeed due to O_CREAT
            if (fd == -1) {
                perrorFlush("could not create new historgram file!\n");
                return;
            }

            char * fullHist = (char*)malloc(sizeof(char)); // realloc later then free
            strcpy(fullHist, "");
            for(int i = 0; i < 26; i++) {
                size_t size = (strlen(fullHist) + 1) * sizeof(char); // current size in bytes
                char temp[20] = "";
                if (i<25) {
                    sprintf(temp, "%c %d\n", i+97, readInts[i]); // format histogram
                } else {
                    sprintf(temp, "%c %d", i+97, readInts[i]);
                }
                fullHist = realloc(fullHist, sizeof(char) * (size + (strlen(temp))));
                strcat(fullHist, temp); //concatenate the strings
            }
            if(write(fd, fullHist, strlen(fullHist)) != strlen(fullHist)) {
                free(fullHist);
                close(fd);
                perrorFlush("Did not write the proper amount of bytes into the histogram file\n");
                return;
            }
            free(fullHist);
            close(fd);
        }
    }
    return;
}

int main (int argc, char** argv) {
    // printFlush("start of main\n");
    // setup of signals
    struct sigaction sa = {0};
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sigHandler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&(sa.sa_mask), SIGCHLD);
    sigaction(SIGCHLD, &sa, NULL);

    if (argc <= 1) {
        perrorFlush("You must enter more than one command line argument\n");
        return 1;
    }
    int files = argc - 1; // number of files is one less than CL arguments
    if (files > MAX_FILES) {
        perrorFlush("max %d arguments allowed\n", MAX_FILES);
        return 1;
    }
	
    histogramNums = (int**)malloc(sizeof(int*) * files);
    fileNames = (char**)malloc(sizeof(char*) * files);
    globalArr = (pid_t*)malloc(sizeof(pid_t) * files);

    if (!(histogramNums && fileNames && globalArr)) {
        perrorFlush("one of the mallocs failed\n");
        return 1;
    }
	
    for (int i = 0; i < files; i++) {
        // note that strdup does use malloc so free is needed
        fileNames[i] = strdup(argv[i+1]); // use strdup to copy the strings directly instead of strcpy (which would need a null terminator)
        // printFlush("copied %s\n", fileNames[i]);
        histogramNums[i] = calloc(26, sizeof(int)); // each file will produce a histogram array of ints of size 26
        if(pipe(pipes[i]) == -1) {
            perrorFlush("could not create pipe array\n");
            freeAllGlobalVariables(files);
            closeAllPipes();
            return 1;
        }
    }
    int parentID = getpid(); // id of parent process
    int pids[files];

    for (int i = 0; i < files; i++) {
        if((pids [i] = fork()) < 0) {
            perrorFlush("could not fork\n");
            freeAllGlobalVariables(files);
            closeAllPipes();
            return 1;
        }
		else if (pids[i] == 0) {
            // we are in the child
            // printFlush("in child %d with string %s\n", getpid(), fileNames[i]);
            globalArr[i] = getpid();
            if (strcmp(fileNames[i], "SIG") == 0) {
				// printFlush("Found SIG match. Pausing process %d\n", globalArr[i]);
                freeAllGlobalVariables(files);
                closeAllPipes();
                pause(); // wait to get killed
            }
            char fileToOpen[MAX_FILENAME_LEN + 1];
            strcpy(fileToOpen, fileNames[i]);
            // printFlush("opening file: %s\n", fileToOpen);
            int fd; // file descriptor
            if ((fd = open(fileToOpen, O_RDONLY)) == -1) {
                perrorFlush("could not open the file \"%s\", make sure the files exist!\n", fileToOpen);
                freeAllGlobalVariables(files);
                closeAllPipes();
                abort();
            }
            int size = lseek(fd, 0, SEEK_END); // size of file (in bytes/chars)
            // printFlush("File %s has %d bytes\n", fileToOpen, (int)size);
            char *fileContents = malloc(sizeof(char) * (size + 1)); // malloc one extra byte for \0
            if (fileContents == NULL) {
                perrorFlush("Memory allocation failed for fileContents\n");
                freeAllGlobalVariables(files);
                closeAllPipes();
                return 10;
            }

            if (lseek(fd,0,SEEK_SET) == -1) { // change file offset to 0 for proper reading
                perrorFlush("could not use lseek for file\n");
                freeAllGlobalVariables(files);
                closeAllPipes();
                free(fileContents);
                return 9;
            }
            int n = read(fd, fileContents, size);
            if (n != size) {
                perrorFlush("File was not read properly\n");
                freeAllGlobalVariables(files);
                closeAllPipes();
                free(fileContents);
                return 3;
            }
            fileContents[size] = '\0'; // manually add in the null terminator
            generateLetterCount(fileContents, i);
            close(fd); // don't need the original file anymore
            free(fileContents); // since the character counts have been generated, don't need the contents of the file  
            // printFlush("wrote to pipe successfully for child pid: %d\n", getpid());
            if (write(pipes[i][1],histogramNums[i], sizeof(int)*26) != sizeof(int)*26) {
                perrorFlush("Could not write data to the pipe properly\n");
                freeAllGlobalVariables(files);
                closeAllPipes();
                abort();
            }
            // free and exit
            freeAllGlobalVariables(files);
            closeAllPipes();
            // printf("wrote to pipe. Exiting child\n");
            sleep(1 + i);
            return 0; // terminate with no problems, sends SIGCHLD to parent
        } else {
            globalArr[i] = pids[i];
        }
    } // end loop
    if(getpid() == parentID){
        for (int i = 0; i < files; i++) {
            if(strcmp("SIG", argv[i+1]) == 0) {
                // printFlush("attempting to kill the child %d\n", globalArr[i]);
                sleep(i);
                kill(globalArr[i], SIGINT); // send the sigint signal to child process
            }
        }
        
        while (1) {
            if (countInvoked == files) {
                break;
            }
        }
        freeAllGlobalVariables(files);
        closeAllPipes();
        printFlush("Signal handler was invoked a total of %d times\n", countInvoked);
        printFlush("Exited main (pid = \"%d\")\n", parentID);
        return 0;
    }
}

void generateLetterCount(char* givenString, int index) {
    char* duplicate = strdup(givenString);
    if (!duplicate) {
        return;
    }
    int len = (int)(strlen(duplicate));
    for (int i = 0; i < len; i++) {
        duplicate[i] = toupper(duplicate[i]); // make all chars uppercase
        if (duplicate[i] >= 'A' && duplicate[i] <= 'Z') {
            histogramNums[index][duplicate[i] - 'A'] += 1; // increment histograms
        }
    }
    free(duplicate);
    return;
}

void freeAllGlobalVariables(int numFiles) {
    if (fileNames == NULL || globalArr == NULL || histogramNums == NULL|| *fileNames == NULL || *histogramNums == NULL) {
        // do nothing
        return;
    }
    for (int i = 0; i < numFiles; i++) {
        if (histogramNums[i]) {
            free(histogramNums[i]);
            histogramNums[i] = NULL;
        }
        if (fileNames[i]) {
            free(fileNames[i]);
            fileNames[i] = NULL; 
        }
    }
    free(histogramNums);
    free(globalArr);
    free(fileNames);
    fileNames = NULL;
    globalArr = NULL;
    histogramNums = NULL;
    return;
}

void closeAllPipes() {
    for (int i = 0; i < MAX_FILES; ++i) {
        // Close both ends of the pipe
        if (pipes[i][0] != -1) {
            close(pipes[i][0]);
            pipes[i][0] = -1; // Mark as closed
        }
        if (pipes[i][1] != -1) {
            close(pipes[i][1]);
            pipes[i][1] = -1; // Mark as closed
        }
    }
}

void printFlush(const char *string, ...) {
    va_list args;
    va_start(args, string);
    vfprintf(stdout, string, args);
    va_end(args);
    fflush(stdout);
}

void perrorFlush(const char *string, ...) {
    va_list args;
    va_start(args, string);
    vfprintf(stderr, string, args);
    va_end(args);
    fflush(stderr);
}
