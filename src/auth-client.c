/**
 * @file auth-client.c
 * @author Martin Weise <e1429167@student.tuwien.ac.at>
 * @date 03.01.2017
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

/* === Global Variables === */

/** @brief Used to terminate the client only once. */
volatile sig_atomic_t terminating = -1;
/** @brief Semaphor to allow client the initial request. @details Is a binary semaphor. */
extern sem_t *sem1;
/** @brief Semaphor to allow client further commands when logged in. @details Is a binary semaphor. */
extern sem_t *sem2;
/** @brief Semaphor to sync waiting for response from server. @details Is a binary semaphor. */
extern sem_t *sem3;
/** @brief Holds the program name. */
static char *progname;
/** @brief Holds the username for logged-in requests. */
static char *username;
/** @brief Holds the password for logged-in requests. */
static char *password;
/** @brief Holds the session id for logged-in requests. */
static char session_id[MAX_DATA];
/** @brief Holds the secret, once set */
static char secret[MAX_DATA];
/** @brief The shared memory file descriptor */
static int shmfd;
/** @brief Flag for releasing the used semaphores in case of a client crash
 * @details -1 = no semaphor used, 1 = semaphor 2 used. */
static int server_waits = -1;
/** @brief The shared command that is sent between server and client */
static struct shared_command *shared;
/** @brief The mode in which the client operates in. @details Is determined by the argument vector. */
static int m = -1;

/* === Prototypes === */

/**
 * @brief Frees the used resources.
 * @details This method is also invoked when the signals SIGINT and SIGTERM occur.
 */
static void free_resources(void);
/**
 * @brief Exists the program with a given message.
 * @param fmt Formatted string for parsing the latter arguments to.
 */
static void error_exit (const char *fmt, ...);
/**
 * @brief Prints a nice usage message.
 */
static void usage(void);
/**
 * @brief Set the operation mode (register | login) and verify only one mode is given.
 * @param argc The argument counter.
 * @param argv The argument vector.
 */
static void parse_args(int argc, char **argv);
/**
 * @brief Method to handle certain signals.
 * @details Is invoked on SIGINT and SIGTERM.
 * @param sig Signal code.
 */
static void signal_handler(int sig);
/**
 * @brief The program entry point.
 * @param argc The argument vector.
 * @param argv The argument counter.
 * @return EXIT_SUCCESS on succesful program execution, EXIT_FAILURE otherwise.
 */
int main(int argc, char **argv);

/* === Implementations === */

static void usage() {
    (void) fprintf (stderr, "USAGE: %s { -r | -l } username password\n", progname);
    exit(EXIT_FAILURE);
}

