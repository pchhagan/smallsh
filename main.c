// If you are not compiling with the gcc option --std=gnu99, then
// uncomment the following line or you might get a compiler warning
//#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#define LENGTH 10

int status;
int process[LENGTH] = {0};
int backgroundMode = 0;
struct sigaction SIGINT_action = { { 0} };
struct sigaction SIGSTP_action = { { 0} };

/* struct for user input information */
struct userInput
{
    char* command;
    char* args[513];
    char* input_file;
    char* output_file;
    int background;
};


/*
* function to handle SIGSTP signal. This will toggle foreground only mode
*/
void handle_SIGSTP(int signo) {
    if (backgroundMode == 1) {
        backgroundMode = 0;
        char* message = "Exiting foreground-only mode\n";
        write(1, message, 29);
    }
    else {
        backgroundMode = 1;
        char* message = "Entering foreground-only mode (& is now ignored)\n";
        write(1, message, 49);
    }
}

/*
* This will redirect input if the user has entered a new input file
*/
void redirectInput(struct userInput* currCommand) {
    // Open source file
    int sourceFD = open(currCommand->input_file, O_RDONLY);
    if (sourceFD == -1) {
        perror("source open()");
        exit(1);
    }

    // Redirect stdin to source file
    int result = dup2(sourceFD, 0);
    if (result == -1) {
        perror("source dup2()");
        exit(1);
    }
}

