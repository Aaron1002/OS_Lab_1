#include "receiver.h"

#define MAX_MSG_SIZE 1024
#define MSG_PASSING 1
#define SHARED_MEM 2

#define KRED "\x1B[0;31m"
#define RESET "\x1B[0m"
#define KCYN_L "\x1B[1;36m"

struct msg_buffer{
    long msg_type;
    char msg_text[MAX_MSG_SIZE];
};

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr){
    
    if (mailbox_ptr->flag == MSG_PASSING){
        
        int msgid = mailbox_ptr->storage.msqid; // load mailbox ID
        struct msg_buffer msg;  // load the contents of message buffer in sender
        
        if (msgrcv(msgid, &msg, sizeof(msg.msg_text), 1, 0) == -1){ // link msg queue with msg buffer
            /*
                msgrcv(msg queue ID, msg buffer)
                copy the msg buffer in msg queue to the msg buffer in receiver.c 
            */
            perror("Failed to receive message via message queue");
            exit(1);
        }

        strcpy(message_ptr->data, msg.msg_text);
        if (strcmp(message_ptr->data, "exit") != 0){
            printf(KCYN_L "Receiving message: ");
            printf(RESET "%s\n", message_ptr->data);
        }
    
    }

    else if (mailbox_ptr->flag == SHARED_MEM){
        
        if (mailbox_ptr->storage.shm_addr == NULL){
            fprintf(stderr, "Shared Memory address is NULL\n");
            exit(1);
        }

        strcpy(message_ptr->data, mailbox_ptr->storage.shm_addr);   // Read msg from shared memory area
        if (strcmp(message_ptr->data, "exit") != 0){
            printf(KCYN_L "Receiving message: ");
            printf(RESET "%s\n", message_ptr->data);
        }

    }   

    else{
        fprintf(stderr, "Invalid communication method flag\n");
        exit(1);
    }

}

int main(int argc, char* argv[]){
    
    // Input error detection
    if (argc != 2){
        printf("Usage: %s <1 for message passing, 2 for shared memory>", argv[0]);
        return 1;
    }

    int method = atoi(argv[1]);
    int shmid = 0;

    message_t message;
    mailbox_t mailbox;

    sem_t* Sender_SEM;
    sem_t* Receiver_SEM;
    Sender_SEM = sem_open("/sender_sem", 0);
    Receiver_SEM = sem_open("/receiver_sem", 0);
    if (Sender_SEM == SEM_FAILED || Receiver_SEM == SEM_FAILED){
        perror("Failed to open semaphore");
        exit(1);
    }

    if (method == 1){   /*Message Passing*/
        mailbox.flag = MSG_PASSING;

        printf(KCYN_L "Message Passing\n");

        key_t key = ftok("sender.c", 65);   // create the same key as sender.c
        mailbox.storage.msqid = msgget(key, 0666);  // open message queue
        if (mailbox.storage.msqid == -1){
            perror("Failed to access message queue");
            exit(1);
        }
    
    }
    else if (method == 2){  /*Shared Memory*/
        mailbox.flag = SHARED_MEM;

        printf(KCYN_L "Shared Memory\n");

        key_t key = ftok("sender.c", 66);
        shmid = shmget(key, MAX_MSG_SIZE, 0666);    // open shm area
        if (shmid == -1){
            perror("Failed to access shared memory segment");
            exit(1);
        }
        mailbox.storage.shm_addr = (char*) shmat(shmid, NULL, 0);   // attach receiver to shm area 

    }
    else{
        printf("Invalid method\n");
        return 1;
    }

    struct timespec start, end;
    double time_spent = 0.0;

    /* Send */
    while (1){

        if (strcmp(message.data, "exit") == 0) break;
        
        if (method == 1){
            /* SEM & Critical region */
            sem_wait(Sender_SEM);

            clock_gettime(CLOCK_MONOTONIC, &start);
            receive(&message, &mailbox);
            clock_gettime(CLOCK_MONOTONIC, &end);
            time_spent += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

            sem_post(Receiver_SEM);
        }
        else if (method == 2){
            /* SEM & Critical region */
            sem_wait(Sender_SEM);

            clock_gettime(CLOCK_MONOTONIC, &start);
            receive(&message, &mailbox);
            clock_gettime(CLOCK_MONOTONIC, &end);
            time_spent += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

            sem_post(Receiver_SEM);   
        }

    }

    printf(KRED "Sender exit!\n");
    printf(RESET "Total time spent in sending msg: %f ms\n", time_spent);

    /* Post process */
    sem_close(Sender_SEM);
    sem_close(Receiver_SEM);
    sem_unlink("/sender_sem");
    sem_unlink("/receiver_sem");

    if (method == 1){
        msgctl(mailbox.storage.msqid, IPC_RMID, NULL);  // delete msg queue
    }
    else if (method == 2){
        shmdt(mailbox.storage.shm_addr);
    }

    return 0;

}