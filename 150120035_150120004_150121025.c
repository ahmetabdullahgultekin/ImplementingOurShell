#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>

/**
 * This program is a simple shell that supports the following features:
 *     - Running executables in the current directory or in the PATH environment variable.
 *     - Input/output redirection.
 *     - Searching for a string in the current directory or in a file.
 *     - Bookmarks.
 *
 * The program first defines some global variables:
 *     - input, output, append, standardError: These variables are used to check if input/output redirection is to be performed.
 *     - numberOfArguments: This variable is used to store the number of arguments in the command line.
 *     - inputFile, outputFile: These variables are used to store the input and output file names for input/output redirection.
 *     - executablePath: This variable is used to store the path to the executable file.
 *     - path: This variable is used to store the path to the executable file.
 *     - foregroundProcess: This variable is used to store the pid of the foreground process.
 *     - backgroundProcesses: This variable is used to store the pids of the background processes.
 *     - backgroundProcessCount: This variable is used to store the number of background processes.
 *     - bookmarks: This variable is used to store the bookmarks.
 *     - bookmarkCount: This variable is used to store the number of bookmarks.
 *
 * Then it defines the following functions:
 *     - setup: This function is used to read the command line and separate it into distinct arguments.
 *     - search: This function is used to search for a string in the current directory or in a file.
 *     - searchInDirectory: This function is used to search for a string in a directory.
 *     - searchInFile: This function is used to search for a string in a file.
 *     - findExecutablePath: This function is used to find the path to an executable file.
 *     - isExecutable: This function is used to check if a file is executable.
 *     - createProcess: This function is used to create a new process.
 *     - handleIO: This function is used to handle input/output redirection.
 *     - removeProcess: This function is used to remove a process from the array of background processes.
 *     - sigtstpHandler: This function is used to handle the SIGTSTP signal (Ctrl+Z).
 *     - sigchldHandler: This function is used to handle the SIGCHLD signal, which is sent to a process when a child process terminates.
 *     - bookmark: This function is used to handle bookmark operations.
 *     - deleteBookmark: This function is used to delete a bookmark from the bookmarks array.
 *     - listBookmark: This function is used to list all the bookmarks in the bookmarks array.
 *     - executeBookmark: This function is used to execute a bookmark from the bookmarks array.
 *     - addBookmark: This function is used to add a new bookmark to the bookmarks array.
 *     - main: This function is used to run the shell.
 *     - checkIO: This function is used to check and handle input/output redirection in the command line arguments.
 *
 */

int input, output, append, standardError, numberOfArguments;
char *inputFile, *outputFile;
char executablePath[256];
char path[256];
pid_t foregroundProcess;

pid_t *backgroundProcesses = NULL;
int backgroundProcessCount = 0;
char **bookmarks = NULL;
int bookmarkCount = 0;

int checkIO(char **args);

void search(char **args);

void searchInDirectory(const char *directory, const char *string);

void searchInFile(char *filePath, const char *args);

void findExecutablePath(const char *executable);

int isExecutable(const char *path);

void createProcess(char **args, int background);

void handleIO(char **args);

void removeProcess(pid_t pid);

void sigtstpHandler();

void sigchldHandler();

void bookmark(char **args);

void deleteBookmark(char **args);

void listBookmark(char **args);

void executeBookmark(char **args);

void addBookmark(char **args);

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */

/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */

void setup(char inputBuffer[], char *args[], int *background) {
    int length, /* # of characters in the command line */
    i,      /* loop index for accessing inputBuffer array */
    start,  /* index where beginning of next command parameter is */
    ct;     /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE);

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0); /* ^d was entered, end of user command stream */

    /* the signal interrupted the read system call */
    /* if the process is in the read() system call, read returns -1
      However, if this occurs, errno is set to EINTR. We can check this  value
      and disregard the -1 value */
    if ((length < 0) && (errno != EINTR)) {
        perror("error reading the command");
        exit(-1); /* terminate with error code of -1 */
    }

    for (i = 0; i < length; i++) { /* examine every character in the inputBuffer */

        switch (inputBuffer[i]) {
            case ' ':
            case '\t': /* argument separators */
                if (start != -1) {
                    args[ct] = &inputBuffer[start]; /* set up pointer */
                    ct++;
                }
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n': /* should be the final char examined */
                if (start != -1) {
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
                break;

            default: /* some other character */
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&') {
                    *background = 1;
                    inputBuffer[i - 1] = '\0';
                }
        }            /* end of switch */
    }                /* end of for */
    args[ct] = NULL; /* just in case the input line was > 80 */
    numberOfArguments = ct;
} /* end of setup routine */

