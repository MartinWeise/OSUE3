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
extern sem_t *sem1;
extern sem_t *sem2;
extern sem_t *sem3;

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
                m = LOGIN;
                break;
            case 'r':
                if (flag_r != -1 || optarg == NULL) {
                    usage();
                }
                flag_r = 1;
                m = REGISTER;
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
    /* Close semaphor */
    if (sem_close(sem1) == -1) {
        error_exit("Couldn't remove semaphor 1.");
    }
    if (sem_close(sem2) == -1) {
        error_exit("Couldn't remove semaphor 2.");
    }
    if (sem_close(sem3) == -1) {
        error_exit("Couldn't remove semaphor 3.");
    }
}

static void signal_handler(int sig) {
    free_resources();
    exit (EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    bool logout = false;
    const int signals[] = {SIGINT, SIGTERM};
    struct sigaction s;
    sigset_t blocked_signals;
    status response;

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
    /* Create Semaphores */
    if ((sem1 = sem_open(SEM1_NAME, O_EXCL, PERMISSION, 1)) == SEM_FAILED) {
        error_exit("Couldn't create semaphore 1.");
    }
    if ((sem2 = sem_open(SEM2_NAME, O_EXCL, PERMISSION, 1)) == SEM_FAILED) {
        error_exit("Couldn't create semaphore 2.");
    }
    if ((sem3 = sem_open(SEM3_NAME, O_EXCL, PERMISSION, 1)) == SEM_FAILED) {
        error_exit("Couldn't create semaphore 3.");
    }

    DEBUG("Client running ...\n");
    /* Change mode */
    switch (m) {
        case REGISTER:
            /* reserve semaphor here */

            sem_wait(sem2);
            shared->modus = REGISTER;
            strncpy(shared->username, argv[2], MAX_DATA);
            strncpy(shared->password, argv[3], MAX_DATA);
            shared->status = STATUS_NONE;
            while (shared->status == STATUS_NONE) {
                // wait
            }
            switch (shared->status) {
                case REGISTER_SUCCESS:
                    printf("Successfully registered a new user.\n");
                    exit (EXIT_SUCCESS);
                    break;
                case REGISTER_FAILED:
                    error_exit("Failed to register a new user. User exists in database.");
                    break;
                default:
                    DEBUG("Unexpected status code:\n");
                    print_shared(shared);
                    assert(1==0);
            }
            /* release it here */
            sem_post(sem2);
            break;
        case LOGIN:
            while (!logout) {
                printf("Commands:\n  1) write secret\n  2) read secret\n  3) logout\nPlease select a command (1-3):\n");
                char buffer[MAX_DATA];
                if (fgets(buffer, sizeof buffer, stdin) == NULL) {
                    error_exit("fgets");
                }
                cmd command = (int) strtol(buffer, (char **)NULL, 10);
                /* now switch between the commands */
                switch (command) {
                    case WRITE:
                        DEBUG("Command is WRITE.\n");
                        char buf[MAX_DATA];
                        printf("Write secret here, commit with [RETURN]:\n");
                        if (fgets(buf, sizeof buf, stdin) == NULL) {
                            error_exit("fgets secret");
                        }
                        int len = strlen(buf);
                        if (buf[len - 1] == '\n') {
                            buf[len - 1] = '\0';
                        }
                        /* reserve semaphor here */
                        sem_wait(sem2);
                        sleep(10);
                        shared->modus = LOGIN;
                        shared->command = WRITE;
                        strncpy(shared->secret, buf, MAX_DATA);
                        strncpy(shared->username, argv[2], MAX_DATA);
                        strncpy(shared->password, argv[3], MAX_DATA);
                        shared->status = STATUS_NONE;
                        DEBUG("Waiting for server ...\n");
                        while (shared->status == STATUS_NONE) {
                            // wait
                        }
                        response = shared->status;
                        /* release semaphore here */
                        sem_post(sem2);
                        switch (response) {
                            case WRITE_SECRET_SUCCESS:
                                printf("Successfully wrote the secret.\n");
                                break;
                            case WRITE_SECRET_FAILED:
                                error_exit("Failed to write the secret. User does not exist in database.");
                                break;
                            default:
                                error_exit("Unknown response status code. %d", response);
                                break;
                        }
                        break;
                    case READ:
                        DEBUG("Command is READ.\n");
                        /* reserve semaphor here */
                        sem_wait(sem2);
                        shared->modus = LOGIN;
                        shared->command = READ;
                        strncpy(shared->username, argv[2], MAX_DATA);
                        strncpy(shared->password, argv[3], MAX_DATA);
                        shared->status = STATUS_NONE;
                        DEBUG("waiting for server to send secret ...\n");
                        while(shared->status == STATUS_NONE) {
                            // wait
                        }
                        response = shared->status;
                        char secret[MAX_DATA];
                        strncpy(secret, shared->secret, MAX_DATA);
                        /* release semaphore here */
                        sem_post(sem2);
                        switch (response) {
                            case LOGIN_SUCCESS:
                                printf("Success! Your secret is: %s\n", secret);
                                break;
                            case LOGIN_FAILED:
                                error_exit("Nope. Invalid credentials.");
                                break;
                            default:
                                assert(1==0);
                                break;
                        }
                        break;
                    case LOGOUT:
                        DEBUG("Command is LOGOUT.\n");
//                        shared->command = LOGOUT;
                        logout = true;
                        break;
                    default:
                        fprintf(stderr, "Invalid command. Please try again:\n");
                        break;
                }
            }
            break;
        default:
            usage();
            break;
    }
//    shared->status = STATUS_NONE;
}
