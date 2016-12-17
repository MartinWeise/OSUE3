/**
 * @file shared.c
 * @author Martin Weise <e1429167@student.tuwien.ac.at>
 * @date 17.12.2016
 *
 * @brief Shared memory file.
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <memory.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <assert.h>
#include <fcntl.h>
#include <semaphore.h>
#include "shared.h"

/* === Global Variables === */

volatile sig_atomic_t terminating = 0;

struct entry *first = NULL;

int shmfd = -1;

int sem_client = -1;

int sem_server = -1;

struct shared_command *shared = NULL;

char *progname;

/* === Implementations === */

void error_exit (const char *fmt, ...) {
    va_list ap;

    (void) fprintf(stderr, "%s: ", progname);
    if (fmt != NULL) {
        va_start(ap, fmt);
        (void) vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    if (errno != 0) {
        (void) fprintf(stderr, ": %s", strerror(errno));
    }
    (void) fprintf(stderr, "\n");
    free_resources();

    DEBUG("Shutting down now.\n");
    exit (EXIT_FAILURE);
}

void free_resources(void) {
    struct entry *temp;
    if (terminating == 1) {
        return;
    }
    terminating = 1;
    if (shmfd != -1) {
        (void) close (shmfd);
    }
    while (first != NULL) {
        temp = first;
        first = first->next;
        free(temp);
    }
    /* Unmap the shared memory */
    if (munmap(shared, sizeof *shared) == -1) {
        error_exit("Couldn't unmap shared memory.");
    }
    /* Remove shared memory object */
    if (shm_unlink(SHM_NAME) == -1) {
        error_exit("Couldn't remove shared memory.");
    }
    /* Unlink Semaphore */
    if (sem_unlink(SEM_NAME) == -1) {
        error_exit("Couldn't remove semaphore.");
    }
    /* Close Semaphores */
    if (sem_close(sem_server) == -1) {
        error_exit("Couldn't close semaphore.");
    }
}

void signal_handler(int sig) {
    free_resources();
    exit (EXIT_SUCCESS);
}