/**
 * This function is used to search for a string in the current directory.
 *
 * @param args The command line arguments. The string to be searched for is expected to be in args[2] and onwards.
 *             The string should start with a double quote (") and end with a double quote (").
 *             For example, if the string to be searched for is "hello world", args[2] will be "\"hello" and args[3] will be "world\"".
 *
 * The function first checks if args[1] is not NULL. If it is, it prints an error message and returns.
 * Then it checks if args[1] is "-r". If it is, it calls the searchInDirectory function with the current directory and args as the arguments.
 * If it is not, it loops through the files in the current directory and checks if the file is a .c or .h file.
 * If it is, it calls the searchInFile function to search for the string in the file.
 */
void search(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Wrong usage of search\n");
        return;
    }
    int index;
    if (!strcmp(args[1], "-r")) {
        if (args[2] == NULL) {
            fprintf(stderr, "Wrong usage of search\n");
            return;
        }
        index = 2;
    } else index = 1;

    // args[1] starts with " and the last argument ends with "
    if (args[index][0] == '"' && args[numberOfArguments - 1][strlen(args[numberOfArguments - 1]) - 1] == '"') {
        char directory[2] = ".";
        DIR *dir = opendir(directory);

        //recursive search
        if (!strcmp(args[1], "-r")) {
            char searchString[256];
            searchString[0] = '\0';  // Initialize an empty string

            int i = 2;
            while (args[i] != NULL) {
                strcat(searchString, args[i]);
                if (args[i + 1] != NULL)
                    strcat(searchString, " ");
                i++;
            }
            if (strlen(searchString) < 3) {
                fprintf(stderr, "Wrong usage of search\n");
                return;
            }
            // Remove double quotes
            memmove(searchString, searchString + 1, strlen(searchString));
            searchString[strlen(searchString) - 1] = '\0';
            searchInDirectory(directory, searchString);
        } else { //non-recursive search
            struct dirent *entry;
            if (dir == NULL) {
                fprintf(stderr, "Error opening directory\n");
                return;
            }
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_REG && (strstr(entry->d_name, ".c") || strstr(entry->d_name, ".h"))) {
                    // Concatenate args[2] to args[argc-1]
                    char searchString[256];
                    searchString[0] = '\0';  // Initialize an empty string
                    int i = 1;
                    while (args[i] != NULL) {
                        strcat(searchString, args[i]);
                        if (args[i + 1] != NULL)
                            strcat(searchString, " ");
                        i++;
                    }
                    if (strlen(searchString) < 3) {
                        fprintf(stderr, "Wrong usage of search\n");
                        return;
                    }
                    // Remove double quotes
                    memmove(searchString, searchString + 1, strlen(searchString));
                    searchString[strlen(searchString) - 1] = '\0';
                    char path[259];
                    sprintf(path, "./%s", entry->d_name);
                    searchInFile(path, searchString);
                }
            }
        }
        closedir(dir);
    } else {
        fprintf(stderr, "Wrong usage of search\n");
    }
}

/**
 * This function is used to search for a string in a file.
 *
 * @param filePath The path to the file to be searched.
 * @param string The string to be searched for.
 *
 * The function first opens the file and checks if it is NULL.
 * If it is, it prints an error message and returns.
 * Then it loops through the lines in the file.
 * If the line contains the given string, it prints the line along with the line number and the file path.
 */
void searchInFile(char *filePath, const char *string) {
    char line[256];
    FILE *file = fopen(filePath, "r");
    if (file == NULL) {
        fprintf(stderr, "Cannot open file: %s\n", filePath);
        return;
    }

    int lineNumber = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        lineNumber++;
        if (strstr(line, string) != NULL) {
            printf("%d: %s -> %s", lineNumber, filePath, line);
        }
    }
    fclose(file);
}

