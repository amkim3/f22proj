#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "meeting_request_formats.h"
#include "queue_ids.h"
#include "common_threads.h"
#include "tree.h"

int msqid;
int msgflg = IPC_CREAT | 0666;
key_t key;

pthread_mutex_t sendLock;
extern struct node* root; // Red-black-tree

#define INPUT_MAX_L EMP_ID_MAX_LENGTH+DESCRIPTION_MAX_LENGTH+LOCATION_MAX_LENGTH+DATETIME_LENGTH+23

typedef struct {
    meeting_request_buf rbuf;
    pthread_mutex_t waitLock;
    pthread_cond_t waitCond;
    int signal;
} threadArg;


void* send_thread(void* arg){
    threadArg *args= (threadArg *)arg;
    Pthread_mutex_lock(&sendLock);
    meeting_request_buf test;
    if((msgsnd(msqid, &args->rbuf, SEND_BUFFER_LENGTH, IPC_NOWAIT)) < 0) {
        int errnum = errno;
        fprintf(stderr,"%d, %ld, %d, %ld\n", msqid, args->rbuf.mtype, args->rbuf.request_id, SEND_BUFFER_LENGTH);
        perror("(msgsnd)");
        fprintf(stderr, "Error sending msg: %s\n", strerror( errnum ));
        exit(1);
    }
    Pthread_mutex_unlock(&sendLock);

    Pthread_mutex_lock(&args->waitLock);
    while (args->signal < 0) {
        Pthread_cond_wait(&args->waitCond, &args->waitLock);
    }
    int avail = args->signal;
    Pthread_mutex_unlock(&args->waitLock);

    if(avail==1){
        fprintf(stdout, "Meeting request %d for employee %s was accepted (%s @ %s starting %s for %d minutes\n",
         args->rbuf.request_id,args->rbuf.empId,args->rbuf.description_string,args->rbuf.location_string,args->rbuf.datetime,args->rbuf.duration);
    }else{
       fprintf(stdout, "Meeting request %d for employee %s rejected due to conflict (%s @ %s starting %s for %d minutes\n",
         args->rbuf.request_id,args->rbuf.empId,args->rbuf.description_string,args->rbuf.location_string,args->rbuf.datetime,args->rbuf.duration); 
    }

    Pthread_cond_destroy(&args->waitCond);
    Pthread_mutex_destroy(&args->waitLock);
}

void* receive_response(void* arg) {
    int ret;
    meeting_response_buf rbuf;
    while (1) {
        do {
            ret = msgrcv(msqid, &rbuf, sizeof(rbuf)-sizeof(long), 1, 0);//receive type 1 message

            int errnum = errno;
            if (ret < 0 && errno !=EINTR){
                fprintf(stderr, "Value of errno: %d\n", errno);
                perror("Error printed by perror");
                fprintf(stderr, "Error receiving msg: %s\n", strerror( errnum ));
            }
        } while ((ret < 0 ) && (errno == 4));
        fprintf(stderr, "Value received\n");


        struct node* cur = root;
        while (cur != NULL) {
            if (cur->d > rbuf.request_id) {
                cur = cur->l;
            }
            else if (cur->d < rbuf.request_id) {
                cur = cur->r; 
            }else{
                break;
            }
        }

        if (cur != NULL) {
            Pthread_mutex_lock(cur->mut);  
            if(rbuf.avail==1){
                *cur->avail=1;
            }else if(rbuf.avail==0){
                *cur->avail=0;
            }
            Pthread_cond_signal(cur->cond);
            Pthread_mutex_unlock(cur->mut);
        }else{
            fprintf(stderr,"node not found");
        }

        fprintf(stderr, "left");
    }
    
}


int main(int argc, char *argv[]){
    //get queue id from file
    key = ftok(FILE_IN_HOME_DIR,QUEUE_NUMBER);
    if (key == 0xffffffff) {
        fprintf(stderr,"Key cannot be 0xffffffff..fix queue_ids.h to link to existing file\n");
        return 1;
    }

    Pthread_mutex_init(&sendLock);

    pthread_t threadArray[200];
    int threadCount = 0;

    pthread_t receiveThread;
    Pthread_create(&receiveThread, NULL, receive_response, NULL);

    if ((msqid = msgget(key, msgflg)) < 0) {
        int errnum = errno;
        fprintf(stderr, "Value of errno: %d\n", errno);
        perror("(msgget)");
        fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
    }
    
    char *input=malloc(INPUT_MAX_L * sizeof(char));
    size_t input_sz = INPUT_MAX_L;

    for(;;){
        threadArg *argument = malloc(sizeof(threadArg));

        // Get line, separate
        int charRead=getline(&input,&input_sz,stdin);
        if(charRead<=1) continue;
        char *save;
        char *str1=strtok_r(input, ",", &save);
        argument->rbuf.mtype=2;   
        argument->rbuf.request_id=atoi(str1);
        str1=strtok_r(NULL, ",", &save);
        strncpy(argument->rbuf.empId,str1,EMP_ID_MAX_LENGTH);
        str1=strtok_r(NULL, ",", &save);
        strncpy(argument->rbuf.description_string,str1,DESCRIPTION_MAX_LENGTH);
        str1=strtok_r(NULL, ",", &save);
        strncpy(argument->rbuf.location_string,str1,LOCATION_MAX_LENGTH);
        str1=strtok_r(NULL, ",", &save);
        strncpy(argument->rbuf.datetime,str1,DATETIME_LENGTH);
        str1=strtok_r(NULL, ",", &save);
        argument->rbuf.duration=atoi(str1);
        
        // Initialize condition variable and lock
        Pthread_cond_init(&argument->waitCond);
        Pthread_mutex_init(&argument->waitLock);
        // Signal
        argument->signal=-1;

        // node for rbt
        struct node* n = (struct node*)malloc(sizeof(struct node));
        n->d = argument->rbuf.request_id;
        n->r = NULL;
		n->l = NULL;
		n->p = NULL;
		n->c = 1;
        n->cond = &argument->waitCond;
        n->mut = &argument->waitLock;
        n->avail = &argument->signal;
        root = bst(root, n);
        fixup(root, n);
        
        if (argument->rbuf.request_id == 0) {
            for(int i=0;i<threadCount;++i){
                Pthread_join(threadArray[i],NULL);
            }
        }
        fprintf(stderr,"%d %s %s %s %s %d\n",argument->rbuf.request_id,argument->rbuf.empId,argument->rbuf.description_string,argument->rbuf.location_string,argument->rbuf.datetime,argument->rbuf.duration);
        Pthread_create(&threadArray[threadCount++], NULL, send_thread, argument);

        if(argument->rbuf.request_id==0){
            break;
        }
    }  
}