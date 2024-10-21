#include "receiver.h"

#define MAX_MSG_SIZE 1024
#define MSG_PASSING 1
#define SHARED_MEM 2

struct msg_buffer{
    long msg_type;
    char msg_text[MAX_MSG_SIZE];
};

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr){
    
    if (mailbox_ptr->flag == MSG_PASSING){
        int msgid = mailbox_ptr->storage.msqid;
        struct msg_buffer msg;
        
        if (msgrcv(msgid, &msg, sizeof(msg.msg_text), 1, 0) == -1){
            /*
                msgrcv(msg queue ID, msg buffer)
                copy the msg buffer in msg queue to the msg buffer in receiver.c 
            */
            perror("Failed to receive message via message queue");
            exit(1);
        }

        strcpy(message_ptr->data, msg.msg_text);
        printf("Received via Message Passing: %s\n", message_ptr->data);

    }
    else if (mailbox_ptr->flag == SHARED_MEM){
        if (mailbox_ptr->storage.shm_addr == NULL){
            fprintf(stderr, "Shared Memory address is NULL\n");
            exit(1);
        }

        // Read msg from shared memory area
        strcpy(message_ptr->data, mailbox_ptr->storage.shm_addr);
        printf("Received via Shared Memory: %s\n", message_ptr->data);

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

        key_t key = ftok("msg1", 65);   // create the same key as sender.c
        mailbox.storage.msqid = msgget(key, 0666 | IPC_CREAT);  // open message queue
        if (mailbox.storage.msqid == -1){
            perror("Failed to access message queue");
            exit(1);
        }

        /* SEM & Critical region */
        sem_wait(Sender_SEM);

        receive(&message, &mailbox);

        sem_post(Receiver_SEM);

    }
    else if (method == 2){  /*Shared Memory*/
        mailbox.flag = SHARED_MEM;

        key_t key = ftok("msg2", 66);
        shmid = shmget(key, MAX_MSG_SIZE, 0666 | IPC_CREAT);    // open shm area
        if (shmid == -1){
            perror("Failed to access shared memory segment");
            exit(1);
        }

        mailbox.storage.shm_addr = (char*) shmat(shmid, NULL, 0);   // attach to shm area 

        /* SEM & Critical region */
        sem_wait(Sender_SEM);

        receive(&message, &mailbox);

        sem_post(Sender_SEM);
    
    }
    else{
        printf("Invalid method\n");
        return 1;
    }

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
        shmctl(shmid, IPC_RMID, NULL);
    }

    return 0;


    /*  TODO: 
        1) Call receive(&message, &mailbox) according to the flow in slide 4
        2) Measure the total receiving time
        3) Get the mechanism from command line arguments
            • e.g. ./receiver 1
        4) Print information on the console according to the output format
        5) If the exit message is received, print the total receiving time and terminate the receiver.c
    */
}