/* pshm_ucase_bounce.c

    Licensed under GNU General Public License v2 or later.
*/
#include <ctype.h>
#include <string.h>
#include <pthread.h>

#include "ex1.h"

// Global variables for stats. Both threads have access and modify these accordingly.
int messagesSent = 0;
int messagesReceived = 0;
int totalPacksReceived = 0;
long responseTime = 0;


// Global shmpath to unlink in the end.
char *shmpath;

// Prints the stats at the end of the execution.
void printStats(FILE* outputStream) {
    fprintf(outputStream, "\n\tProcess 1 summary:Messages Sent: %d\n\t\tMessages Received(Packages): %d(%d)\n\t\tAverage Packages per Message: %d\n",
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
            gettimeofday(&memStruct->lastMessage1, NULL); // Set the time that message was sent for p2.
        }
        memcpy(memStruct->buf, str, (size_t)BUF_SIZE);
        sem_post(&memStruct->sem2);
        usleep(1); // Sleep for 1 ms to give time to receiver of the other proccess before copying the next 15 characters.
        if (strcmp(str, TERM_STRING) == 0) { 
            messagesSent++;
            printStats(outputStream);

            /* Unlink the shared memory object. Even if the peer process
            is still using the object, this is okay. The object will
            be removed only after all open references are closed. */
            if(shm_unlink(shmpath) == -1)
                errExit("shm_unlink");
            if(outputStream != stdout)
                fclose(outputStream);
            printf("\nSuccessfully unlinked from memory.\n");

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
        if (sem_wait(&memStruct->sem1) == -1)
            errExit("sem_post");
        
        /* Get and print the string from shared memory. */
        if(firstPack == 1) {
            if(outputStream == stdout)
                printf("\33[2K\r"); // Erases current line ("Type a message: ").
            fprintf(outputStream, "Message from process 2 received: ");
            fflush(outputStream);
            firstPack = 0;
            struct timeval lastFrom2 = memStruct->lastMessage2;
            gettimeofday(&received, NULL);
            responseTime += (received.tv_sec - lastFrom2.tv_sec) * 1000000 + (received.tv_usec - lastFrom2.tv_usec); // Add response time to total
        }
        char* str = memStruct->buf;
        totalPacksReceived++;
        fputs(str, outputStream);
        fflush(outputStream);
        if(strcmp(str, TERM_STRING) == 0) {
            messagesReceived++;
            printStats(outputStream);

            /* Unlink the shared memory object. Even if the peer process
            is still using the object, this is okay. The object will
            be removed only after all open references are closed. */
            if(outputStream != stdout)
                fclose(outputStream);
            if(shm_unlink(shmpath) == -1)
                errExit("shm_unlink");
            fprintf(outputStream, "\nSuccessfully unlinked from memory.\n");

            exit(0);
        }     
        if(strchr(str, '\n') != NULL) {
            messagesReceived++;
            firstPack = 1; 
            if(outputStream == stdout)
                printf("Type a message: "); // Prepare for possible input.
            fflush(outputStream); // Instantly print above message.  
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int fileDesc;
    struct shmbuf *memStruct;

    // Instructions for correct usage.
    if ((argc < 2) || (argc > 3)) {
        fprintf(stderr, "Usage: %s /shm-path for writing in terminal, %s /shm-path fileName for writing in file.\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    shmpath = argv[1];


    // Create shared memory object and set its size to the size of our structure.
    fileDesc = shm_open(shmpath, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fileDesc == -1)
        errExit("shm_open");

    if (ftruncate(fileDesc, sizeof(struct shmbuf)) == -1)
        errExit("ftruncate");

    // Map the object into the caller's address space.
    memStruct = mmap(NULL, sizeof(*memStruct), PROT_READ | PROT_WRITE,
                MAP_SHARED, fileDesc, 0);
    if (memStruct == MAP_FAILED)
        errExit("mmap");

    // If we want to write in a file, update outputStream.
    FILE* outputStream;
    if(argv[2]) {
        strcat(argv[2], ".txt");
        if((outputStream = fopen(argv[2], "a")) == NULL)
            errExit("fopen");
    } else 
        outputStream = stdout;
    memStruct->outputStream = outputStream;
    
    // Print address.
    fprintf(memStruct->outputStream, "Shared memory object \"%s\" has been created from process 1 at address\"%p\"\n", shmpath, memStruct);
    close(fileDesc);          // 'fd' is no longer needed


    // Initialize semaphores as process-shared, with value 0 and set flag to 0.
    if (sem_init(&memStruct->sem1, 1, 0) == -1)
        errExit("sem_init-sem1");
    if (sem_init(&memStruct->sem2, 1, 0) == -1)
        errExit("sem_init-sem2");
    
    // Initialize and call threads
    pthread_t thread_id2;
    pthread_create(&thread_id2, NULL, &receiveMessageThread, (void*)memStruct);
    fprintf(memStruct->outputStream, "Created receiver thread in process 1 with id %u \n", (unsigned int)thread_id2);
    pthread_t thread_id1;
    pthread_create(&thread_id1, NULL, &sendMessageThread, (void*)memStruct);
    fprintf(memStruct->outputStream, "Created sender thread in process 1 with id %u \n", (unsigned int)thread_id1);
    fflush(memStruct->outputStream);

    // Join threads so proccess waits for termination.
    if(pthread_join(thread_id2, NULL) == -1)
        errExit("pthread_join");
    if(pthread_join(thread_id1, NULL) == -1)
        errExit("pthread_join");

    exit(EXIT_SUCCESS);
}