static void parse_args(int argc, char **argv) {
    int flag_l = -1;
    int flag_r = -1;
    int flag_d = -1;
    char opt;
    progname = argv[0];
    if (argc != 4 || optind != 1) {
        usage();
    }
    while ((opt = getopt (argc, argv, "r:l:")) != -1 && flag_d == -1) {
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
                flag_d = 1;
                break;
        }
    }
    if ((flag_l == 1) ^ (flag_r == 1)) {
        username = argv[2];
        password = argv[3];
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
    /* Tell server the client is done */
    if (server_waits != -1) {
        shared->command = COMMAND_NONE;
        shared->modus = MODE_UNSET;
        if (sem_post(sem2) == -1) {
            error_exit("Server quit.");
        }
    }
    /* Close shared memory */
    if (shmfd != -1) {
        if (close (shmfd) == -1) {
            error_exit("Couldn't close shared memory.");
        }
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
    terminating = 1;
}

int main(int argc, char **argv) {
    const int signals[] = {SIGINT, SIGTERM};
    struct sigaction s;
    sigset_t blocked_signals;
    status response;

    session_id[0] = 0;
    secret[0] = 0;
    progname = argv[0];
    /* Fill set with all signals */
    if(sigfillset(&blocked_signals) < 0) {
        error_exit("Failed to fill signal set.");
    }
    s.sa_handler = signal_handler;
    (void) memcpy(&s.sa_mask, &blocked_signals, sizeof(s.sa_mask));
    s.sa_flags = SIGQUIT;
    s.sa_flags = SIGINT;
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
    if ((sem2 = sem_open(SEM2_NAME, O_EXCL, PERMISSION, 0)) == SEM_FAILED) {
        error_exit("Couldn't create semaphore 2.");
    }
    if ((sem3 = sem_open(SEM3_NAME, O_EXCL, PERMISSION, 0)) == SEM_FAILED) {
        error_exit("Couldn't create semaphore 3.");
    }

    DEBUG("Client running ...\n");

    /* wait for server to allow client to send request */
    while (shared->server_down != -1 || sem_wait(sem1) == -1) {
        error_exit("Server quit.");
    }
    server_waits = 1;
    (void) strncpy(shared->username, username, MAX_DATA);
    (void) strncpy(shared->password, password, MAX_DATA);
    shared->command = COMMAND_NONE;

    switch (m) {
        case REGISTER:
            shared->modus = REGISTER;
            /* tell server to continue */
            if (sem_post(sem2) == -1) {
                error_exit("Server quit.");
            }
            if (sem_post(sem1) == -1) {
                error_exit("Server quit.");
            }
            server_waits = -1;
            /* wait for response */
            while (shared->server_down != -1 || sem_wait(sem3) == -1) {
                error_exit("Server quit");
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
                    error_exit("Unexpected status code while REGISTER:\n");
            }
            /* tell server to wait for a new request */
            break;
        case LOGIN:
            shared->modus = LOGIN;
            /* tell server to continue */
            if (sem_post(sem2) == -1) {
                error_exit("Server quit.");
            }
            if (sem_post(sem1) == -1) {
                error_exit("Server quit.");
            }
            server_waits = -1;
            /* wait for response */
            while (shared->server_down != -1 || sem_wait(sem3) == -1) {
                error_exit("Server quit.");
            }
            response = shared->status;
            switch (response) {
                case LOGIN_SUCCESS:
                    strncpy(session_id, shared->session_id, MAX_DATA);
                    while (terminating == -1) {
                        printf("Commands:\n  1) write secret\n  2) read secret\n  3) logout\nPlease select a command (1-3):\n");
                        char buffer[MAX_DATA];
                        if (fgets(buffer, sizeof buffer, stdin) == NULL) {
                            error_exit("fgets");
                        }
                        if (shared->server_down != -1) {
                            error_exit("Server quit.");
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
                                /* wait for server to allow client to send request */
                                while (shared->server_down != -1 || sem_wait(sem1) == -1) {
                                    error_exit("Server quit.");
                                }
                                server_waits = 1;
                                shared->modus = LOGIN;
                                shared->command = WRITE;
                                (void) strncpy(shared->secret, buf, MAX_DATA);
                                (void) strncpy(shared->username, username, MAX_DATA);
                                (void) strncpy(shared->password, password, MAX_DATA);
                                (void) strncpy(shared->session_id, session_id, MAX_DATA);
                                /* tell server to continue */
                                if (sem_post(sem2) == -1) {
                                    error_exit("Server quit.");
                                }
                                if (sem_post(sem1) == -1) {
                                    error_exit("Server quit.");
                                }
                                server_waits = -1;
                                /* wait for response */
                                while (shared->server_down != -1 || sem_wait(sem3) == -1) {
                                    error_exit("Server quit.");
                                }
                                response = shared->status;
                                /* tell server to wait for a new request */
                                switch (response) {
                                    case WRITE_SECRET_SUCCESS:
                                        printf("Successfully wrote the secret.\n");
                                        break;
                                    case WRITE_SECRET_FAILED:
                                        error_exit("Failed to write the secret. User does not exist in database.");
                                        break;
                                    case SESSION_FAILED:
                                        error_exit("Session auth failed.");
                                        break;
                                    default:
                                        (void) fprintf(stderr, "Unexpected response while WRITE.\n");
                                        break;
                                }
                                break;
                            case READ:
                                /* wait for server to allow client to send request */
                                while (shared->server_down != -1 || sem_wait(sem1) == -1) {
                                    error_exit("Server quit.");
                                }
                                server_waits = 1;
                                shared->modus = LOGIN;
                                shared->command = READ;
                                (void) strncpy(shared->username, username, MAX_DATA);
                                (void) strncpy(shared->password, password, MAX_DATA);
                                (void) strncpy(shared->session_id, session_id, MAX_DATA);
                                (void) strncpy(secret, shared->secret, MAX_DATA);
                                /* tell server to continue */
                                if (sem_post(sem2) == -1) {
                                    error_exit("Server quit.");
                                }
                                if (sem_post(sem1) == -1) {
                                    error_exit("Server quit.");
                                }
                                server_waits = -1;
                                /* wait for response */
                                while (shared->server_down != -1 || sem_wait(sem3) == -1) {
                                    error_exit("Server quit.");
                                }
                                response = shared->status;
                                switch (response) {
                                    case LOGIN_SUCCESS:
                                        if (strlen(secret) == 0) {
                                            (void) printf("No secret was set on server!\n");
                                            break;
                                        }
                                        (void) printf("Success! Your secret is: %s\n", secret);
                                        break;
                                    case LOGIN_FAILED:
                                        error_exit("Login failed.");
                                        break;
                                    case SESSION_FAILED:
                                        error_exit("Session auth failed.");
                                        break;
                                    default:
                                        error_exit("Unexpected response value.");
                                        break;
                                }
                                break;
                            case LOGOUT:
                                /* wait for server to allow client to send request */
                                while (shared->server_down != -1 || sem_wait(sem1) == -1) {
                                    error_exit("Server quit.");
                                }
                                server_waits = 1;
                                shared->modus = LOGIN;
                                shared->command = LOGOUT;
                                (void) strncpy(shared->username, username, MAX_DATA);
                                (void) strncpy(shared->password, password, MAX_DATA);
                                (void) strncpy(shared->session_id, session_id, MAX_DATA);
                                /* tell server to continue */
                                if (sem_post(sem2) == -1) {
                                    error_exit("Server quit.");
                                }
                                if (sem_post(sem1) == -1) {
                                    error_exit("Server quit.");
                                }
                                server_waits = -1;
                                /* wait for response */
                                while (shared->server_down != -1 || sem_wait(sem3) == -1) {
                                    error_exit("Server quit.");
                                }
                                response = shared->status;
                                switch (response) {
                                    case LOGOUT_SUCCESS:
                                        terminating = 1;
                                        (void) printf("Logout successful. Goodbye!\n");
                                        exit(EXIT_SUCCESS);
                                        break;
                                    case LOGOUT_FAILED:
                                        error_exit("Logout failed. User does not exist in database.");
                                        break;
                                    case SESSION_FAILED:
                                        error_exit("Session auth failed.");
                                        break;
                                    default:
                                        error_exit("Unexpected response value: %d.", response);
                                        break;
                                }
                                break;
                            default:
                                /* tell server to wait for a new request */
                                (void) fprintf(stderr, "Invalid command. Please try again:\n");
                                break;
                        }
                    }
                    if (shared->server_down != -1) {
                        error_exit("Server down.");
                    }
                    exit (EXIT_SUCCESS);
                    break;
                case LOGIN_FAILED:
                    error_exit("User not found in database.");
                    break;
                case SESSION_FAILED:
                    error_exit("Session auth failed.");
                    break;
                default:
                    error_exit("Unexpected status code while LOGIN:\n");
            }
            break;
        default:
            usage();
            break;
    }
}
