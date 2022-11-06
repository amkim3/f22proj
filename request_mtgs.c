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

int msqid;
int msgflg = IPC_CREAT | 0666;
key_t key;

#define INPUT_MAX_L EMP_ID_MAX_LENGTH+DESCRIPTION_MAX_LENGTH+LOCATION_MAX_LENGTH+DATETIME_LENGTH+23

int main(int argc, char *argv[]){
    //get queue id from file
    key = ftok(FILE_IN_HOME_DIR,QUEUE_NUMBER);
    if (key == 0xffffffff) {
        fprintf(stderr,"Key cannot be 0xffffffff..fix queue_ids.h to link to existing file\n");
        return 1;
    }

    if ((msqid = msgget(key, msgflg)) < 0) {
        int errnum = errno;
        fprintf(stderr, "Value of errno: %d\n", errno);
        perror("(msgget)");
        fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
    }
    
    char input[INPUT_MAX_L];
    size_t input_sz = INPUT_MAX_L;

    for(;;){
        meeting_request_buf rbuf;
        int charRead=getline(&input,&input_sz,stdin);
        if(charRead<=1) continue;
        char *save;
        char *str1=strtok_r(input, ",", &save);
        rbuf.request_id=atoi(str1);
        str1=strtok_r(NULL, ",", &save);
        strncpy(rbuf.empId,str1,EMP_ID_MAX_LENGTH);
        str1=strtok_r(NULL, ",", &save);
        strncpy(rbuf.description_string,str1,DESCRIPTION_MAX_LENGTH);
        str1=strtok_r(NULL, ",", &save);
        strncpy(rbuf.location_string,str1,LOCATION_MAX_LENGTH);
        str1=strtok_r(NULL, ",", &save);
        strncpy(rbuf.datetime,str1,DATETIME_LENGTH);
        str1=strtok_r(NULL, ",", &save);
        rbuf.duration=atoi(str1);
        
        fprintf(stderr,"%d %s %s %s %s %d",rbuf.request_id,rbuf.empId,rbuf.description_string,rbuf.location_string,rbuf.datetime,rbuf.duration);

        if(rbuf.request_id==0){
            break;
        }
    }
}