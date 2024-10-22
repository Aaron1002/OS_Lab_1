#include "sender.h"

#define MAX_MSG_SIZE 1024
#define MSG_PASSING 1
#define SHARED_MEM 2

#define KRED "\x1B[0;31m"
#define RESET "\x1B[0m"
#define KCYN_L "\x1B[1;36m"

// msg buffer for Message Passing (independent in each of the process)
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
    
    if (msgsnd(mailbox_ptr -> storage.msqid, &msg, sizeof(msg.msg_text), 0) == -1){ // link msg buffer with msg queue
        /* 
        msgsnd("a queue ID which receive msg", "get message type and content",
        "the size of the msg content", "0: if queue is full, wait")
        */
        // if return value is 0 => success, -1 => fail  
        perror("Failed to send message via message queue");
        exit(1);    
    } 
    if (strcmp(message.data, "exit") != 0){
        printf(KCYN_L "Sending Message: ");
        printf(RESET "%s\n",message.data);
    }

    }

    // Use "Shared_Memory" to send msg
    else if (mailbox_ptr -> flag == SHARED_MEM){

        if (mailbox_ptr -> storage.shm_addr == NULL){
            fprintf(stderr, "Shared memory address is NULL\n");
            return;
        }

        strcpy(mailbox_ptr -> storage.shm_addr, message.data);  // msg write to shm segment
        if (strcmp(message.data, "exit") != 0){
            printf(KCYN_L "Sending message: ");
            printf(RESET "%s\n", message.data);
        }
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

    int method = atoi(argv[1]); // string to int
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
    Sender_SEM = sem_open("/sender_sem", O_CREAT, 0664, 0); // create sem and set initial value
    Receiver_SEM = sem_open("/receiver_sem", O_CREAT, 0664, 1);

    if (Sender_SEM == SEM_FAILED || Receiver_SEM == SEM_FAILED){
        perror("Failed to open semaphore");
        exit(1);
    }

/*-----------------------------------------------------*/

    // Sending preprocess
    if (method == 1){
        
        printf(KCYN_L "Message Passing\n");

        // Message Passing initialization
        mailbox.flag = MSG_PASSING;
        
        key_t key = ftok("sender.c", 65);   // create a key(unique for msg queue)
        mailbox.storage.msqid = msgget(key, 0666 | IPC_CREAT);  // create msg queue
        if (mailbox.storage.msqid == -1){
            perror("Failed to create or access message queue");
            exit(1);
        }    
    
    }
    else if (method == 2){
        
        printf(KCYN_L "Shared Memory\n");

        // Shared Memory initialization
        mailbox.flag = SHARED_MEM;
        
        key_t key = ftok("sender.c", 66);
        shmid = shmget(key, MAX_MSG_SIZE, 0666 | IPC_CREAT);    // create shm segment
        if (shmid == -1){
            perror("Failed to create shared memory segment");
            exit(1);
        }
        mailbox.storage.shm_addr = (char*)shmat(shmid, NULL, 0);    // sender attatch to created shm segment 

    }

    struct timespec start, end;
    double time_spent = 0.0;

    /* Send */
    while (fgets(message.data, sizeof(message.data), file)){
        message.data[strcspn(message.data, "\n")] = 0;  // remove \n

        if (method == 1){   /* Message Passing */

            /* SEM & Critical region */ 
    
            sem_wait(Receiver_SEM);   // wait B to signal (receiver_sem - 1)

            clock_gettime(CLOCK_MONOTONIC, &start);
            send(message, &mailbox);    // send msg to mailbox.msq
            clock_gettime(CLOCK_MONOTONIC, &end);
            time_spent += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9; 

            sem_post(Sender_SEM); // signal B (sender_sem + 1)

        }
        else if (method == 2){  /* Shared Memory */

            /* SEM & Critical region */ 

            sem_wait(Receiver_SEM);   // wait B to signal

            clock_gettime(CLOCK_MONOTONIC, &start);
            send(message, &mailbox);
            clock_gettime(CLOCK_MONOTONIC, &end);
            time_spent += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9; 

            sem_post(Sender_SEM); // signal B

        }
        else{
            printf("Invalid method\n");
            return 1;
        }
    
    }
    
    strcpy(message.data, "exit");
    if (method == 1){
        printf(KRED "End of input file! exit!\n");
        sem_wait(Receiver_SEM);
        send(message, &mailbox);    // send "exit" to receiver
        sem_post(Sender_SEM);
    }
    else if (method == 2){
        printf(KRED "End of input file! exit!\n");
        sem_wait(Receiver_SEM);
        send(message, &mailbox);
        sem_post(Sender_SEM);
    }

    printf(RESET "Total time spent in sending msg: %f ms\n", time_spent);

    fclose(file);

    // Semaphore
    sem_close(Sender_SEM);  // close semaphore
    sem_close(Receiver_SEM);
    sem_unlink("/sender_sem");  // delete semaphore
    sem_unlink("/receiver_sem");    

    if (method == 2){
        // Shared Memory
        shmdt(mailbox.storage.shm_addr);    // detach sender with shm segment
        shmctl(shmid, IPC_RMID, NULL);  // delete shm segment
    }

    return 0;

}