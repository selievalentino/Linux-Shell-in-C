#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <errno.h>

#include <sys/wait.h>

#include <ctype.h>

#include <regex.h>

#include <sys/stat.h>

#include <dirent.h>

#include <signal.h>

#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

# define MAX_COMMAND_LENGTH 128
# define MAX_NUMBER_OF_PARAMS 10
# define HISTORY_COUNT 10
# define MAX_BG_JOBS 100

struct Jobs {
  pid_t jobId;
  char * jobName;
  int jobsCount;
};
struct Jobs bgJobs[MAX_BG_JOBS];

void printWorkingDir();
void executeBuiltinCmd(char ** params, char * hist[], char cmd[], int currentHand);
void addJobToTable(struct Jobs jobs[], pid_t pid, char * jobName);
void removeJobFromTable(struct Jobs bgJobs[], pid_t pid);
void showJobs(struct Jobs jobs[]);
int isExitSafe();
int executeCmd(char ** params, char * hist[], char cmd[], int currentHand);
int isBuiltinCmd(char * command);
int isBgProcess(char * cmd, char ** params);
int cd(char * pth);
int kill_process(pid_t pid);
int executeCmdFromHistory(char * hist[], char cmd[], int currentHand);
int history(char * hist[], int currentHand);
int clear_history(char * hist[]);
int getHistoryNumber(char cmd[]);
char * getHistoryParameter(char * history[], char cmd[]);
char ** parseCmd(char * cmd, char ** params);
int Pipe(char * cmd, char ** params);

int historySize = 0;
char child_pids[1500];
int redirect=0;
int piped=0;
int jobsCount;
int cmdCount;
char *cat_args[MAX_NUMBER_OF_PARAMS + 1];
char *grep_args[MAX_NUMBER_OF_PARAMS + 1];


int main() {
  char cmd[MAX_COMMAND_LENGTH + 1];
  char fullCommand[MAX_COMMAND_LENGTH + 1];
  char * params[MAX_NUMBER_OF_PARAMS + 1];
 
  char * historyBuffer[HISTORY_COUNT];

  int historyCount = 0;
  cmdCount = 0;
  jobsCount = 0;

  int i, currentHand = 0;
  for (i = 0; i < HISTORY_COUNT; i++) historyBuffer[i] = NULL;

  while (1) {
    printWorkingDir();

    // Read command from standard input, exit on Ctrl+D
    if (fgets(cmd, sizeof(cmd), stdin) == NULL) break;
    if (strcmp(cmd, "\n") == 0) continue; // no input so loop again. if not included empty history is recorded
    // Remove trailing newline character, if any
    if (cmd[strlen(cmd) - 1] == '\n') {
      cmd[strlen(cmd) - 1] = '\0';
    }

    //free the history
    free(historyBuffer[currentHand]);
    if ((strncmp(cmd, "!", 1) != 0)) {
      historyBuffer[currentHand] = strdup(cmd);
      if (historySize < HISTORY_COUNT)
        historySize++; //track histororySize coz couldt be retrieved by sizeof char*   
      currentHand = (currentHand + 1) % HISTORY_COUNT;
    } else {
      currentHand = (currentHand) % HISTORY_COUNT;
    }

    parseCmd(cmd, params);

    // Execute command
    if (strncmp(cmd, "!", 1) == 0) executeCmdFromHistory(historyBuffer, cmd, currentHand);
    if (isBuiltinCmd(cmd)) executeBuiltinCmd(params, historyBuffer, cmd, currentHand);
    else if (executeCmd(params, historyBuffer, cmd, currentHand) == 0) break;

  }
  clear_history(historyBuffer);
  return 0;

}

