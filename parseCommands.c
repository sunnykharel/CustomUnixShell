#include "input.h"
#include <stdio.h> 
#include <unistd.h>
#include <string.h>

struct TokenNode {
    char * token_str;
    struct TokenNode *next;
};
typedef struct TokenNode tokenNode;
struct TokenList {
    int size;
    struct TokenNode * head;
};
typedef struct TokenList tokenList;
char ** parseString(char * str, char * delim);
tokenList * parseStringToLinkedList(char * str, char *delim);
char ** tokenLLtoArray(tokenList * tokenLL);

int checkAmpersand(char * jobString){
    int i=-1;
    for (i = strlen(jobString)-1; i>=0 && jobString[i]==' '; i-- ){}
    if (i < 0 || jobString[i]!='&')
        return -1;
    else
        return i;  
}

job_t * parseJob(char * jobString){
    //assume number of pipes <= 1
    job_t * job = malloc(sizeof(job_t));
    job->processList[0]=NULL;job->processList[1]=NULL;job->jobString=NULL;
    job->jobString = strdup(jobString);
    int ampersandidx = checkAmpersand(jobString);
    bool background = false;
    if (ampersandidx!=-1){
        jobString[ampersandidx]='\0';
        background = true;
    }

    job->n_proc=0; job->background=background; job->status=READY;
    char *token;
    int processCount = 0;
    char * saveptr;
    for ( ;; jobString = NULL){
        token = strtok_r(jobString, "|", &saveptr);
        if (token == NULL){
            break;
        }
        process * command = parseCommand(token);
        job->processList[job->n_proc]=command;
        job->n_proc++;
    }
    return job;
}


process * parseCommand(char * commandString){
    if (commandString==NULL){
        return NULL;
    }
    process * command = malloc(sizeof(process)); 
    command->_commandArgs=NULL; command->_commandName=NULL;
    command->_errFile=NULL;command->_inFile=NULL;command->_outFile=NULL;
    char * commandSegment, * outFile, * inFile, * errFile;
    bool redirectionEncountered = false; 
    char * space = " ";
    char ** tokenedCommand = parseString(commandString, space);
    int i;
    for(i = 0; tokenedCommand[i]!='\0'; i++){
        char * token = tokenedCommand[i];
        char * nextToken = '\0';
        if(tokenedCommand[i+1]!='\0'){
            nextToken = tokenedCommand[i+1];
        }
        bool bool1 = strcmp(token, ">")==0;
        bool bool2 = strcmp(token,"<")==0 ;
        bool bool3 = strcmp(token, "2>")==0;
        if (!redirectionEncountered && (bool1||bool2||bool3)){
            redirectionEncountered=true;
            char ** commandArgs = malloc(sizeof(char *) * (i+1));
            int j;
            for(j=0;j<i;j++){
                commandArgs[j] = tokenedCommand[j];
            }
            commandArgs[j] = '\0';
            command->_commandArgs=commandArgs;
        }
        if(i == 0){
            command->_commandName = token;
        }else if(strcmp(tokenedCommand[i],">")==0 && nextToken[0]){
            command->_outFile = nextToken; i++;
        }else if(strcmp(tokenedCommand[i],"<")==0 && nextToken[0]){
            command->_inFile = nextToken; i++;
        }else if(strcmp(tokenedCommand[i], "2>")==0 && nextToken[0]){
            command->_errFile = nextToken; i++;
        }
    }
    if (redirectionEncountered==false){
        //this means the whole string is the commandarg
        char ** commandArgs = malloc(sizeof(char *) * (i+2));
        int j;
        for(j=0;j<=i;j++){
            commandArgs[j] = tokenedCommand[j];
        }
        commandArgs[j] = '\0';
        command->_commandArgs=commandArgs;
    }

    free(tokenedCommand);
    return command;
}


char ** parseString(char * str, char * delim){
    //malloc an array that you return
    if(str==NULL)
        return NULL;
    tokenList *tokenLL = parseStringToLinkedList(str, delim);
    if (tokenLL->size == 0){
        return NULL;
    }
    char ** tokenArray = tokenLLtoArray(tokenLL);
    return tokenArray;

    //we now have a linked_list of token_nodes
}

tokenList * parseStringToLinkedList(char * str, char * delim){
    tokenList *tokenLL;
    tokenNode *head, *prev, *p;
    head = NULL;
    char *token;
    int nodeCount = 0;
    char * saveptr;
    for ( ;; str = NULL){
        token = strtok_r(str, " ", &saveptr);
        if (token == NULL){
            break;
        }
        p = malloc(sizeof(tokenNode));
        p->token_str = token;
        p->next = NULL;
        if (head==NULL){
            head=p;
        }else{
            prev->next=p;
        }
        prev = p;
        nodeCount++;
    }
    tokenLL = malloc(sizeof(tokenList));
    tokenLL->size = nodeCount;
    tokenLL->head = head;
    return tokenLL;
}

char ** tokenLLtoArray(tokenList * tokenLL){
    tokenNode * node = tokenLL->head;
    char **tokenArray = malloc(sizeof (char *) * (tokenLL->size+1));
    int i=0;

    tokenNode * temp;
    while (node!=NULL){
        temp = node;
        node=node->next;
        tokenArray[i] = temp->token_str;
        free(temp); i++;
    }
    free(tokenLL);
    tokenArray[i] = '\0';
    return tokenArray;
}






