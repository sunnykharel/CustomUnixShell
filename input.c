#include <stdio.h> 
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <string.h>
#include "input.h"
#include <sys/types.h>

char * trimwhitespace(char *str);

int main(){
    // signal(SIGTSTP, SIG_IGN);
    if ( signal(SIGINT, SIG_IGN) == SIG_ERR)
        printf("signal(SIGINT) error");
    if ( signal(SIGTTOU, SIG_IGN)== SIG_ERR)
        printf("signal(SIGTTOU) error");
    signal(SIGTSTP, SIG_IGN);

    jobStack_t * jobStack = malloc(sizeof(jobStack_t));
    jobStack->head=NULL; jobStack->size=0;jobStack->maxJobNumber=0;

    int cpid;
    char *inString;
    char ** tokenedInput;
    char * delim = " ";

    while(inString = readline("# ")){
        reapAllChildren(jobStack);
        inString = trimwhitespace(inString);
        if(strlen(inString)==0){
            continue;
        }else if (strcmp(inString, "jobs")==0 ){
            //prints all jobs in the jobStack
            handleJobsCommand(jobStack);
        }else if(strcmp(inString, "bg")==0){
            //pops job off of jobStack and runs it in the background
            handleBgCommand(jobStack);
        }else if(strcmp(inString, "fg")==0){
            //pops job off of jobStack and runs it in the foreground
            handleFgCommand(jobStack);
        }else{
            job_t * job = parseJob(inString);
            job->ppid=getpid();
            executeJob(job, jobStack);
        }
    }
}

job_t * popJob(jobStack_t * jobStack){
    //assume that the jobStack is not empty
    if (jobStack==NULL)
        return NULL;
    if (jobStack->size==0)
        return NULL;
    jobStackNode_t * jobNode = jobStack->head;
    jobStack->head=jobNode->next;
    jobStack->size--;
    return jobNode->job;
}

void pushJob(job_t * job, jobStack_t * jobStack){
    //adds a job to the head of the stack
    //assumes job and jobStack not null for lab purposes
    //set job, set jobs next to head, and make head point to the new jobNode
    jobStackNode_t * jobNode;
    jobNode = malloc(sizeof(jobStackNode_t));
    jobNode->jobNumber=jobStack->maxJobNumber+1;
    jobStack->maxJobNumber+=1;
    jobNode->job = job; jobNode->next=NULL;
    if (jobStack->head==NULL){
        jobStack->head=jobNode;
        jobNode->next=NULL;
    }else{
        jobNode->next = jobStack->head;
        jobStack->head = jobNode;
    }   
    jobStack->size++;
}

char * trimwhitespace(char *str){
    char *end;
    // Trim leading space
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0)  // All spaces?
    return str;
    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    // Write new null terminator character
    end[1] = '\0';
    return str;
}