/**
 * This function is used to search for a string in a directory.
 *
 * @param directory The path to the directory to be searched.
 * @param string The string to be searched for.
 *
 * The function first opens the directory and checks if it is NULL.
 * If it is, it prints an error message and returns.
 * Then it loops through the entries in the directory.
 * If the entry is a directory, it recursively calls the function for the subdirectory.
 * If the entry is a file, it checks if the file is a .c or .h file.
 * If it is, it calls the searchInFile function to search for the string in the file.
 */
void searchInDirectory(const char *directory, const char *string) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(directory);
    if (dir == NULL) {
        fprintf(stderr, "Error opening directory\n");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);

            if (entry->d_type == DT_DIR) {
                // Recursive call for subdirectories
                searchInDirectory(path, string);
            } else if (entry->d_type == DT_REG && (strstr(entry->d_name, ".c") || strstr(entry->d_name, ".h"))) {
                // Search in .c and .h files
                searchInFile(path, string);
            }
        }
    }
    closedir(dir);
}

/**
 * This function is used to handle input/output redirection in the command line arguments.
 *
 * @param args The command line arguments. The redirection operator (<, >, >>, or 2>) is expected to be in one of the arguments,
 *             and the file for redirection is expected to be in the next argument. For example, if the output is to be redirected to a file named "output.txt",
 *             one of the arguments will be ">" and the next argument will be "output.txt".
 *
 * The function first resets the global variables input, output, append, and standardError to 0.
 * Then it loops through the command line arguments. If it finds a redirection operator, it sets the corresponding global variable to 1,
 * sets the global variable inputFile or outputFile to the next argument, and sets the current argument to NULL.
 * If it finds an input redirection operator (<), it sets inputFile and leaves outputFile as NULL.
 * If it finds an output redirection operator (> or >>), it sets outputFile and leaves inputFile as NULL.
 * If it finds a standard error redirection operator (2>), it sets outputFile and leaves inputFile as NULL.
 * The function returns 1 if it finds a redirection operator, and 0 otherwise.
 */
void handleIO(char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        findExecutablePath(args[0]);
        if (!isExecutable(executablePath)) {
            fprintf(stderr, "Error: %s is not executable\n", args[0]);
        } else {
            if (input == 1) {
                freopen(inputFile, "r", stdin);
                execv(executablePath, args);
            } else if (output == 1) {
                freopen(outputFile, "w", stdout);
                execv(executablePath, args);
            } else if (append == 1) {
                freopen(outputFile, "a", stdout);
                execv(executablePath, args);
            } else if (standardError == 1) {
                freopen(outputFile, "w", stderr);
            }
        }
    } else if (pid < 0) {
        fprintf(stderr, "Error forking process\n");
    }
}

/**
 * This function is used to create a new process.
 *
 * @param args The command line arguments.
 * @param background Equals 1 if the process is to be run in the background, 0 otherwise.
 *
 * The function first forks the current process. If the fork fails, it prints an error message and terminates the program.
 * If the fork succeeds, it checks if the current process is the child process or the parent process.
 * If it is the child process, it executes the command using the execv function.
 * If the execv function fails, it prints an error message and terminates the program.
 * If it is the parent process, it checks if the process is to be run in the background.
 * If it is not, it waits for the child process to terminate.
 * If it is, it adds the pid of the child process to the array of background processes.
 */
void createProcess(char **args, int background) {
    pid_t pid = fork();

    if (pid == -1) {
        fprintf(stderr, "Error forking process\n");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        execv(executablePath, args);
        fprintf(stderr, "Error executing command\n");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        if (background == 0) { //for foreground process
            int status;
            foregroundProcess = pid;
            wait(NULL);
        } else { //for background process
            backgroundProcessCount++;
            backgroundProcesses = realloc(backgroundProcesses, backgroundProcessCount * sizeof(pid_t));
            if (backgroundProcesses == NULL) {
                fprintf(stderr, "Error reallocating memory for background processes\n");
                exit(EXIT_FAILURE);
            }
            backgroundProcesses[backgroundProcessCount - 1] = pid;
        }
    }
}

/**
 * This function is used to remove a process from the array of background processes.
 *
 * @param pid The pid of the process to be removed.
 *
 * The function loops through the array of background processes and checks if the given pid matches any of the pids in the array.
 * If it does, it shifts all subsequent elements to the left, reallocates memory for the array, and decreases the background process count by 1.
 */
