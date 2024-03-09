// smallsh by David Cornett for CS344 Assignment 3

#include "prototypes.h"
#include "Header.h"
#define MAX_PROCESSES 200  // max amount of background processes this program will support
#define MAX_LINE_CHAR 2048 // max number of characters a line of text from user may contain
#define MAX_ARGS 512       // max amount of args a command may contain

// global array to track current background processes; zeros indicate no child process
int backgroundProcs[MAX_PROCESSES] = { 0 }; 

// for SIGTSTP handler to set commands as foreground-only 
volatile sig_atomic_t foregroundFlag = 0;  // 1 is foreground-only, 0 is normal

// Struct stores elements processed from line entered by user and information about those elements
struct Line {
	char* command;
	char* arguments[MAX_ARGS + 1]; // allow for specified amount of args and termination marker '0'
	int   argCount;		 // tracks number of arguments after command are entered by user
	bool  input;		 // set value means input redirect
	char* inputFile;	 // stores location for input redirect
	bool  output;		 // set value means output redirect
	char* outputFile;	 // stores file for output redirect
	bool  background;	 // true means command should be executed in background, false means foreground execution 
	int   wstatus;		 // stores status of process (used to get exit value)
	int   prevStatus;	 // stores status of previous process (ignores built-in functions)
};

// ----------- REDIRECT FUNCTIONS ------------------------
void redirIn(struct Line* line) {
	// changes stdin to specified read-only file, or returns error 
	int fd = open(line->inputFile, O_RDONLY);
	if (fd == -1) {
		perror("source open()");
		exit(1);
	}
	int inputChange = dup2(fd, STDIN_FILENO); // get standard input (file descript #0) from specified input file
	if (inputChange == -1) {
		perror("source dup2()");
		exit(2);
	}
	close(fd);
}