int executeCmd(char ** params, char * historyBuffer[], char cmd[], int currentHand) {

  pid_t pid = fork();
  if (pid == -1) {
    char * error = strerror(errno);
    printf("fork: %s\n", error);
    return 0;
  } 
  else if (pid == 0) {
    //printf("pid %d\n", getpid());
	
	
    if (isBgProcess(cmd, params)) cmd[strlen(cmd) - 1] = '\0'; //remove the '&' char
	
	//added for redirecting
	
	int i,in=0,out=0;
    char input[64],output[64];

    // finds where '<' or '>' occurs and make that argv[i] = NULL , to ensure that command wont't read that
   int index1=MAX_NUMBER_OF_PARAMS,index2=MAX_NUMBER_OF_PARAMS;
    for(i=0;params[i]!='\0';i++)
    {
        if(strcmp(params[i],"<")==0)
        {        
            params[i]="\0";
            strcpy(input,params[i+1]);
            in=1;
            index1=i;
            redirect=1;			
        }               

        if(strcmp(params[i],">")==0)
        {      
            params[i]="\0";
            strcpy(output,params[i+1]);
            out=2;
			index2=i;
			redirect=1;
			
        } 
       
    }
	
	

    //if '<' char was found in string inputted by user
    if(in==1)
    {   

        // fdo is file-descriptor
        int fd0;
        if ((fd0 = open(input,O_RDONLY)) < 0) {
		    
            perror("Couldn't open input file");
            exit(0);
        } 
		
			 
        // dup2() copies content of fdo in input of preceeding file
        dup2(fd0, STDIN_FILENO); // STDIN_FILENO here can be replaced by 0 
        

        close(fd0); // necessary
		for(int i=index1;i<MAX_NUMBER_OF_PARAMS;i++)
			   params[i]="\0";
		
    }
	

    //if '>' char was found in string inputted by user 
    if (out==2)
    {
        //avoidErrorMsg=true;
        int fd0 ;
        if ((fd0 = creat(output , 0644)) < 0) {
            perror("Couldn't open the output file");
            exit(0);
        }           

        dup2(fd0, STDOUT_FILENO); // 1 here can be replaced by STDOUT_FILENO
        close(fd0);
    }
	
	  int posOfpipe=0;
	  for(i=0;params[i]!='\0';i++)
    {
        if(strcmp(params[i],"|")==0)
        {   posOfpipe=i;    
            params[i]="\0";
            
            piped=1;
			break;
        } 
	}
	if(piped==1)
	{
	for(int i=0;i< posOfpipe;i++)
	   grep_args[i]=params[i];
	for(int i=posOfpipe+1;i<MAX_NUMBER_OF_PARAMS;i++)
	{
	   cat_args[i-posOfpipe-1]=params[i];
	}
	
	return Pipe(cmd,params);
	}
	
    	
     
	
    if(piped!=1&& redirect!=1)
    execvp(cmd, params);
    // Error occurred
    char * error = strerror(errno);
    printf("shell: %s: %s\n", params[0], error);

  }
  // Parent process
  else {
    if (isBgProcess(params[0], params)) {
      cmd[strlen(cmd) - 1] = '\0';
      if (!isBuiltinCmd(cmd)) {
        addJobToTable(bgJobs, pid, cmd);
        // printf("childPid first: %d\n", bgJobs[0].jobId);  
        return 1;
      }
    }

    int status;
    int i = 0;
    while ((pid = waitpid(-1, & status, WUNTRACED)) > 0) {
      for (int i = 0; i < jobsCount; i++) {
        if (pid == bgJobs[i].jobId) {
          printf("[proc %d exited with code %d]\n", pid, WEXITSTATUS(status));
          removeJobFromTable(bgJobs, pid);
        }

      }
    }

  }

  return 1;

}
void executeBuiltinCmd(char ** params, char * historyBuffer[], char cmd[], int currentHand) {
  if (strcmp(params[0], "history") == 0) history(historyBuffer, currentHand);

  if (strcmp(params[0], "hc") == 0) clear_history(historyBuffer);

  if (strcmp(params[0], "cd") == 0) cd(params[1]);

  if (strcmp(params[0], "$PATH") == 0) printf("%s\n", getenv("PATH"));

  if (strcmp(params[0], "jobs") == 0) {
    showJobs(bgJobs);
  }
  if (strcmp(params[0], "exit") == 0) {
    if (isExitSafe()) exit(0);
    else printf("kill'em first\n");
  }
  if (strcmp(params[0], "kill") == 0) {
    if ((params[1] != NULL) && (!strcmp(params[1], "") == 0)) {

      if (strncmp(params[1], "%", 1) == 0) {
        char temp[20];
        for (int i = 0; i <= strlen(cmd); i++) {
          temp[i] = cmd[i + 6];
        }
        int tobeKilledId;
        sscanf(temp, "%d", & tobeKilledId);
        if (!kill_process(tobeKilledId)) printf("No jobs by that ID\n");
        else removeJobFromTable(bgJobs, tobeKilledId);

        memset(temp, 0, sizeof(temp));

      }
    } else {
      printf("kill must be followed by a valid job id\n");
    }
  }

}
int executeCmdFromHistory(char * historyBuffer[], char cmd[], int currentHand) {
  char * paramsFromHistory[MAX_NUMBER_OF_PARAMS + 1];
  char * cmdFromHistory = getHistoryParameter(historyBuffer, cmd);
  if (cmdFromHistory != NULL) {
    parseCmd(cmdFromHistory, paramsFromHistory);
    //printf("Param from history: %s\n", paramsFromHistory[1]); //returns null if no arguments

    if (isBuiltinCmd(cmdFromHistory)) executeBuiltinCmd(paramsFromHistory, historyBuffer, cmdFromHistory, currentHand);
    else executeCmd(paramsFromHistory, historyBuffer, cmdFromHistory, currentHand);

  }
}