void removeProcess(pid_t pid) {
    int i;
    for (i = 0; i < backgroundProcessCount; i++) {
        if (backgroundProcesses[i] == pid) {
            // Shift all subsequent elements to the left
            memmove(&backgroundProcesses[i], &backgroundProcesses[i + 1],
                    (backgroundProcessCount - i - 1) * sizeof(pid_t));
            backgroundProcessCount--;

            // Reallocate memory for the array
            backgroundProcesses = realloc(backgroundProcesses, backgroundProcessCount * sizeof(pid_t));
            if (backgroundProcesses == NULL && backgroundProcessCount > 0) {
                fprintf(stderr, "Error reallocating memory for background processes\n");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }
}

/**
 * This function checks if the given path is executable.
 *
 * @param path The path to the file to be checked.
 * @return Returns 1 if the file is executable, 0 otherwise.
 */
int isExecutable(const char *path) {
    return access(path, X_OK) == 0;
}

/**
 * This function finds the executable path of a given executable file.
 *
 * @param executable The name of the executable file.
 *
 * The function first constructs a command "which <executable>" to find the path of the executable.
 * Then it opens a pipe to execute the command and read its output.
 * If the pipe cannot be opened, it prints an error message and returns.
 * It reads the output of the command line by line and copies each line to the global variable executablePath.
 * After reading all the output, it closes the pipe and removes the trailing newline character from executablePath.
 */
void findExecutablePath(const char *executable) {
    char command[256];
    sprintf(command, "which %s", executable);
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error executing command\n");
        return;
    }
    while (fgets(path, sizeof(path) - 1, fp) != NULL) {
        strcpy(executablePath, path);
    }
    pclose(fp);
    executablePath[strcspn(executablePath, "\n")] = '\0';
}

/**
 * This function is used to check and handle input/output redirection in the command line arguments.
 *
 * @param args The command line arguments. The function expects the redirection operator (<, >, >>, or 2>) to be in one of the arguments,
 *             and the file for redirection to be in the next argument. For example, if the output is to be redirected to a file named "output.txt",
 *             one of the arguments will be ">" and the next argument will be "output.txt".
 *
 * The function first resets the global variables input, output, append, and standardError to 0.
 * Then it loops through the command line arguments. If it finds a redirection operator, it sets the corresponding global variable to 1,
 * sets the global variable inputFile or outputFile to the next argument, and sets the current argument to NULL.
 * If it finds an input redirection operator (<), it sets inputFile and leaves outputFile as NULL.
 * If it finds an output redirection operator (> or >>), it sets outputFile and leaves inputFile as NULL.
 * If it finds a standard error redirection operator (2>), it sets outputFile and leaves inputFile as NULL.
 * The function returns 1 if it finds a redirection operator, and 0 otherwise.
 */
int checkIO(char **args) {
    // reset input and output and append
    input = 0;
    output = 0;
    append = 0;
    standardError = 0;

    int i = 0;

    // loop through the command line input
    while (args[i] != NULL) {
        if (!strcmp(args[i], "<")) { // check for input <
            inputFile = args[i + 1];
            outputFile = NULL;
            args[i] = NULL;
            input = 1;
            return 1;
        } else if (!strcmp(args[i], ">")) { // check for output >
            outputFile = args[i + 1];
            inputFile = NULL;
            args[i] = NULL;
            output = 1;
            return 1;
        } else if (!strcmp(args[i], ">>")) { // check for append output >>
            outputFile = args[i + 1];
            inputFile = NULL;
            args[i] = NULL;
            append = 1;
            return 1;
        } else if (!strcmp(args[i], "2>")) { // check for standard error 2>
            outputFile = args[i + 1];
            inputFile = NULL;
            args[i] = NULL;
            standardError = 1;
            return 1;
        }
        i++;
    }
    return 0;
}

/**
 * This function is used to handle the SIGTSTP signal (Ctrl+Z).
 *
 * If there is a foreground process running, it sends the SIGKILL signal to terminate it and sets the foregroundProcess to 0.
 */
void sigtstpHandler() {
    if (foregroundProcess > 0) {
        kill(-foregroundProcess, SIGKILL);
        foregroundProcess = 0;
    }
}

/**
 * This function is used to handle the SIGCHLD signal, which is sent to a process when a child process terminates.
 *
 * It repeatedly calls waitpid with a pid of -1 and the WNOHANG option, which causes it to return immediately if no child processes have exited.
 * If a child process has exited, it gets its pid and checks if it exited normally using the WIFEXITED macro.
 * If it did, it calls the removeProcess function to remove the pid from the array of background processes.
 */
void sigchldHandler() {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            removeProcess(pid);  // Remove the process ID from the array
        }
    }
}

