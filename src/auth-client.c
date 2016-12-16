/**
 * @file auth-client.c
 * @author Martin Weise <e1429167@student.tuwien.ac.at>
 * @date 28.11.2016
 *
 * @brief Main program module.
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
#include <stdbool.h>
#include "shared.h"


/* === Constants === */

/** The buffer size. */

/* === Macros === */

/**
 * @brief Provides a debugging function to output status messages
 * @details Activate/Deactivate by adding/removing -DENDEBUG to DEFS in Makefile.
 */
#if defined(ENDEBUG)
#define DEBUG(...) do { fprintf(stderr, __VA_ARGS__); } while(0)
#else
#define DEBUG(...)
#endif

/* === Global Variables === */

char *progname;
static int shmfd = -1;
static struct shared_command *shared;
static sem_t *sem;

/* === Prototypes === */

static void usage();
static void error_exit (const char *fmt, ...);
static void free_resources(void);
static void parse_args(int argc, char **argv);
static void command(bool printhelp);

/* === Implementations === */

static void usage() {
    fprintf(stderr, "USAGE: %s { -r | -l } username password\n", progname);
    exit(EXIT_FAILURE);
}

static void error_exit (const char *fmt, ...) {
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

static void free_resources(void) {
    if (shmfd != -1) {
        (void) close (shmfd);
    }
    /* Unmap the shared memory */
    if (munmap(shared, sizeof *shared) == -1) {
        error_exit("Couldn't unmap shared memory.");
    }
    /* Remove shared memory object */
    if (shm_unlink(SHM_NAME) == -1) {
        error_exit("Couldn't remove shared memory.");
    }
    /* Remove Semaphores */
    if (sem_close(sem) == -1) {
        error_exit("Couldn't remove semaphore.");
    }
}

static void parse_args(int argc, char **argv) {
    int flag_l = -1;
    int flag_r = -1;
    char opt;
    progname = argv[0];
    if (argc != 4 || optind != 1) {
        usage();
    }
    shared = malloc(sizeof(struct shared_command));
    while ((opt = getopt (argc, argv, "r:l:")) != -1) {
        switch (opt) {
            case 'l':
                if (flag_l != -1 || optarg == NULL) {
                    usage();
                }
                flag_l = 1;
                DEBUG("Login %d\n", LOGIN);
                shared->modus = LOGIN;
                break;
            case 'r':
                if (flag_r != -1 || optarg == NULL) {
                    usage();
                }
                flag_r = 1;
                DEBUG("Register %d\n", REGISTER);
                shared->modus = REGISTER;
                break;
            default: // ?
                break;
        }
    }
    if ((flag_l == 1) ^ (flag_r == 1)) {
        return;
    }
    usage();
}

static void command(bool printhelp) {
    int cmd = 1;
    if (printhelp) {
        printf("Commands:\n  1) write secret\n  2) read secret\n  3) logout\nPlease select a command (1-3):\n");
    }
    switch (cmd) {
        case 1:
            shared->command = WRITE;
            break;
        case 2:
            shared->command = READ;
            break;
        case 3:
            shared->command = LOGOUT;
            break;
        default:
            error_exit("Not a valid command.");
            break;
    }
    DEBUG("Modus is: %d, Command is: %d\n", shared->modus, shared->command);
}

int main(int argc, char **argv) {
    parse_args(argc, argv);

    /* Open shared memory object SHM_NAME in for reading and writing,
     * create it if it does not exist */
    if ((shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT, PERMISSION)) == -1) {
        error_exit("Couldn't init shared.");
    }

    /* Extend set size */
    if (ftruncate(shmfd, sizeof *shared) == -1) {
        error_exit("Couldn't extend shared size.");
    }

    /* Create a new mapping, let the kernel choose the address at which to create the memory  */
    if ((shared = mmap(NULL, sizeof *shared, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0)) == MAP_FAILED) {
        error_exit("Couldn't create mapping.");
    }

    /* Create Semaphores */
    if ((sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, PERMISSION, 1)) == SEM_FAILED) {
        error_exit("Couldn't create semaphore.");
    }

    command(true);
}
