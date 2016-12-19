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

extern void signal_handler(int sig);
extern void error_exit (const char *fmt, ...);
extern void free_resources(void);

static void usage(void);
static int parse_args(int argc, char **argv);
static void parse_database(void);
static int login(char *username, char *password);
static void save(void);
static int registeR(char *username, char *password);

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

static int login(char *username, char *password) {
    struct entry *tmp = first;
    while (tmp != NULL) {
        if (strcmp(tmp->username, username) == 0 && strcmp(tmp->password, password) == 0) {
            /* copy secret for READ command */
            strncpy(shared->secret, tmp->secret, MAX_DATA);
            return 1;
        }
        tmp = tmp->next;
    }
    return -1;
}

static int registeR(char *username, char *password) {
    struct entry *ptr = first;
    while (ptr != NULL) {
        if (strcmp(ptr->username, username) == 0 && strcmp(ptr->password, password) == 0) {
            return -1;
        }
        ptr = ptr->next;
    }
    struct entry *data = malloc(sizeof(struct entry));
    data->next = first;
    first = data;
    return 1;
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
    strncpy(shared->username,"hello", MAX_DATA);
    /* Create Semaphores */
    if ((sem_server = sem_open(SEM_NAME, O_CREAT | O_EXCL, PERMISSION, 1)) == SEM_FAILED) {
        error_exit("Couldn't create semaphore.");
    }
    DEBUG("Server running ...\n");
    while(1) {
        switch (shared->modus) {
            case REGISTER:
                shared->status = (registeR(shared->username, shared->password) == -1
                                  ? REGISTER_FAILED : REGISTER_SUCCESS);
                break;
            case LOGIN:
                switch (shared->command) {
                    case WRITE:
//                        DEBUG("Login => WRITE\n");
                        break;
                    case READ:
//                        DEBUG("Login => READ\n");
                        if (login(shared->username, shared->password) == -1) {
                            shared->status = LOGIN_FAILED;
                            break;
                        }
                        shared->status = LOGIN_SUCCESS;
                        break;
                    case LOGOUT:
//                        DEBUG("Login => LOGOUT\n");
                        break;
                    default:
                        break;
                }
                break;
            default:
                DEBUG("Modus: %d\n", shared->modus);
                assert(0==1);
        }
    }

    save();

    free_resources();
    return EXIT_SUCCESS;
}
