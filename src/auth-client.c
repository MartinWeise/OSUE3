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
#include <limits.h>
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
#include <sys/time.h>
#include "shared.h"


/* === Constants === */


/* === Global Variables === */

extern sig_atomic_t terminating;
extern sem_t *sem_server;

char *progname;
static int shmfd;
static struct shared_command *shared;

static int m = -1;

/* === Prototypes === */

static void free_resources(void);
static void error_exit (const char *fmt, ...);
static void usage(void);
static void parse_args(int argc, char **argv);

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
                m = 1;
                break;
            case 'r':
                if (flag_r != -1 || optarg == NULL) {
                    usage();
                }
                flag_r = 1;
                m = 0;
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
    if (terminating == 1) {
        return;
    }
    terminating = 1;
    /* Close shared memory */
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
    /* Close semaphor */
    if (sem_close(sem_server) == -1) {
        error_exit("Couldn't remove semaphor 1.");
    }
    /* Unlink semaphor */
    if (sem_unlink(SEM1_NAME)) {
        error_exit("Couldn't unlink sempaphor 1.");
    }
}

static void signal_handler(int sig) {
    free_resources();
    exit (EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    bool cmd_set = false;
    const int signals[] = {SIGINT, SIGTERM};
    struct sigaction s;
    sigset_t blocked_signals;

    progname = argv[0];
    /* Fill set with all signals */
    if(sigfillset(&blocked_signals) < 0) {
        error_exit("Failed to fill signal set.");
    }
    s.sa_handler = signal_handler;
    (void) memcpy(&s.sa_mask, &blocked_signals, sizeof(s.sa_mask));
    s.sa_flags = SA_RESTART;
    for(int i = 0; i < 2; i++) {
        if (sigaction(signals[i], &s, NULL) < 0) {
            error_exit("Changing of signal failed.");
        }
    }

    parse_args(argc, argv);

    /* Open shared memory object SHM_NAME in for reading and writing,
     * create it if it does not exist */
    if ((shmfd = shm_open(SHM_NAME, O_RDWR, PERMISSION)) == -1) {
        error_exit("Couldn't access shared fragement. Is the server running?");
    }
    /* Create a new mapping, let the kernel choose the address at which to create the memory  */
    if ((shared = mmap(NULL, sizeof *shared, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0)) == MAP_FAILED) {
        error_exit("Couldn't create mapping.");
    }
    /* Open Semaphores */
    if ((sem_server = sem_open(SEM1_NAME, O_EXCL | O_TRUNC, PERMISSION, 1)) == SEM_FAILED) {
        error_exit("Couldn't open semaphore. Is the server running?");
    }

    /* Change mode */
    switch (m) {
        case 0: // r
//            shared->modus = REGISTER;
            break;
        case 1: // l
//            shared->modus = LOGIN;
            break;
        default:
            usage();
            break;
    }

    DEBUG("Client running ...\n");

    printf("Commands:\n  1) write secret\n  2) read secret\n  3) logout\nPlease select a command (1-3):\n");

    while (!cmd_set) {
        char buffer[MAX_DATA];
        if (fgets(buffer, sizeof buffer, stdin) == NULL) {
            error_exit("fgets");
        }
        cmd command = (int) strtol(buffer, (char **)NULL, 10);
        /* now switch between the commands */
        switch (command) {
            case WRITE:
                DEBUG("Command is WRITE.\n");
//                shared->command = WRITE;
                cmd_set = true;
                break;
            case READ:
                DEBUG("Command is READ.\n");
//                shared->command = READ;
//                shared->status = STATUS_NONE;
//                status response;
//                while (shared->status == STATUS_NONE) {
//                    response = shared->status;
//                }
//                switch (response) {
//                    case LOGIN_SUCCESS:
//                        printf("Success! Your secret is: %s\n", shared->secret);
//                        break;
//                    case LOGIN_FAILED:
//                        error_exit("Nope. Invalid credentials.");
//                        break;
//                    default:
//                        assert(1==0);
//                        break;
//                }
                cmd_set = true;
                break;
            case LOGOUT:
                DEBUG("Command is LOGOUT.\n");
//                shared->command = LOGOUT;
                cmd_set = true;
                break;
            default:
                fprintf(stderr, "Invalid command. Please try again:\n");
                break;
        }
    }
//    shared->status = STATUS_NONE;
}
