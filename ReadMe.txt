# CIS 3110 Assignment 2

## Description

Files are given via the command line when running the program. The files are then opened in a child process (unique to each file), and a histogram file with the counts of each alphabetical character will be created (same directory) once the child has terminated succesffuly. The command line arguments (besides the first, which is just for starting the program) must be the names of preexisting files in the same folder as the executable. If they do not exist, the corresponding child process will abort. At least one file name must be present. If one of the file names passed via command line is "SIG", that child process will terminate by recieving a SIGINT from the parent, then immediately sending a SIGCHLD back to the parent since the default action for SIGINT causes the process to terminate. SIGCHLD signals are handled by the signal handler. The program does NOT account for race conditions for multiple SIG filenames, but works for any number of normal filenames. Each child takes 10 + 2i seconds to complete with i >= 0. The children run simultaneously so the full runtime should be 10 +2i seconds where i will be one less than the number of files.

THE ADDITIONAL BONUS EVALUATION WAS NOT ATTEMPTED	

### Dependencies

No necessary dependencies (prerequisites, libraries, OS version, etc.) known

### Executing program

* cd to correct directory with c file inside it
* compile with "gcc -Wall -g -std=c11 A2.c" or just type "make" to use the makefile instead
* run with "./A2" for the makefile or "./a.out" for the gcc command above. Be sure to enter the filenames as command line arguments

## Limitations
* file names MUST include their extensions when passed in the command line. If not, the program will think they do not exist and will abort for the relevant child processes
* It is possible that there are some memory leaks for the edge cases only. When the program runs normally, there should be no memory leaks. Still, I never saw any memory leaks during all my testing
* Due to a need for global variables to be accessed in the handler, some max values had to be hardcoded. The program can handle at most 50 files, and each file needs to have a name < 100 characters
* Entering more than one SIG as a command line argument generally causes the program to hang, so it must be terminated manually with ctrl+C. This is a bug, likely due to race conditions with the signal handler.

## Author Information

* Name: Martin Sergo
* Mail: msergo@uoguelph.ca
* Student number:1132977

## Acknowledgments

Inspiration, code snippets, etc.

https://www.youtube.com/playlist?list=PLfqABt5AS4FkW5mOn2Tn9ZZLLDwA3kZUY