#include "sender.h"

#define MAX_MSG_SIZE 1024
#define MSG_PASSING 1
#define SHARED_MEM 2

// msg buffer for Message Passing
struct msg_buffer{
    long msg_type;
    char msg_text[MAX_MSG_SIZE];
};

void send(message_t message, mailbox_t* mailbox_ptr){
    
    // Use "Message_Passing" to send msg 
    if (mailbox_ptr -> flag == 1){
        struct msg_buffer msg;
        msg.msg_type = 1;   // set buffer state as "msg_passing"
        strcpy(msg.msg_text, message.data);    // copy message to msg_buffer
    
    if (msgsnd(mailbox_ptr -> storage.msqid, &msg, sizeof(msg.msg_text), 0) == -1){
        /* 
        msgsnd("a queue ID which receive msg", "get message type and content",
        "the size of the msg content", "0: if queue is full, wait")
        */
        // if return value is 0 => success, -1 => fail  
        perror("Failed to send message via message queue");
        exit(1);    
    }
    
    printf("Sent via Message Passing: %s\n", message.data);

    }
    // Use "Shared_Memory" to send msg
    else if (mailbox_ptr -> flag == SHARED_MEM){
        
        if (mailbox_ptr -> storage.shm_addr == NULL){
            fprintf(stderr, "Shared memory address is NULL\n");
            return;
        }

        strcpy(mailbox_ptr -> storage.shm_addr, message.data);  // msg write to shm_addr
        printf("Sent via Shared Memory: %s\n", message.data);

    }
    else{
        fprintf(stderr, "Invaild communication method flag\n");
    }
    
}

int main(int argc, char* argv[]){
    
    // Input error detection
    if (argc != 3){
        printf("Usage: %s <1 for Message Passing, 2 for Shared Memory> <input file>\n", argv[0]);
        return 1;
    }

    int method = atoi(argv[1]);
    const char* input_file = argv[2];

    // Read file
    FILE* file = fopen(input_file, "r");
    if (file == NULL){
        perror("Failed to open this file");
        return 1;
    }

    /*-----------------------------------------------------*/

    message_t message;
    mailbox_t mailbox;
    int shmid = 0;

    // Semaphore initialization
    sem_t* Sender_SEM;
    sem_t* Receiver_SEM;
    Sender_SEM = sem_open("/sender_sem", O_CREAT, 0664, 0);
    Receiver_SEM = sem_open("/receiver_sem", O_CREAT, 0664, 1);

    if (Sender_SEM == SEM_FAILED || Receiver_SEM == SEM_FAILED){
        perror("Failed to open semaphore");
        exit(1);
    }

    while (fgets(message.data, sizeof(message.data), file)){
        message.data[strcspn(message.data, "\n")] = 0;  // remove \n

        if (method == 1){   /* Message Passing */
            
            // Message Passing initialization
            mailbox.flag = MSG_PASSING;

            key_t key = ftok("msg1", 65);   // create a key
            mailbox.storage.msqid = msgget(key, 0666 | IPC_CREAT);  // create msg queue
            if (mailbox.storage.msqid == -1){
                perror("Failed to create or access message queue");
                exit(1);
            }

            /* SEM & Critical region */ 
    
            sem_wait(Receiver_SEM);   // wait B to signal

            send(message, &mailbox);    // send msg to mailbox.msq

            sem_post(Sender_SEM); // signal B

        }
        else if (method == 2){  /* Shared Memory */
            
            // Shared Memory initialization
            mailbox.flag = SHARED_MEM;

            key_t key = ftok("msg2", 66);
            shmid = shmget(key, MAX_MSG_SIZE, 0666 | IPC_CREAT);    // create shm segment
            if (shmid == -1){
                perror("Failed to create shared memory segment");
                exit(1);
            }
            mailbox.storage.shm_addr = (char*)shmat(shmid, NULL, 0);    // shm attatch to created shm segment 

            /* SEM & Critical region */ 

            sem_wait(Receiver_SEM);   // wait B to signal

            send(message, &mailbox);

            sem_post(Sender_SEM); // signal B

        }
        else{
            printf("Invalid method\n");
            return 1;
        }
    
    }
    
    fclose(file);

    // Semaphore
    sem_close(Sender_SEM);
    sem_close(Receiver_SEM);
    sem_unlink("/sender_sem");
    sem_unlink("/receiver_sem");    

    if (method == 1){
        // Message Passing
        msgctl(mailbox.storage.msqid, IPC_RMID, NULL);  // delete msg queue
    }
    else if (method == 2){
        // Shared Memory
        shmdt(mailbox.storage.shm_addr);
        shmctl(shmid, IPC_RMID, NULL);
    }

    return 0;

    /*  TODO: 
        1) Call send(message, &mailbox) according to the flow in slide 4
        2) Measure the total sending time
        3) Get the mechanism and the input file from command line arguments
            • e.g. ./sender 1 input.txt
                    (1 for Message Passing, 2 for Shared Memory)
        4) Get the messages to be sent from the input file
        5) Print information on the console according to the output format
        6) If the message form the input file is EOF, send an exit message to the receiver.c
        7) Print the total sending time and terminate the sender.c
    */
}