/**
 * @file auth-server.c
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
#include <sys/time.h>
#include "shared.h"

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

/* === Prototypes === */

static void signal_handler(int sig);
static void error_exit (const char *fmt, ...);
static void free_resources(void);
static void usage(void);
static int parse_args(int argc, char **argv);
static void parse_database(void);
static void save(void);
static int prepend(struct shared_command *update);
static struct entry *search(struct shared_command *update);
static int update_secret(struct shared_command *update);

/* === Global Variables === */

extern int shmfd;
extern sig_atomic_t terminating;
extern sem_t *sem_server;

static struct entry *first;
static char *progname;
static char *dbname = NULL;
static struct shared_command *shared = NULL;

/* === Implementations === */

static void usage(void) {
    (void) fprintf (stderr, "USAGE: %s [-l database]\n", progname);
    exit (EXIT_FAILURE);
}

static int parse_args(int argc, char **argv) {
    int flag_l = -1;
    char opt;
    if (argc > 3 || argc == 2) {
        return -1;
    }
    while ((opt = getopt (argc, argv, "l:")) != -1) {
        switch (opt) {
            case 'l':
                if (flag_l != -1) {
                    usage();
                }
                if (optarg == NULL) {
                    return -1;
                }
                dbname = optarg;
                DEBUG("Database is %s.\n", dbname);
                flag_l = 1;
                break;
            default: // ?
                break;
        }
    }
    return 0;
}

static void parse_database(void) {
    FILE *database;
    char line[MAX_DATA];
    int i;

    if (dbname != NULL) {

        if ((database = fopen(dbname, "r")) == NULL) {
            error_exit("Couldn't open file.");
        }

        struct entry *data;

        while (fgets(line, MAX_DATA, database)) {
            char* tmp = strdup(line);
            char* tok;
            for (i=0, tok = strtok(tmp, ";"); tok && *tok; tok = strtok(NULL, ";\n"), i++) {
                switch(i) {
                    case 0:
                        data = malloc(sizeof(struct entry));
                        strncpy(data->username, tok, MAX_DATA);
                        break;
                    case 1:
                        strncpy(data->password, tok, MAX_DATA);
                        break;
                    case 2:
                        strncpy(data->secret, tok, MAX_DATA);
                        break;
                    default:
                        error_exit("Malformed input data.");
                }
            }
            data->next = first;
            first = data;
            free(tmp);
        }
    }
}

static void save(void) {
    FILE *db;
    struct entry *ptr = first;

    if ((db = fopen("auth-server.db.csv", "w+")) == NULL) {
        error_exit("Couldn't open the database file.");
    }
    DEBUG("\nSaving to auth-server.db.csv.\n");
    while (ptr != NULL) {
        fprintf(db, "%s;%s;%s\n", ptr->username, ptr->password, ptr->secret);
        DEBUG("> %s;%s;%s\n", ptr->username, ptr->password, ptr->secret);
        ptr = ptr->next;
    }
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

    save();
    DEBUG("Shutting down now.\n");
    exit (EXIT_FAILURE);
}

static void free_resources(void) {
    struct entry *temp;
    if (terminating == 1) {
        return;
    }
    terminating = 1;
    /* Close shared memory */
    if (shmfd != -1) {
        (void) close (shmfd);
    }
    /* save database */
    save();
    /* Free all space from linked list */
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
//    /* Close semaphor */
//    if (sem_close(sem_server) == -1) {
//        error_exit("Couldn't remove semaphor 1.");
//    }
//    /* Unlink semaphor */
//    if (sem_unlink(SEM1_NAME)) {
//        error_exit("Couldn't unlink sempaphor 1.");
//    }
}

static void signal_handler(int sig) {
    free_resources();
    exit (EXIT_SUCCESS);
}

static int update_secret(struct shared_command *update) {
    struct entry *tmp = first;
    print_shared(update);
    while (1) {
        if (tmp != NULL) {
            if (strcmp(tmp->username, update->username) == 0
                && strcmp(tmp->password, update->password) == 0) {
                /* registered user found */
                strncpy(tmp->secret, update->secret, MAX_DATA);
                return 1;
            }
            tmp = tmp->next;
            continue;
        }
        return -1;
    }
}

static int prepend(struct shared_command *update) {
    if (search(update) != NULL) {
        return -1;
    }
    struct entry *tmp = malloc(sizeof(struct entry));
    strncpy(tmp->username, update->username, MAX_DATA);
    strncpy(tmp->password, update->password, MAX_DATA);
    strncpy(tmp->secret, update->secret, MAX_DATA);
    tmp->next = first;
    first = tmp;
    return 1;
}

static struct entry *search(struct shared_command *update) {
    struct entry *tmp = first;
    while (1) {
        if (tmp != NULL) {
            if (strcmp(update->username, tmp->username) == 0 && strcmp(update->password, tmp->password) == 0) {
                /* registered user found */
                return tmp;
            }
            tmp = tmp->next;
            continue;
        }
        free (tmp);
        return NULL;
    }
}

int main(int argc, char **argv) {
    const int signals[] = {SIGINT, SIGTERM};
    struct sigaction s;
    sigset_t blocked_signals;
    struct entry *tmp;

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

    if (parse_args(argc, argv) == -1) {
        usage();
    }
    parse_database();

    /* Open shared memory object SHM_NAME in for reading and writing,
     * create it if it does not exist */
    if ((shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT, PERMISSION)) == -1) {
        error_exit("Couldn't init shared fragment.");
    }
    /* Extend set size */
    if (ftruncate(shmfd, sizeof *shared) == -1) {
        error_exit("Couldn't extend shared size.");
    }
    /* Create a new mapping, let the kernel choose the address at which to create the memory  */
    if ((shared = mmap(NULL, sizeof *shared, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0)) == MAP_FAILED) {
        error_exit("Couldn't create mapping.");
    }
    if (shared == (struct shared_command*) -1) {
        error_exit("mmap did not init a shared_command.");
    }
//    /* Create Semaphores */
//    if ((sem_server = sem_open(SEM1_NAME, O_CREAT | O_EXCL, PERMISSION, 1)) == SEM_FAILED) {
//        error_exit("Couldn't create semaphore.");
//    }
    DEBUG("Server running ...\n");
    while (1) {
        /* reserve semaphor here */
        switch (shared->modus) {
            case LOGIN:
                switch (shared->command) {
                    case WRITE:
                        if (update_secret(shared) == -1) {
                            shared->status = WRITE_SECRET_FAILED;
                        } else {
                            shared->status = WRITE_SECRET_SUCCESS;
                        }
                        /* reset */
                        shared->modus = MODE_UNSET;
                        shared->command = COMMAND_NONE;
                        /* release semaphor here */
                        break;
                    case READ:
                        /* reserve semaphore here */
                        if ((tmp = search(shared)) == NULL) {
                            shared->status = LOGIN_FAILED;
                        } else {
                            strncpy(shared->secret, tmp->secret, MAX_DATA);
                            shared->status = LOGIN_SUCCESS;
                        }
                        /* reset */
                        shared->modus = MODE_UNSET;
                        shared->command = COMMAND_NONE;
                        /* release semaphor here */
                        break;
                    default:
                        break;
                }
                break;
            case REGISTER:
                /* reserve semaphor here */
                if (prepend(shared) == -1) {
                    shared->status = REGISTER_FAILED;
                } else {
                    shared->status = REGISTER_SUCCESS;
                }
                shared->modus = MODE_UNSET;
                shared->command = COMMAND_NONE;
                print_shared(shared);
                /* release semaphor here */
                break;
            default:
                break;
        }
    }
    return EXIT_SUCCESS;
}
