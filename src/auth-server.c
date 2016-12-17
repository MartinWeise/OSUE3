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

static void usage(void);
static int parse_args(int argc, char **argv);
static void error_exit (const char *fmt, ...);
static void free_resources(void);
static void parse_database(void);
static void signal_handler(int sig);
static void save(void);

/* === Global Variables === */

static char *progname;
static int shmfd = -1;
static char *dbname = NULL;
static struct shared_command *shared = NULL;
static struct entry *first = NULL;
volatile sig_atomic_t terminating = 0;
static sem_t *sem;

/* === Implementations === */

static void usage(void) {
    (void) fprintf (stderr, "USAGE: %s [-l database]\n", progname);
    exit (EXIT_FAILURE);
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
                        data->username = strdup(tok);
                        break;
                    case 1:
                        data->password = strdup(tok);
                        break;
                    case 2:
                        data->secret = strdup(tok);
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
    /* free database */
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
    if (sem_close(sem) == -1) {
        error_exit("Couldn't close semaphore.");
    }
}

static void save(void) {
    FILE *db;
    struct entry *ptr = first;

    if ((db = fopen("auth-server.db.csv", "w+")) == NULL) {
        error_exit("Couldn't open the database file.");
    }
    DEBUG("Saving to auth-server.db.csv.\n");
    while (ptr != NULL) {
        fprintf(db, "%s;%s;%s\n", ptr->username, ptr->password, ptr->secret);
        ptr = ptr->next;
    }
}

static void signal_handler(int sig) {
    DEBUG("Signal caught: %s\n", (sig==SIGINT ? "SIGINT" : "OTHER"));
    free_resources();
    exit (EXIT_SUCCESS);
}

int main(int argc, char **argv) {
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

    if (parse_args(argc, argv) == -1) {
        usage();
    }
    parse_database();

    /* Open shared memory object SHM_NAME in for reading and writing,
     * create it if it does not exist */
    if ((shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT, PERMISSION)) == -1) {
        error_exit("Couldn't init shared.");
    }
    /* Extend set size */
    if (ftruncate(shmfd, sizeof *shared) == -1) {
        error_exit("Couldn't extend shared size.");
    }
    shared = malloc(sizeof(struct shared_command));
    shared->data = malloc(sizeof(struct entry));
    /* Create a new mapping, let the kernel choose the address at which to create the memory  */
    if ((shared = mmap(NULL, sizeof *shared, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0)) == MAP_FAILED) {
        error_exit("Couldn't create mapping.");
    }
    /* Create Semaphores */
    if ((sem = sem_open(SEM_NAME, O_CREAT | O_EXCL | O_TRUNC, PERMISSION, 1)) == SEM_FAILED) {
        error_exit("Couldn't create semaphore.");
    }

//    DEBUG("Server running ...\n");
//    for(int i = 0; i < 3; ++i) {
//        sem_wait(sem);
//        DEBUG("critical: %s: i = %d\n", argv[0], i);
//        sleep(1);
//        sem_post(sem);
//    }
    DEBUG("OK\n");
//    while(1) {
//        if (shared != NULL) {
//            DEBUG("%s\n",shared->data->username);
//            break;
//        }
//    }

    save();

    free_resources();
    return EXIT_SUCCESS;
}
