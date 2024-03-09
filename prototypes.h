#include "Header.h"

// function prototype declarations
struct Line;
void redirOut(struct Line* line);
void redirIn(struct Line* line);
void backgroundRedir(struct Line* line);
void changeDir(char* dir);
int getStatus(struct Line* line);
void ignoreSIGINT(bool choice);
void handle_SIGTSP_foreground(int signo);
void handle_SIGTSP_normal(int signo);
int ignoreSIGTSTP(bool choice);
void passCommand(struct Line* line);
int digitCount(int num);
char* expand(char* string, char* newString, char* pid);
void removeReturn(char* string);
int processCommand(char* line, int prevStatus);
void removeFromBGArray(pid_t pid);
void addToBGArray(pid_t pid);
void killAllBG(void);
void getTerminated(void);
void prompt(void);