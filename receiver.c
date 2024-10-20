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

    }
    
    /*  TODO: 
        1. Use flag to determine the communication method
        2. According to the communication method, receive the message
    */
}

int main(){
    
    message_t message;
    mailbox_t mailbox;

    /*-----------------------------------------------*/

    /*Message Passing*/

    mailbox.flag = MSG_PASSING;

    key_t key = ftok("msg1", 65);   // create the same key as sender.c
    mailbox.storage.msqid = msgget(key, 0666 | IPC_CREAT);  // open message queue

    receive(&message, &mailbox);

    msgctl(mailbox.storage.msqid, IPC_RMID, NULL);  // delete msg queue

    /*-----------------------------------------------*/

    /*Shared Memory*/

    mailbox.flag = SHARED_MEM;

    key = ftok("msg2", 66);
    int shmid = shmget(key, MAX_MSG_SIZE, 0666 | IPC_CREAT);    // open shm area
    mailbox.storage.shm_addr = (char*) shmat(shmid, NULL, 0);   // attach to shm area 

    receive(&message, &mailbox);

    shmdt(mailbox.storage.shm_addr);
    shmctl(shmid, IPC_RMID, NULL);

    /*  TODO: 
        1) Call receive(&message, &mailbox) according to the flow in slide 4
        2) Measure the total receiving time
        3) Get the mechanism from command line arguments
            • e.g. ./receiver 1
        4) Print information on the console according to the output format
        5) If the exit message is received, print the total receiving time and terminate the receiver.c
    */
}