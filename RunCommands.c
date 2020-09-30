#include "input.h"
#include <stdio.h> 
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>

void executeSimpleJob(job_t * job, jobStack_t * jobStack);
void executeComplexJob(job_t * job, jobStack_t * jobStack);

void runCommand(process * command){
    int readfd, writefd, errorfd;
    if( command->_outFile){
        writefd = open(command->_outFile, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
        dup2(writefd, 1);
    }if(command->_inFile){
        if( access( command->_inFile, F_OK ) != -1 ) {
            readfd = open(command->_inFile, O_RDONLY, 0);
            dup2(readfd, 0);
        } else {
            printf("%s does not exist!\n", command->_inFile);
            return;
        }
    }if(command->_errFile){
        errorfd = open(command->_errFile, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
        dup2(errorfd, 2);
    }
    execvp(command->_commandArgs[0], command->_commandArgs);
}

void executeJob(job_t * job, jobStack_t * jobStack){
    if(job->n_proc==1){
        executeSimpleJob(job, jobStack);
    }else if(job->n_proc==2){
        executeComplexJob(job, jobStack);
    }
}

void executeSimpleJob(job_t * job, jobStack_t * jobStack){
    job->status=RUNNING;
    pid_t cpid;
    int status;
    cpid = fork();
    if (cpid == 0){
        setpgid(0,0);
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        process * command = job->processList[0];
        runCommand(command);
        freeProcess(command);
        job->processList[0]=NULL;
        exit(0);
    }
    job->pgid=cpid;
    if(!job->background){
        pid_t ppid = getpgrp();
        tcsetpgrp(0, cpid);
        waitpid(cpid, &status, WUNTRACED);
        if (WIFEXITED(status)){
            freeJob(job);
        }
        else if(WIFSIGNALED(status)){
            if(WTERMSIG(status)==SIGINT){
                free(job);
            }
        }else if (WIFSTOPPED(status)){
            if (WSTOPSIG(status)==SIGTSTP){
                job->status=SUSPENDED;
                pushJob(job, jobStack);
            } 
        }
        // waitpid(cpid, &status, WUNTRACED);
        tcsetpgrp(0, ppid);
    }else{
        pushJob(job, jobStack);
    }
}

void executeComplexJob(job_t * job, jobStack_t * jobStack){
    job->status=RUNNING;
    int pipefd[2], status1, status2;
    pid_t lcpid, rcpid;
    pipe(pipefd);
    lcpid = fork();
    if(lcpid==0){
        setpgid(0,0);
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO); 
        runCommand(job->processList[0]);
        freeProcess(job->processList[0]);
        job->processList[0]=NULL;
        exit(0);
    }
    job->pgid=lcpid;
    rcpid = fork();
    if (rcpid==0){
        setpgid(0, lcpid);
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        runCommand(job->processList[1]);
        freeProcess(job->processList[1]);
        job->processList[1]=NULL;
        exit(0);
    }
    close(pipefd[0]);
    close(pipefd[1]);
    if (!job->background){
        pid_t ppid = getpgrp();
        tcsetpgrp(0, lcpid);
        waitpid(lcpid, &status1, WUNTRACED);
        // waitpid(rcpid, &status2, WUNTRACED); 
        if (WIFEXITED(status1)){
            freeJob(job);
        }
        else if(WIFSIGNALED(status1)){
            if(WTERMSIG(status1)==SIGINT){
                free(job);
            }
        }else if (WIFSTOPPED(status1)){
            if (WSTOPSIG(status1)==SIGTSTP){
                job->status=SUSPENDED;
                pushJob(job, jobStack);
            } 
        }
        tcsetpgrp(0, ppid);
    }else{
        pushJob(job, jobStack);
    }
}

// pops job off of jobStack and runs it in the background
void handleJobsCommand(jobStack_t * jobStack){
    //does a print of all jobs according to the format
    if (jobStack->head!=NULL){
        handleJobsRecursive(jobStack->head, 0);
        printf("\n");
    }
}
void handleJobsRecursive(jobStackNode_t * jNode, int depth){
    if (jNode==NULL){
        return;
    } else{
        handleJobsRecursive(jNode->next, depth+1);
        char * jstat="\0";
        if (jNode->job->status==1){
            jstat = "Running";
        } else if (jNode->job->status == 2){
            jstat="Stopped";
        } else if (jNode->job->status == 3){
            jstat="Done";
        }
        if (depth==0){
            printf("[%d] + %s      %s",jNode->jobNumber,jstat, jNode->job->jobString);
        }else{
            printf("[%d] - %s      %s\n",jNode->jobNumber ,jstat ,jNode->job->jobString);
        }
    }
}
void handleBgCommand(jobStack_t * jobStack){
    //we loop through the jobStack and look for first SUSPENDED jobNode
    jobStackNode_t * jobNode=NULL;
    for (jobNode=jobStack->head; jobNode!=NULL && jobNode->job->status!=SUSPENDED; jobNode=jobNode->next){}
    if (jobNode!=NULL){
        //send the SIGCONT signal to every child in the process
        //print process to stdout
        jobNode->job->status=RUNNING;
        int idx;
        if ((idx=checkAmpersand(jobNode->job->jobString))==-1)
            jobNode->job->jobString = strcat(jobNode->job->jobString, " &");
        printf(jobNode->job->jobString);printf("\n");
        kill(-1*jobNode->job->pgid, SIGCONT);
    }
} 

void handleFgCommand(jobStack_t * jobStack){
    //we remove the most recent stopped process/background process
    jobStackNode_t * prev=NULL;
    jobStackNode_t * jobNode=NULL;
    for (jobNode=jobStack->head; jobNode!=NULL && jobNode->job->status!=SUSPENDED && jobNode->job->status!=RUNNING; jobNode=jobNode->next){
        prev = jobNode;
    }
    if (jobNode != NULL){
        //remove the job from the stack
        //need to adjust the highest number in the stack
        if (prev == NULL){//i.e. we pop the head
            jobStack->head=jobNode->next;
        }else{
            prev->next=jobNode->next;
        }
        job_t * job = jobNode->job;
        free(jobNode);
        
        //modify jobstring accordingly
        int idx = -1;
        if ((idx = checkAmpersand(job->jobString) )!=-1)
            job->jobString[idx]='\0';
        printf(job->jobString);printf("\n");
        jobStack->size--;
        jobStack->maxJobNumber = getMaxJobNum(jobStack);
        kill(-1*job->pgid, SIGCONT);
        tcsetpgrp(0,job->pgid);
        pid_t ppid = getpgrp();
        int status;
        // waitpid(job->pgid, &status, WUNTRACED);
        waitpid(job->pgid, &status, WUNTRACED);
        if (WIFEXITED(status)){
            freeJob(job);
        }
        else if(WIFSIGNALED(status)){
            if(WTERMSIG(status)==SIGINT){
                freeJob(job);
            }else if (WTERMSIG(status)==SIGCONT){
            }
        }else if (WIFSTOPPED(status)){
            if (WSTOPSIG(status)==SIGTSTP){
                job->status=SUSPENDED;
                pushJob(job, jobStack);
            } 
        }
        tcsetpgrp(0, ppid);
    }
}

void reapAllChildren(jobStack_t * jobStack){
    //while loop that reaps all children
    //does a nohang untraced while loop
    pid_t pid;
    int stat;
    while ( (pid=waitpid(-1, &stat, WNOHANG | WUNTRACED))>0 ){
        if (WIFEXITED(stat)){
            job_t * job = getJobWithPid(jobStack, pid, true);
            if (job!=NULL)
                job->status=TERMINATED; 
            //remove pid number from stack
            //print(DONE thing over here, on the next enter command) (not that important)
        } else if (WIFSTOPPED(stat)){
            job_t * job = getJobWithPid(jobStack, pid, false);
            if (job)
                job->status=SUSPENDED;
        }
        //need to catch if the process  
        /*
            * exited normally terminated
            * it was put into a suspended state by a ctrl Z or 
            * put into a terminated state with ctrlC
        **/
    } 
}

job_t * getJobWithPid(jobStack_t * jobStack, pid_t pid, bool remove){
    jobStackNode_t * jobNode=NULL;
    jobStackNode_t * prev=NULL;
    job_t * job = NULL;
    int count;
    for (jobNode=jobStack->head, count=0; jobNode!=NULL && jobNode->job->pgid!=pid; jobNode=jobNode->next, count++){
        prev = jobNode; count++;
    }
    if (jobNode != NULL){
        if (remove){
            if (prev == NULL){//i.e. we pop the head
                jobStack->head=jobNode->next;
            }else{
                prev->next=jobNode->next;
            }
            jobStack->size--;
            jobStack->maxJobNumber = getMaxJobNum(jobStack);
            job = jobNode->job;
            if (count==0)
                printf("[%d] + %s      %s\n",jobNode->jobNumber,"Done", jobNode->job->jobString);
            else
                printf("[%d] - %s      %s\n",jobNode->jobNumber,"Done", jobNode->job->jobString);
            free(jobNode);
        }
        return job;
    }
    return NULL;
}

int getMaxJobNum(jobStack_t * jobStack){
    if (jobStack->head==NULL)
        return 0;
    jobStackNode_t * node  = NULL;
    int newMaxJobNum = -1;
    for (node=jobStack->head; node!=NULL; node = node->next){
        if (node->jobNumber>newMaxJobNum)
            newMaxJobNum = node->jobNumber;
    }
    return newMaxJobNum;//need to change this later
}


void freeProcess(process * proc){
    if (proc->_commandArgs!=NULL){
        free(proc->_commandArgs);
    }
    free(proc);
}

void freeJob(job_t * job){
    if (job->jobString)
        free(job->jobString);
    free(job);
}