/**
 * This function is used to handle bookmark operations.
 *
 * @param args The command line arguments. The operation to be performed is expected to be in args[1].
 *             For example, if the operation is to delete a bookmark, args[1] will be "-d".
 *             The additional arguments required for the operation (such as the index of the bookmark to be deleted)
 *             are expected to be in args[2] and onwards.
 *
 * The function first checks if args[1] is not NULL. If it is, it prints an error message and returns.
 * Then it checks the value of args[1] to determine the operation to be performed:
 *     - If args[1] is "-d", it calls the deleteBookmark function with args as the argument.
 *     - If args[1] is "-l", it calls the listBookmark function with args as the argument.
 *     - If args[1] is "-i", it calls the executeBookmark function with args as the argument.
 *     - If args[1] is none of the above, it calls the addBookmark function with args as the argument.
 */
void bookmark(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Wrong usage of bookmark\n");
        return;
    }
    if (!strcmp(args[1], "-d")) {
        deleteBookmark(args);
    } else if (!strcmp(args[1], "-l")) {
        listBookmark(args);
    } else if (!strcmp(args[1], "-i")) {
        executeBookmark(args);
    } else {
        addBookmark(args);
    }
}

/**
 * This function is used to delete a bookmark from the bookmarks array.
 *
 * @param args The command line arguments. The index of the bookmark to be deleted is expected to be in args[2].
 *             For example, if the bookmark at index 0 is to be deleted, args[2] will be "0".
 *
 * The function first checks if args[1] and args[2] are not NULL and args[3] is NULL. If not, it prints an error message and returns.
 * Then it converts args[2] to an integer and checks if the index is within the range of the bookmarks array.
 * If it is, it frees the memory allocated for the bookmark at the given index, shifts all subsequent bookmarks to the left,
 * reallocates the bookmarks array to decrease its size by 1, and decreases the bookmark count by 1.
 * If the index is not within the range of the bookmarks array, it prints an error message.
 */
void deleteBookmark(char **args) {
    if (args[1] == NULL || args[2] == NULL || args[3] != NULL) {
        fprintf(stderr, "Wrong usage of bookmark\n");
        return;
    }
    int index = atoi(args[2]);
    if (index < 0 || index >= bookmarkCount) {
        fprintf(stderr, "Wrong usage of bookmark\n");
        return;
    }
    free(bookmarks[index]);
    // Shift all subsequent elements to the left
    memmove(&bookmarks[index], &bookmarks[index + 1], (bookmarkCount - index - 1) * sizeof(char *));
    bookmarks = realloc(bookmarks, (bookmarkCount - 1) * sizeof(char *));
    bookmarkCount--;
}

/**
 * This function is used to list all the bookmarks in the bookmarks array.
 *
 * @param args The command line arguments. No additional arguments are expected after the command itself.
 *             For example, the correct usage is "bookmark -l".
 *
 * The function first checks if args[1] is NULL and args[2] is NULL. If not, it prints an error message and returns.
 * Then it iterates over the bookmarks array and prints each bookmark along with its index.
 */
void listBookmark(char **args) {
    if (args[1] == NULL || args[2] != NULL) {
        fprintf(stderr, "Wrong usage of bookmark\n");
        return;
    }
    for (int i = 0; i < bookmarkCount; i++) {
        printf("%d \"%s\"\n", i, bookmarks[i]);
    }
}

/**
 * This function is used to execute a bookmark from the bookmarks array.
 *
 * @param args The command line arguments. The index of the bookmark to be executed is expected to be in args[2].
 *             For example, if the bookmark at index 0 is to be executed, args[2] will be "0".
 *
 * The function first checks if args[1] and args[2] are not NULL and args[3] is NULL. If not, it prints an error message and returns.
 * Then it converts args[2] to an integer and checks if the index is within the range of the bookmarks array.
 * If it is, it executes the bookmark at the given index using the system function.
 * If the index is not within the range of the bookmarks array, it prints an error message.
 */
