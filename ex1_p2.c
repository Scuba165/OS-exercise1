/* Proccess 2 of Exercise 1. This process joins and uses the shared memory segment. It is not responsible for initializing nor
unlinking the memory.*/

#include <ctype.h>
#include <string.h>
#include <pthread.h>

#include "ex1.h"


// Global variables for stats. Both threads have access and modify these accordingly.
int messagesSent = 0;
int messagesReceived = 0;
int totalPacksReceived = 0;
long responseTime = 0;


// Prints the stats at the end of the execution.
void printStats(FILE* outputStream) {
    fprintf(outputStream, "\n\tProcess 2 summary:Messages Sent: %d\n\t\tMessages Received(Packages): %d(%d)\n\t\tAverage Packages per Message: %d\n",
            messagesSent, messagesReceived, totalPacksReceived, messagesReceived ? totalPacksReceived/messagesReceived : 0);
    fprintf(outputStream, "\t\tAverage Response Time: %ld microseconds.\n", messagesReceived ? responseTime/messagesReceived : 0);
    fflush(outputStream);
}

void* sendMessageThread(void* arg) {
    struct shmbuf* memStruct = (struct shmbuf*)arg;
    char str[BUF_SIZE];
    int newMessage = 1;
    FILE* outputStream = memStruct->outputStream;
    printf("Type a message: ");
    while(fgets(str, BUF_SIZE, stdin) != NULL) {
        if(newMessage) {
            newMessage = 0;
            gettimeofday(&memStruct->lastMessage2, NULL); // Set the time that message was sent for p1.
        }
        memcpy(memStruct->buf, str, (size_t)BUF_SIZE);
        sem_post(&memStruct->sem1);
        usleep(1); // Sleep for 1 ms to give time to receiver of the other proccess before copying the next 15 characters.
        if (strcmp(str, TERM_STRING) == 0) { 
            messagesSent++;
            printStats(outputStream);
            exit(0); // We can safely exit the entire program. Memory has been unlinked.
        } 
        if(strchr(str, '\n') != NULL) {
            messagesSent++;
            printf("Type a message: "); // Prepare for next input.
            newMessage = 1; // Reset flag.
        }
    }
    return NULL;
}

void* receiveMessageThread(void* arg) {
    struct shmbuf* memStruct = (struct shmbuf*)arg;
    int firstPack = 1;
    struct timeval received;
    FILE* outputStream = memStruct->outputStream;
    while(1) {  
        if (sem_wait(&memStruct->sem2) == -1)
            errExit("sem_post");
        
        /* Get and print the string from shared memory. */
        if(firstPack == 1) {
            if(outputStream == stdout)
                printf("\33[2K\r"); // Erases current line ("Type a message: ").
            fprintf(outputStream, "Message from process 1 received: ");
            firstPack = 0;
            struct timeval lastFrom1 = memStruct->lastMessage1;
            gettimeofday(&received, NULL);
            responseTime += (received.tv_sec - lastFrom1.tv_sec) * 1000000 + (received.tv_usec - lastFrom1.tv_usec); // Add response time to total
        }
        char* str = memStruct->buf;
        totalPacksReceived++;
        fputs(str, outputStream);
        fflush(outputStream);
        if(strcmp(str, TERM_STRING) == 0) {
            messagesReceived++;
            printStats(outputStream);
            exit(0);
        }     
        if(strchr(str, '\n') != NULL) {
            messagesReceived++;
            firstPack = 1; 
            if(memStruct->outputStream == stdout)
                printf("Type a message: "); // Prepare for possible input.
            fflush(outputStream); // Instantly print above message.  
        }
    }
    return NULL;
}



int main(int argc, char *argv[]) {
    int fileDesc;
    char *shmpath;
    struct shmbuf *memStruct;

    // Instructions for correct usage.
    if ((argc < 2) || (argc > 3)) {
        fprintf(stderr, "Usage: %s /shm-path for writing in terminal, %s /shm-path fileName for writing in file.\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    shmpath = argv[1];

    /* Open the existing shared memory object and map it
        into the caller's address space. */

    fileDesc = shm_open(shmpath, O_RDWR, 0);
    if (fileDesc == -1) {
        errExit("shm_open");
    }

    // Map the object into the caller's address space.
    memStruct = mmap(NULL, sizeof(*memStruct), PROT_READ | PROT_WRITE,
                MAP_SHARED, fileDesc, 0);
    if (memStruct == MAP_FAILED)
        errExit("mmap");


    close(fileDesc);          // 'fd' is no longer needed

    // If we want to write in file, update outputStream.
    FILE* outputStream;
    if(argv[2]) {
        strcat(argv[2], ".txt");
        if((outputStream = fopen(argv[2], "a")) == NULL)
            errExit("fopen");
    } else 
        outputStream = stdout;
    memStruct->outputStream = outputStream;
    
    // Initialize and call threads
    pthread_t thread_id2;
    pthread_create(&thread_id2, NULL, &receiveMessageThread, (void*)memStruct);
    fprintf(memStruct->outputStream, "Created receiver thread in process 2 with id %u \n", (unsigned int)thread_id2);
    pthread_t thread_id1;
    pthread_create(&thread_id1, NULL, &sendMessageThread, (void*)memStruct);
    fprintf(memStruct->outputStream, "Created sender thread in process 2 with id %u \n", (unsigned int)thread_id1);
    fflush(memStruct->outputStream);

    // Join threads so proccess waits for termination.
    if(pthread_join(thread_id2, NULL) == -1)
        errExit("pthread_join");
    if(pthread_join(thread_id1, NULL) == -1)
        errExit("pthread_join");


    exit(EXIT_SUCCESS);
}