int cd(char * pth) {
  char path[MAX_COMMAND_LENGTH];
  strcpy(path, pth);

  char cwd[MAX_COMMAND_LENGTH];
  if (pth[0] != '/') {
    getcwd(cwd, sizeof(cwd));
    strcat(cwd, "/");
    strcat(cwd, path);
    chdir(cwd);
  } else {
    chdir(pth);
  }
  return 0;
}
int isExitSafe() {
  if (jobsCount == 0) return 1;
  printf("%d Jobs are running ", jobsCount);

  return 0;
}
int kill_process(pid_t pid) {
  int isKilled = 0;
  for (int i = 0; i < jobsCount; i++) ///
  {
    if (bgJobs[i].jobId == pid) {
      kill(pid, SIGKILL);
      isKilled = 1;
      break;
    }
  }
  return isKilled;
}
void showJobs(struct Jobs jobs[]) {
  if (jobsCount > 0)
    for (int i = 0; i < jobsCount; i++)
      printf("[%d] %d\n", i + 1, jobs[i].jobId);
  else printf("No background jobs running\n");

}
void addJobToTable(struct Jobs bgJobs[], pid_t pid, char * jobName) {
  bgJobs[jobsCount].jobId = pid;
  bgJobs[jobsCount].jobName = jobName;
  jobsCount++;

}
void removeJobFromTable(struct Jobs bgJobs[], pid_t pid) {
  int tobeRemovedIndex;
  for (int i = 0; i < jobsCount; i++) {
    if (pid == bgJobs[i].jobId) {
      tobeRemovedIndex = i;
      break;
    }
  }
  for (tobeRemovedIndex; tobeRemovedIndex < jobsCount; tobeRemovedIndex++) {
    bgJobs[tobeRemovedIndex] = bgJobs[tobeRemovedIndex + 1];
  }
  jobsCount--;
}
int isBuiltinCmd(char * command) {

  if ((strcmp(command, "kill") == 0) || (strcmp(command, "$PATH") == 0) || (strcmp(command, "exit") == 0) || (strcmp(command, "history") == 0) || (strcmp(command, "hc") == 0) || (strcmp(command, "hc") == 0) ||
    (strcmp(command, "cd") == 0) || (strcmp(command, "jobs") == 0) || (strcmp(command, "kill") == 0))
    return 1;

  return 0;
}
int isBgProcess(char * cmd, char ** params) {
  if (cmd[strlen(cmd) - 1] == '&' || strcmp(params[0], "&") == 0)
    return 1;

  return 0; //if it has & but not a forkable process or if it doesnt have &.
}
void printWorkingDir() {
  char cwd[MAX_COMMAND_LENGTH];
  char * getcwd(char * buf, size_t size);
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    char * username = getenv("USER");
    printf("%s@comand%d> ", cwd, ++cmdCount);

  } else {
    perror("getcwd() error");
  }
}
int history(char * historyBuffer[], int currentHand) {
  int i = currentHand;
  int hist_num = 1;

  do {
    if (historyBuffer[i]) {
      printf("%4d  %s\n", hist_num, historyBuffer[i]);
      hist_num++;
    }

    i = (i + 1) % HISTORY_COUNT;

  } while (i != currentHand);

  return 0;
}

int clear_history(char * historyBuffer[]) {
  int i;
  for (i = 0; i < HISTORY_COUNT; i++) {
    free(historyBuffer[i]);
    historyBuffer[i] = NULL;
  }
  historySize = 0;
  return 0;
}

int getHistoryNumber(char cmd[]) {
  int valid = 0;
  char temp[2];
  for (int i = 1; i < strlen(cmd); i++) {
    if ((cmd[i] > '0') && (cmd[i] <= '9')) valid = 1;

    else if (strcmp(cmd, "!-1") == 0) {
      valid = 1;
      break;
    } else {
      valid = 0;
      break;
    }

  }
  if (valid) {
    for (int i = 1; i < strlen(cmd); i++) {
      temp[i - 1] = cmd[i];
    }
    int commandNumber;
    sscanf(temp, "%d", & commandNumber);
    return commandNumber;
    memset(temp, 0, sizeof(temp));
  } else {
    return 0;
    memset(temp, 0, sizeof(temp));
  }

}
char * getHistoryParameter(char * history[], char cmd[]) {
  int historyNumber = getHistoryNumber(cmd);
  if ((historyNumber != 0)) { //if the history number is valid
    if ((historyNumber > 0) && (historyNumber <= historySize)) return history[historyNumber - 1];
    else if (historyNumber == -1) {
      //  int lastItem = (sizeof(*history)/sizeof(history[0]));
      //printf("\nhistorysize: %d\n", historySize);
      return history[historySize - 1]; //return the most recent history
    } else {
      printf("History does not exist\n");
      return NULL;
    } //when input is like !-17

  } else {
    printf("Invalid entry\n");
    return NULL;
  }

}
char ** parseCmd(char * cmd, char ** params) {
  for (int i = 0; i < MAX_NUMBER_OF_PARAMS; i++) {
    params[i] = strsep( & cmd, " ");
    if (params[i] == NULL) break;
  }
  return params;

}


int Pipe(char * cmd, char ** params)
{
  int pipefd[2];
  int pid;
  pipe(pipefd);
  pid = fork();

  if (pid == 0)
    {
      // child gets here and handles "grep Villanova"

      // replace standard input with input part of pipe

      dup2(pipefd[0], 0);

      // close unused hald of pipe

      close(pipefd[1]);

      // execute grep
	  execvp(cat_args[0], cat_args);

      
    }
  else
    {
      // parent gets here and handles "cat scores"

      // replace standard output with output part of pipe

      dup2(pipefd[1], 1);

      // close unused unput half of pipe

      close(pipefd[0]);

      // execute cat
      execvp(grep_args[0], grep_args);
      
    }
	exit(1);
}
