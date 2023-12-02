#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>

// Macro to print possible error.
#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

#define BUF_SIZE 15   // Maximum size for exchanged string 
#define TERM_STRING "#BYE#\n" // String that terminates execution of program.

// Define a structure that will be imposed on the shared memory object

struct shmbuf {
    sem_t  sem1;                 /* POSIX unnamed semaphore */
    sem_t  sem2;                 /* POSIX unnamed semaphore */
    char   buf[BUF_SIZE];        /* Data being transferred */
    struct timeval lastMessage1; /* Last Message from p1 */
    struct timeval lastMessage2; /* Last Message from p2 */
    FILE* outputStream;          /* Output stream. Default will be stdout, will change if argument is given */
};
