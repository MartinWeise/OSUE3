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


/* === Global Variables === */

extern sig_atomic_t terminating;
extern sem_t *sem_client;

char *progname;
static int shmfd;
static struct shared_command *shared;

/* === Prototypes === */

extern void free_resources(void);
extern void error_exit (const char *fmt, ...);

static void usage(void);
static void parse_args(int argc, char **argv);
static void command(bool printhelp);

/* === Implementations === */

static void usage() {
    fprintf (stderr, "USAGE: %s { -r | -l } username password\n", progname);
    exit(EXIT_FAILURE);
}

static void parse_args(int argc, char **argv) {
    int flag_l = -1;
    int flag_r = -1;
    char opt;
    progname = argv[0];
    if (argc != 4 || optind != 1) {
        usage();
    }
    while ((opt = getopt (argc, argv, "r:l:")) != -1) {
        switch (opt) {
            case 'l':
                if (flag_l != -1 || optarg == NULL) {
                    usage();
                }
                flag_l = 1;
                break;
            case 'r':
                if (flag_r != -1 || optarg == NULL) {
                    usage();
                }
                flag_r = 1;
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
        error_exit("Couldn't access shared fragement. Is the server running?");
    }

    /* Create a new mapping, let the kernel choose the address at which to create the memory  */
    if ((shared = mmap(NULL, sizeof *shared, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0)) == MAP_FAILED) {
        error_exit("Couldn't create mapping.");
    }

    /* Open Semaphores */
//    if ((sem = sem_open(SEM_NAME, O_EXCL, PERMISSION, 1)) == SEM_FAILED) {
//        error_exit("Couldn't open semaphore.");
//    }

    DEBUG("Client running ...\n");

//    for(int i = 0; i < 3; ++i) {
//        sem_wait(sem);
//        DEBUG("critical: %s: i = %d\n", argv[0], i);
//        sleep(1);
//        sem_post(sem);
//    }

    command(true);
}
