# Spring21_cs300_project

Distributed Softball Statistics Tracker
You will complete a multithreaded two process system that communicates via System V message queues.  The system goal is to collect game time statistics, calculate player statistics that include previous results in a file and print out game performance include a comparison of the overall and game batting average.

Git Repository for project files

## All commands and code have been tested on cs-operatingsystems01.ua.edu


- Set up JAVA_HOME environment variable

`export JAVA_HOME=/usr/java/latest`

- Compile java files in edu/cs300 packages

`javac edu/cs300/*java`

- Create header file for System V C message functions

`javac -h . edu/cs300/MessageJNI.java`

- Compile C native functions into a library for use with MessageJNI

`gcc -c -fPIC -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux system5_msg.c -o edu_cs300_MessageJNI.o`

`gcc -shared -o libsystem5msg.so edu_cs300_MessageJNI.o -lc`


- Compile test send and receive functions

`gcc -std=c99 -D_GNU_SOURCE msgsnd_atbat.c -o msgsnd`

`gcc -std=c99 -D_GNU_SOURCE msgrcv_stats.c -o msgrcv`


## Commands to run sample programs

- Create a test atbat message and puts it on the queue

`./msgsnd`

```
msgget: msgget succeeded: msgqid = 262144
msgsnd-atbat: player id"#16" Sent (13 bytes)
```

- Java program reads queue contents using native C function and creates and sends a response message via the System V msg queue

`java -cp . -Djava.library.path=. edu.cs300.MessageJNI`

```
Running MessageJNI test routine
JNI msgrcv-atbat: Player #16 pitch results=BKBKFFBO ret bytes rcv'd 13
16:BKBKFFBO
msgget: msgget succeeded: msgqid = 262144
JNI msgsnd-stats: Record 1 of 1: Bailey Hemphill(#16) strikeouts=1, walks=3, singles=1, doubles=0, triples=0, home runs=3, game avg=0.500000, overall avg=0.250000 successfully sent
```

- Retrieves message from System V queue and prints it

`./msgrcv`

```
msgget: msgget succeeded: msgqid = 262144
msgrcv-stats: msg type-2, Player stats 1 of 1 for Bailey Hemphill(#16) strikeouts=1, walks=3, singles=1, doubles=0, triples=0, home runs=3, game avg=0.500000 overall avg=0.250000 ret/bytes rcv'd=96
```

- Program to illustrate use of Java threading and BlockingArrayQUEUE

`java edu.cs300.PlayerStatsTracker`



## Verify the message queue setup

```
> cat ./queue_ids.h 
#define CRIMSON_ID "/home/anderson/anderson"
#define QUEUE_NUMBER 20 //day of birth
> ls -lt  /home/anderson/anderson
-rw-r--r-- 1 anderson users 9 Feb 26  2020 /home/anderson/anderson
```


## Commands to check/delete message queues

- Determine the status of your message queue via `ipcs -a`

```
------ Message Queues --------
key        msqid      owner       perms      used-bytes   messages    
0x0d03bc96 0          crimson2      666        0            0           
0xffffffff 98305      crimson3      666        0            0           
0x0303fabb 65538      crimson1      666        0            0           
**0x0c030904 131075     anderson    666        13            1**           
------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status      
------ Semaphore Arrays --------
key        semid      owner      perms      nsems     
```

*0x0C030904 is the queue created by `./msgsnd`.  It has 1 message of 13 bytes.*


- Removes the queue along with any messages `ipcrm -Q 0x0c030904`

```
------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages    
0x0d03bc96 0          crimson2    666        0            0           
0xffffffff 98305      crimson3    666        0            0           
0x0303fabb 65538      crimson1    666        0            0           

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status      
------ Semaphore Arrays --------
key        semid      owner      perms      nsems
```

>Note:  If your queue key is 0x0xffffffff, you must follow the directions to create the queue file in your home directory and update queue_ids.h