void redirOut(struct Line* line) {
	// changes stdout to specified write-only file, or returns error
	int fd = open(line->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
	if (fd == -1) {
		perror("open()");
		exit(1);
	}
	
	int outputChange = dup2(fd, STDOUT_FILENO); // point standard output (file descript #1) to specified output file
	if (outputChange == -1) {
		perror("dup2");
		exit(2);
	}
	close(fd);
}

void backgroundRedir(struct Line* line) {
	// INPUT redirect from /dev/null file unless user specifies a redirect
	if (line->input != 1) {
		int fd = open("/dev/null", O_RDONLY); // this file exists in all Linux systems
		if (fd == -1) {
			perror("open()");
			exit(1);
		}
		int inputChange = dup2(fd, STDIN_FILENO);
		if (inputChange == -1) {
			perror("source dup2()");
			exit(2);
		}
		close(fd);
	}

	// OUTPUT redirect to /dev/null file unless user specifies a redirect
	if (line->output != 1) {
		int fd = open("/dev/null", O_WRONLY); // this file exists in all Linux systems
		if (fd == -1) {
			perror("open()");
			exit(1);
		}
		int outputChange = dup2(fd, STDOUT_FILENO);
		if (outputChange == -1) {
			perror("dup2");
			exit(2);
		}
		close(fd);
	}
}
// -------------------------------------------------------

// ----------- SIGNAL HANDLING FUNCTIONS -----------------
void ignoreSIGINT(bool choice) {
	// Initialize SIGINT_action struct to be empty
	struct sigaction SIGINT_action = { 0 };

	// Fill out the SIGINT_action struct
	if (choice) {
		SIGINT_action.sa_handler = SIG_IGN; // ignore SIGINT
	}
	else {
		SIGINT_action.sa_handler = SIG_DFL; // default SIGING
	}

	sigfillset(&SIGINT_action.sa_mask); // Block all catchable signals while handle_SIGINT is running
	SIGINT_action.sa_flags = 0; // No flags set

	sigaction(SIGINT, &SIGINT_action, NULL); // Install our signal handler
}

void handle_SIGTSP_foreground(int signo) {
	// Mesage user and set foreground-only flag
	char const foreMessage[] = "\nEntering foreground-only mode (& is now ignored)\n: ";
	write(STDOUT_FILENO, foreMessage, sizeof foreMessage - 1); // use write() because printf() is non-reentrant
	foregroundFlag = 1;
}

void handle_SIGTSP_normal(int signo) {
	// Mesage user and clear foreground-only flag
	char const normalMessage[] = "\nExiting foreground-only mode\n: ";
	write(STDOUT_FILENO, normalMessage, sizeof normalMessage - 1); // use write() because printf() is non-reentrant
	foregroundFlag = 0;
}

int ignoreSIGTSTP(bool choice) {
	// Initialize SIGINT_action struct to be empty
	struct sigaction SIGTSTP_action = { 0 };

	// Fill out the SIGINT_action struct
	if (choice) {
		SIGTSTP_action.sa_handler = SIG_IGN; // ignore SIGINT
	}
	else {
		//printf("flag: %d\n", foregroundFlag);
		//fflush(stdout);
		if (foregroundFlag) {
			SIGTSTP_action.sa_handler = handle_SIGTSP_normal;
		}
		else {
			SIGTSTP_action.sa_handler = handle_SIGTSP_foreground;
		}
		
	}

	sigfillset(&SIGTSTP_action.sa_mask); // Block all catchable signals while handle_SIGINT is running
	SIGTSTP_action.sa_flags = SA_RESTART; // Allows prompt for next line, skipping execution

	sigaction(SIGTSTP, &SIGTSTP_action, NULL); // Install our signal handler
	return 0;
}
// -------------------------------------------------------

// ------------ EXIT CLEAN-UP FUNCTIONS ------------------
void removeFromBGArray(pid_t pid) {
	/* Locates pid in global arrayand sets to 0, indicating it has ended */
	int i = 0;
	while (i < MAX_PROCESSES) {
		if (backgroundProcs[i] == pid) {
			backgroundProcs[i] = 0;
			break;
		}
		i++;
	}
}

void addToBGArray(pid_t pid) {
	/* Add pid to first empty global array index (filled with 0) */
	int i = 0;
	while (i < MAX_PROCESSES) {
		if (backgroundProcs[i] == 0) {
			backgroundProcs[i] = pid;
			break;
		}
		i++;
	}
}

void killAllBG(void) {
	/* Send kill signal to any still-running pid stored in global array */
	int i = 0;
	while (i < MAX_PROCESSES) {
		if (backgroundProcs[i] > 0) {
			kill(backgroundProcs[i], SIGKILL);
		}
		i++;
	}
}
// -------------------------------------------------------

// ----------- OTHER BUILT-IN COMMAND FUNCTIONS ----------
void changeDir(char* dir) {
	/* Changes current working directory based on parameter
	   Handles absolute and relative directory names */
	int change;

	// If parameter is 0, change to HOME directory
	if (strcmp(dir, "0") == 0) {
		change = chdir(getenv("HOME")); // getenv() gets path to HOME from environment
	}
	// Otherwise, change to directory specified by user
	else {
		change = chdir(dir);
	}
	// return error message is invalid directory was specified
	if (change != 0) {
		printf("No such file or directory\n");
		fflush(stdout);
	}
}

int getStatus(struct Line* line) {
	/* Returns status # of last child process */

	// If process exited without interruption, return its exit status
	if (WIFEXITED(line->wstatus)) {
		return WEXITSTATUS(line->wstatus);
	}
	// If command was interrupted by a signal, return status of the signal
	else if (WIFSIGNALED(line->wstatus)) {
		return WTERMSIG(line->wstatus);
	}
}

// ----------- COMMAND PARSING/PROCESSING FUNCTIONS ------
void passCommand(struct Line* line) {
	/* Passes non-built in command to child process, which becomes replaced by execution call */

	// fill out array of command and args needed for execvp()
	int i = 1; // position of first arg (after command)
	int arrLength = line->argCount + 2; // array must hold all arguments + command + null terminator
	char* execArr[arrLength]; 
	execArr[0] = line->command;

	while (i < arrLength - 1) {
		execArr[i] = line->arguments[i-1];
		i++;
	}
	execArr[i] = NULL; // needed at the of arr for execv()

	// fork child process
	int childStatus;
	pid_t spawnPid = fork();

	switch (spawnPid) {
	case -1:
		perror("fork()\n");
		exit(1);
		break;

	case 0: // child process
		ignoreSIGTSTP(1); // do not allow child process to be interrupted by Control-Z signal

		// handle redirect->IN if needed
		if (line->input == 1) {
			redirIn(line);
		}
		
		// handle redirect->OUT if needed
		if (line->output == 1) {
			redirOut(line);
		}
		if (line->background != 1) {
			ignoreSIGINT(0);
		}
		// attempt to replace child process with execution of command from PATH
		execvp(execArr[0], execArr);
		perror("execvp()"); // return if there is an error
		exit(2);
		break;

	default: // parent process
		/* for foreground execution, wait for child process;
		   for background execution, do not wait and allow user to input another command */

		if (line->background == 1) {
			// alert user that a specific background process has begun
			printf("Beginning background process %d\n", spawnPid);
			fflush(stdout);
			addToBGArray(spawnPid); // add background pid to global array for termination upon exit command
			spawnPid = waitpid(spawnPid, &childStatus, WNOHANG);
		}
		else {
			spawnPid = waitpid(spawnPid, &childStatus, 0);

			// check if user sent kill signal (Control-C) to end the foreground process early
			if (WIFSIGNALED(childStatus)) {
				printf("Terminated by signal %d\n", WTERMSIG(childStatus));
				fflush(stdout);
			}
		}
		line->wstatus = childStatus; // store current exit value for use by getStatus()
		break;
	}
}

int digitCount(int num) {
	/* Returns number of digits of parameter
		Citation: based on code from https ://www.geeksforgeeks.org/program-count-digits-integer-3-different-methods/
		Accessed on 1/27 */
	int count = 0;
	while (num != 0) { // repeatedly /10 to count each digit of various significance
		num = num / 10;
		count++;
	}
	return count;
}

char* expand(char* string, char* newString, char* pid) {
	/* Recursive function to handle expansion of variable $$
	   Replaces variable with pid of current process
	   Returns ptr new string when there are no more variables to expand 
	   Note: if no variables existed, pointer to original string is returned */

	// check if string contains variable and if so, move pointer 2 spaces back in the string. If not, return string as-is.
	char* expansion = strstr(string, "$$");
	if (expansion != NULL) {
		expansion = expansion + 2;

		// allow for increase in length of new string by the length of process ID    
		newString = realloc(newString, strlen(newString) + strlen(pid) + 1);
		// add substring containing old string up until $$ variable to new string
		strncat(newString, string, strlen(string) - strlen(expansion - 2));
		strcat(newString, pid); // add pid to new string
		string = expansion; // move pointer to after $$ variable for next recursive call
		return expand(string, newString, pid);
	}
	strcat(newString, string);
	return newString;
}

void removeReturn(char* string) {
	/* Removes hard return ('\n') from a string if it exists.
	   The last word entered by user need this stripped off for correct processing. */ 
	if (string[strlen(string) - 1] == '\n') {
		string[strlen(string) - 1] = '\0';
	}
}

int processCommand(char* line, int prevStatus) {
	/*  Parses input text into valid shell command and executes that command.
	    Executes built-in commands manaully in the current process.
		Executes other commands by passing them to a child process which executes via execvp().
		Returns status # of current line's command or -999 to indicate that it was a built-in command */

	// initialize line struct and previous line's status
	struct Line* currLine = malloc(sizeof(struct Line));
	currLine->prevStatus = prevStatus;

	// begin parsing of text into command, arguments, redirects, background
	char* saveptr;
	char* token = strtok_r(line, " ", &saveptr);

	// get process id and store in string
	int pid = (int)getpid();
	char* pidStr = malloc(digitCount(pid) * sizeof(char));
	sprintf(pidStr, "%d", pid);

	// if line is a comment or left blank, skip processing
	if (token[0] == '\n' || token[0] == '#') {
		return NULL;
	}

	// get command
	removeReturn(token); // remove hard return if it exists	
	char* newStr = malloc(strlen(token) * sizeof(char));
	currLine->command = expand(token, newStr, pidStr); // set shell command after checking for variable expansion

	// move to first arg
	token = strtok_r(NULL, " ", &saveptr);
	currLine->argCount = 0; // tracks count of args to be kept in array

	// get arguments and file I/O redirection
	while (token != NULL) {
		// handle OUTPUT redirection
		if (strcmp(token, ">") == 0) {
			token = strtok_r(NULL, " ", &saveptr);
			removeReturn(token); // remove hard return if it exists
			currLine->outputFile = calloc(strlen(token) + 1, sizeof(char));
			currLine->output = 1; // set bool
			strcpy(currLine->outputFile, token);
			token = strtok_r(NULL, " ", &saveptr); // move to next token
		}
		// handle INPUT redirection
		else if (strcmp(token, "<") == 0) {
			token = strtok_r(NULL, " ", &saveptr);
			removeReturn(token); // remove hard return if it exists
			currLine->inputFile = calloc(strlen(token) + 1, sizeof(char));
			currLine->input = 1; // set bool
			strcpy(currLine->inputFile, token);
			token = strtok_r(NULL, " ", &saveptr); // move to next token
		}

		// handle BACKGROUND execution
		else if (strcmp(token, "&\n") == 0) {
			/* if line ends with "&" and the program is not in foreground - only mode,
			   the command must be run in the background. 
			   Note: this will NOT apply to built-in commands (cd, exit, status) */
			if (foregroundFlag == 0) {
				currLine->background = 1;
			}
			break;
		}

		// handle ARGUMENTS
		else {
			removeReturn(token); // remove hard return if it exists		
			// if the token is empty after removeReturn, it was a hard return and so iteration can stop
			if (strlen(token) == 0) {
				token = NULL;
			}
			// only add additional argument if a redirect has not been reached yet
			else if (currLine->input != 1 && currLine->output != 1) {
				char* argStr = malloc(strlen(token) * sizeof(char));
				currLine->arguments[currLine->argCount] = expand(token, argStr, pidStr); // add new arg after checking for variable expansion
				token = strtok_r(NULL, " ", &saveptr);
				currLine->argCount++;
			}
		}
	}
	currLine->arguments[currLine->argCount] = "0"; // mark termination of array

	// execute built-in commands if applicable
	if (strcmp(currLine->command, "cd") == 0) {
		changeDir(currLine->arguments[0]);
		return -999; // -999 return prevents update of status for current line
	}
	else if (strcmp(currLine->command, "exit") == 0) {
		killAllBG(); // kill any running background processes
		exit(0); // terminate shell
	}
	else if (strcmp(currLine->command, "status") == 0) {
		// output most recent non-built-in command's status
		printf("Exit value %d\n", currLine->prevStatus);
		fflush(stdout);
		return -999; // -999 return prevents update of status for current line
	}
	// pass command to execute in child process
	else {
		passCommand(currLine); // note: this will update status for current line
		return getStatus(currLine);
	}
}

void prompt(void) {
	/* Repeatedly get and process user input until exit command is called */
	char* line;
	int prevStatus = 0;

	while (1) {
		// before giving user another line, check for any background processes that have been terminated
		getTerminated();
		ignoreSIGTSTP(0);

		line = calloc(MAX_LINE_CHAR, sizeof(char));

		// show command line prompt to user
		printf(": ");
		fflush(stdout);

		// get each char from user and add to line
		int count = 0;
		int inputC = 0;
		while (inputC != '\n') {
			inputC = fgetc(stdin);
			line[count] = (char)inputC;
			count++;
		}
		// process line into command, arguments, redirects, and background command
		int currStatus = processCommand(line, prevStatus);

		// unless this line was a built-in function (indicated by -999), set previous exit value for the next iteration
		if (currStatus != -999) {
			prevStatus = currStatus;
		}
		free(line);
	}
}

void getTerminated(void) {
	/* if a child has been terminated, print a message depending on if it was successfull or terminated
	   early from a signal.  Call function again to check for additional terminated children. */
	int backChildStatus;
	int backPID = waitpid(-1, &backChildStatus, WNOHANG); // this waitpid() returns PID of next terminated process

	if (backPID > 0) {
		if (WIFEXITED(backChildStatus)) {
			printf("Background pid %d is done: exit value %d\n", backPID, backChildStatus);
		}
		else if (WIFSIGNALED(backChildStatus)) {
			printf("Background pid %d is done: terminated by signal %d\n", backPID, WTERMSIG(backChildStatus));
		}
		fflush(stdout);
		removeFromBGArray(backPID); // remove terminated child from global array
		return getTerminated(); // check for any additional terminated children
	}
}
// -------------------------------------------------------


void main(void) {

	// set program to ignore control-C program termination but allow use of control-C to stop child process
	ignoreSIGINT(1); 

	prompt(); // begin shell for user

}