/*
* This will redirect the output if the user has entered a new output file
*/
void redirectOutput(struct userInput* currCommand) {
    // Open target file
    int targetFD = open(currCommand->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (targetFD == -1) {
        perror("target open()");
        exit(1);
    }

    // Redirect stdout to target file
    int result = dup2(targetFD, 1);
    if (result == -1) {
        perror("target dup2()");
        exit(1);
    }
}

/*
* this will expand "$$" wherever it appears in the user input
*/
char* expand(char* s)
{
    char strPID[6];
    sprintf(strPID, "%d", getpid());


    char* buffer = malloc(2048);
    char* newLine = s;
    char* i = NULL;
    while ((i = strstr(newLine, "$$")) != NULL) {
        strncat(buffer, newLine, (size_t)(i - newLine));
        strcat(buffer, strPID);
        newLine = i + strlen("$$");
    }
    strcat(buffer, newLine);
    return buffer;
}


/* Parse the input line which is space delimited and create a
*  user input struct with the data in this line
*/
struct userInput *createCommand(char *currLine)
{

    struct userInput *currCommand = malloc(sizeof(struct userInput));
    currCommand->output_file = NULL;
    currCommand->input_file = NULL;
    char *saveptr;
    int i = 1;
    currLine[strcspn(currLine, "\n")] = 0;
    int inputLength = strlen(currLine);
    if (currLine[inputLength - 1] == '&') {
        currCommand->background = 1;
    }
    else
    {
        currCommand->background = 0;
    }

    // The first token is the command to perform
    char *token = strtok_r(currLine, " ", &saveptr);
    currCommand->command = calloc(strlen(token) + 1, sizeof(char));
    strcpy(currCommand->command, token);
    currCommand->args[0] = malloc((strlen(token) + 1) * sizeof(char));
    strcpy(currCommand->args[0], token);

    // The next token is the either input file, output file, arguments, or a character to toggle foreground mode
    token = strtok_r(NULL, " ", &saveptr); 

    while (token != NULL) {
        if (!strcmp(token, "<")) {
            token = strtok_r(NULL, " ", &saveptr);
            if (token != NULL) {
                currCommand->input_file = calloc(strlen(token) + 1, sizeof(char));
                strcpy(currCommand->input_file, token);
            }
        }
        else if (!strcmp(token, ">")) {
            token = strtok_r(NULL, " ", &saveptr);
            if (token != NULL) {
                currCommand->output_file = calloc(strlen(token) + 1, sizeof(char));
                strcpy(currCommand->output_file, token);
            }
        }

        else {
            currCommand->args[i] = malloc((strlen(token) + 1) * sizeof(char));
            strcpy(currCommand->args[i], token);
            i++; 
        }
        token = strtok_r(NULL, " ", &saveptr);
        
    }

    /*
    * The following lines will redirect input and output to /dev/null if needed.
    */
    if (currCommand->background == 1 && currCommand->input_file == NULL) {
        currCommand->input_file = calloc(strlen("/dev/null") + 1, sizeof(char));
        strcpy(currCommand->input_file, "/dev/null");
        }
    if (currCommand->background == 1 && currCommand->output_file == NULL) {
        currCommand->output_file = calloc(strlen("/dev/null") + 1, sizeof(char));
        strcpy(currCommand->output_file, "/dev/null");
        }
    
    return currCommand;
}


/*
* This will get an input from the user, expand $$, and return the line
*/
char* getInput()
{
    char* currLine = NULL;

    printf(": ");
    fflush(stdout);

    size_t inputSize = 2048;
    getline(&currLine, &inputSize, stdin);

    

    if (!strstr(currLine, "$$")) {
        return currLine;
    }
    else {
        return expand(currLine);
    }
}


/*
* This will kill background processes and exit the shell
*/
void exitProgram()
{

    for (int i = 0; i < LENGTH; i++) {
        if (process[i] != 0) {
            kill(process[i], SIGKILL);
            process[i] = 0;
        }
    }
    exit(0);
}

/*
* Used to change dir if the user inputs "cd" command
*/
void changeDir(struct userInput* currCommand)
{
    if (currCommand->args[1] == NULL) {
        chdir(getenv("HOME"));
    }
    else {
        chdir(currCommand->args[1]);
    }    
}

/*
* Used to return status for the last foreground command run
*/
void statusCommand(int status) 
{
    if (WIFEXITED(status)) {
        printf("EXIT VALUE: %d\n", WEXITSTATUS(status));
        fflush(stdout);
    }
    else {
        printf("EXIT VALUE: %d\n", WTERMSIG(status));
        fflush(stdout);
    }
}

/*
* Used to run any user input commands if they are not already built in
*/
void otherCommand(struct userInput* currCommand)
{
    int childStatus;
    pid_t spawnPid = fork();
    switch (spawnPid) {
    case -1:
        perror("fork()\n");
        exit(1);
        break;
    case 0:
        //calls function to redirect input 
        if (currCommand->input_file != NULL && backgroundMode == 0) {
            redirectInput(currCommand);
        }
        //calls function to redirect output
        if (currCommand->output_file != NULL && backgroundMode == 0) {
            redirectOutput(currCommand);
        }
        //redirects SIGINT for foreground processes
        if (currCommand->background == 0 || backgroundMode == 1) {
            SIGINT_action.sa_handler = SIG_DFL;
            SIGINT_action.sa_flags = 0;
            sigaction(SIGINT, &SIGINT_action, NULL);
        }

        if (currCommand->background == 1 && backgroundMode == 0) {
            SIGSTP_action.sa_handler = SIG_DFL;
        }

        signal(SIGTSTP, SIG_IGN);
        execvp(currCommand->command, currCommand->args);

        perror("execvp");
        exit(1);
        break;
    default:
        if (currCommand->background == 0 || backgroundMode == 1) {
            spawnPid = waitpid(spawnPid, &status, 0);
            if (WIFSIGNALED(status))
            {
                printf("terminated by signal %d\n", WTERMSIG(status));
            }
        }
        //adds PID to background processes array
        else if (currCommand->background == 1 && backgroundMode == 0) {
            for (int i = 0; i < LENGTH; i++) {
                if (process[i] == 0) {
                    process[i] = spawnPid;
                    break;
                }
            }
            //doesn't wait for process to finish if it is in the background
            waitpid(spawnPid, &childStatus, WNOHANG);
            printf("Background PID is %d\n", spawnPid);
            fflush(stdout);
        }
    }
}

/*
* Checks to see if any background processes have completed and prints a message if they have
*/
void check_background()
{
    pid_t spawnPID;
    int bgstatus;
    spawnPID = waitpid(-1, &bgstatus, WNOHANG);
    while (spawnPID > 0) {
        for (int i = 0; i < LENGTH; i++) {
            if (process[i] == spawnPID) {
                process[i] = 0;
            }
        }
            if (WIFEXITED(bgstatus)) {
                printf("background pid %d is done. Terminated by signal %d\n", spawnPID, (WEXITSTATUS(bgstatus)));
                fflush(stdout);
            }
            else if (WIFSIGNALED(bgstatus)) {
                printf("background pid %d is done. Terminated by signal %d\n", spawnPID, (WTERMSIG(bgstatus)));
                fflush(stdout);
            }
            spawnPID = waitpid(-1, &bgstatus, WNOHANG);
        }
    }

/*
* redirects user input command so that it can be executed
*/
void executeCommand(struct userInput* currCommand)
{
    if (strcmp(currCommand->command, "cd") == 0) {
        changeDir(currCommand);
    }
    else if (strcmp(currCommand->command, "exit") == 0) {
        exitProgram();
    }
    else if (strcmp(currCommand->command, "status") == 0) {
        statusCommand(status);
    }
    else {
        otherCommand(currCommand);
    }
}


int main()
{
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);


    SIGSTP_action.sa_handler = handle_SIGSTP;
    sigfillset(&SIGSTP_action.sa_mask);
    SIGSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGSTP_action, NULL);

    while (1) {
        check_background();
        char* line = NULL;

        line = getInput();
        while (strncmp(line, "#", strlen("#")) == 0 || (line[0] == '\n')) {
            line = getInput();
        }
        struct userInput* file = createCommand(line);
        executeCommand(file);
    }
}