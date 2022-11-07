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
// global root for the entire tree
extern struct node* root;

#define INPUT_MAX_L EMP_ID_MAX_LENGTH+DESCRIPTION_MAX_LENGTH+LOCATION_MAX_LENGTH+DATETIME_LENGTH+23

typedef struct {
    meeting_request_buf rbuf;
    pthread_mutex_t waitLock;
    pthread_cond_t waitCond;
    int signal;
} threadArg;

void* send_thread(void* arg){
    threadArg *args= (threadArg *)arg;
    //lock
    fprintf(stderr,"before lock\n");
    Pthread_mutex_lock(&sendLock);
    fprintf(stderr,"after lock\n");
    meeting_request_buf test;
    if((msgsnd(msqid, &args->rbuf, SEND_BUFFER_LENGTH, IPC_NOWAIT)) < 0) {
        int errnum = errno;
        fprintf(stderr,"yo\n");
        fprintf(stderr,"%d, %ld, %d, %ld\n", msqid, args->rbuf.mtype, args->rbuf.request_id, SEND_BUFFER_LENGTH);
        perror("(msgsnd)");
        fprintf(stderr, "Error sending msg: %s\n", strerror( errnum ));
        exit(1);
    }
    fprintf(stderr,"after send\n");
    Pthread_mutex_unlock(&sendLock);

    fprintf(stderr,"after lock send\n");

    Pthread_mutex_lock(&args->waitLock);
    while (args->signal < 0) {
        Pthread_cond_wait(&args->waitCond, &args->waitLock);
    }
    int avail = args->signal;
    Pthread_mutex_unlock(&args->waitLock);

    fprintf(stderr,"msgrcv-mtgReqResponse: request id %d  avail %d: \n",args->rbuf.request_id,avail);

    Pthread_cond_destroy(&args->waitCond);
    Pthread_mutex_destroy(&args->waitLock);
    // also decrease active threads

}

void* receive_response(void* arg) {
    int ret;
    meeting_response_buf rbuf;
    do {
      ret = msgrcv(msqid, &rbuf, sizeof(meeting_response_buf)-sizeof(long), 1, 0);//receive type 1 message
      int errnum = errno;
      if (ret < 0 && errno !=EINTR){
        fprintf(stderr, "Value of errno: %d\n", errno);
        perror("Error printed by perror");
        fprintf(stderr, "Error receiving msg: %s\n", strerror( errnum ));
      }
    } while ((ret < 0 ) && (errno == 4));
    fprintf(stderr, "Value received");
    struct node* cur = root;
    while (cur != NULL) {
        if (cur->d > rbuf.request_id) {
            cur = cur->r;
        }
        else if (cur->d < rbuf.request_id) {
            cur = cur->l; 
        }
    }
    Pthread_mutex_lock(cur->mut);
    *cur->avail = rbuf.avail;
    Pthread_cond_signal(cur->cond);
    Pthread_mutex_unlock(cur->mut);
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
        
        Pthread_cond_init(&argument->waitCond);
        Pthread_mutex_init(&argument->waitLock);
        argument->signal=-1;
        
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
        //fprintf(stderr,"%d %s %s %s %s %d\n",argument->rbuf.request_id,argument->rbuf.empId,argument->rbuf.description_string,argument->rbuf.location_string,argument->rbuf.datetime,argument->rbuf.duration);
        Pthread_create(&threadArray[threadCount++], NULL, send_thread, argument);
        if(argument->rbuf.request_id==0){
            break;
        }
    }
}