void executeBookmark(char **args) {
    if (args[1] == NULL || args[2] == NULL || args[3] != NULL) {
        fprintf(stderr, "Wrong usage of bookmark\n");
        return;
    }
    int index = atoi(args[2]);

    if (index >= 0 && index < bookmarkCount) {
        system(bookmarks[index]);
    } else {
        fprintf(stderr, "Wrong usage of bookmark\n");
    }
}

/**
 * This function is used to add a new bookmark to the bookmarks array.
 *
 * @param args The command line arguments. The bookmark to be added is expected to be in args[1] and onwards.
 *             The bookmark should start with a double quote (") and end with a double quote (").
 *             For example, if the bookmark is "ls -l", args[1] will be "\"ls" and args[2] will be "-l\"".
 *
 * The function first reallocates the bookmarks array to increase its size by 1.
 * Then it checks if the first character of args[1] and the last character of the last argument are double quotes.
 * If they are, it concatenates all the arguments from args[1] to the last argument into a single string,
 * removes the double quotes from the start and end of the string, and adds the string to the bookmarks array.
 * If the first character of args[1] and the last character of the last argument are not double quotes,
 * it prints an error message and returns.
 *
 * If any error occurs during the execution of the function (such as failing to reallocate memory for the bookmarks array
 * or failing to duplicate the string for the bookmark), the function prints an error message and terminates the program.
 */
void addBookmark(char **args) {
    bookmarks = realloc(bookmarks, (bookmarkCount + 1) * sizeof(char *));
    if (bookmarks == NULL) {
        fprintf(stderr, "Error reallocating memory for bookmarks");
        exit(EXIT_FAILURE);
    }

    // args[1] starts with " and the last argument ends with "
    if (args[1][0] == '"' && args[numberOfArguments - 1][strlen(args[numberOfArguments - 1]) - 1] == '"') {
        char command[256];
        command[0] = '\0';  // Initialize an empty string
        int i = 1;
        while (args[i] != NULL) {
            strcat(command, args[i]);
            if (args[i + 1] != NULL)
                strcat(command, " ");
            i++;
        }
        if (strlen(command) < 3) {
            fprintf(stderr, "Wrong usage of bookmark\n");
            return;
        }
        // Remove double quotes
        memmove(command, command + 1, strlen(command));
        command[strlen(command) - 1] = '\0';

        bookmarks[bookmarkCount] = strdup(command);
        if (bookmarks[bookmarkCount] == NULL) {
            fprintf(stderr, "Error duplicating string for bookmark");
            exit(EXIT_FAILURE);
        }
        bookmarkCount++;
    } else {
        fprintf(stderr, "Wrong usage of bookmark\n");
    }
}

int main(void) {
    char inputBuffer[MAX_LINE];   /*buffer to hold command entered */
    int background;               /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE / 2 + 1]; /*command line arguments */
    FILE *errorFile = fopen("stdError.txt", "w");
    if (errorFile == NULL) {
        fprintf(stderr, "Error opening file\n");
        return -1;
    }
    // Redirect stderr to the file
    if (dup2(fileno(errorFile), STDERR_FILENO) == -1) {
        perror("Error redirecting stderr");
        exit(EXIT_FAILURE);
    }

    signal(SIGTSTP, sigtstpHandler);
    signal(SIGCHLD, sigchldHandler);

    while (1) {
        background = 0;
        printf("myshell: ");
        fflush(0);
        /*setup() calls exit() when Control-D is entered */
        setup(inputBuffer, args, &background);

        if (args[0] == NULL)
            continue; // If enter pressed without any command

        executablePath[0] = '\0';
        findExecutablePath(args[0]);
        if (checkIO(args) == 1) {
            handleIO(args);
        } else if (strcmp(args[0], "search") == 0) {
            search(args);
            continue;
        } else if (strcmp(args[0], "bookmark") == 0) {
            bookmark(args);
        } else if (strcmp(args[0], "exit") == 0) {
            if (backgroundProcessCount > 0) {
                printf("There are background processes running. Please terminate them first.\n");
                continue;
            } else {
                exit(0);
            }
        } else if (isExecutable(executablePath)) {
            if (strcmp(args[numberOfArguments - 1], "&") == 0) {
                args[numberOfArguments - 1] = '\0';
            }
            createProcess(args, background);
        } else {
            fprintf(stderr, "Error: %s is not executable\n", args[0]);
        }
    }
}

