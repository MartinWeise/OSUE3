/**
 * @file auth-server.c
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
#include <errno.h>
#include <memory.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <semaphore.h>
#include <sys/time.h>
#include "shared.h"

/* === Prototypes === */
/**
 * @brief Method to handle certain signals.
 * @details Is invoked on SIGINT and SIGTERM.
 * @param sig Signal code.
 */
static void signal_handler(int sig);
/**
 * @brief Exists the program with a given message.
 * @param fmt Formatted string for parsing the latter arguments to.
 */
static void error_exit (const char *fmt, ...);
/**
 * @brief Frees the used resources.
 * @details This method is also invoked when the signals SIGINT and SIGTERM occur.
 */
static void free_resources(void);
/**
 * @brief Prints a nice usage message.
 */
static void usage(void);
/**
 * @brief Set the operation mode (register | login) and verify only one mode is given.
 * @param argc The argument counter.
 * @param argv The argument vector.
 */
static int parse_args(int argc, char **argv);
/**
 * @brief Reads data from the specified csv file and add it to the linked lsit.
 * @details The database must contain not more than 3 columns.
 */
static void parse_database(void);
/**
 * @brief Saves the linked list database to the file ./auth-server.db.csv
 */
static void save(void);
/**
 * @brief Add an entry to the linked list database.
 * @param update The new entry.
 * @return 1 on success, -1 on error.
 */
static int prepend(struct shared_command *update);
/**
 * @brief Traverse the linked list database for a given entry.
 * @param update The entry that should be found.
 * @details Only compares username and password. Other attributes are ignored.
 * @return The user entry on success, NULL otherwise.
 */
static struct entry *search(struct shared_command *update);
/**
 * @brief Generate a random id containing alphanumeric characters.
 * @details Length is always 20.
 * @return The generated id.
 */
static char *rdm_id(void);
/**
 * @brief The program entry point.
 * @param argc The argument vector.
 * @param argv The argument counter.
 * @return EXIT_SUCCESS on succesful program execution, EXIT_FAILURE otherwise.
 */
int main(int argc, char **argv);

/* === Global Variables === */

/** @brief The shared memory file descriptor */
static int shmfd;
/** @brief Used to terminate the client only once. */
static volatile sig_atomic_t terminating;
/** @brief Semaphor to allow client the initial request. @details Is a binary semaphor. */
extern sem_t *sem1;
/** @brief Semaphor to allow client further commands when logged in. @details Is a binary semaphor. */
extern sem_t *sem2;
/** @brief Semaphor to sync waiting for response from server. @details Is a binary semaphor. */
extern sem_t *sem3;
/** @brief Stores the user in a linked list */
static struct entry *first;
/** @brief Holds the program name. */
static char *progname;
/** @brief Holds the database name. @details If specified in the argument vector, the value should
 *         equal a filename in csv format */
static char *dbname = NULL;
/** @brief The shared fragment between server and client */
static struct shared_command *shared = NULL;
/** @brief Used to save the database only once. */
static int saved = -1;

/* === Implementations === */

static void usage(void) {
    (void) fprintf (stderr, "USAGE: %s [-l database]\n", progname);
    exit (EXIT_FAILURE);
}

static int parse_args(int argc, char **argv) {
    int flag_l = -1;
    char opt;
    if (argc == 1) {
        return 1;
    }
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
            default:
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
                        (void) strncpy(data->username, tok, MAX_DATA);
                        break;
                    case 1:
                        (void) strncpy(data->password, tok, MAX_DATA);
                        break;
                    case 2:
                        (void) strncpy(data->secret, tok, MAX_DATA);
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

    if (saved != -1) {
        return;
    }
    if ((db = fopen("auth-server.db.csv", "w+")) == NULL) {
        error_exit("Couldn't open the database file.");
    }
    DEBUG("\nSaving to auth-server.db.csv.\n");
    while (ptr != NULL) {
        (void) fprintf(db, "%s;%s;%s\n", ptr->username, ptr->password, ptr->secret);
        DEBUG("> u: %s; p: %s; s: %s; sessid: %s\n", ptr->username, ptr->password, ptr->secret, ptr->session_id);
        ptr = ptr->next;
    }
    if (fclose(db) == -1) {
        error_exit("Failed to close save file.");
    }
    saved = 1;
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
    /* Unlink semaphor */
    if (sem_unlink(SEM1_NAME)) {
        error_exit("Couldn't unlink sempaphor 1.");
    }
    if (sem_unlink(SEM2_NAME)) {
        error_exit("Couldn't unlink sempaphor 2.");
    }
    if (sem_unlink(SEM3_NAME)) {
        error_exit("Couldn't unlink sempaphor 3.");
    }
}

static void signal_handler(int sig) {
    free_resources();
    exit (EXIT_SUCCESS);
}

static int prepend(struct shared_command *update) {
    if (search(update) != NULL) {
        return -1;
    }
    struct entry *tmp = malloc(sizeof(struct entry));
    (void) strncpy(tmp->username, update->username, MAX_DATA);
    (void) strncpy(tmp->password, update->password, MAX_DATA);
    (void) strncpy(tmp->secret, update->secret, MAX_DATA);
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

static char *rdm_id(void) {
    char *s = malloc(SIZE_SESS_ID);
    char *chars = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789";
    (void) srand(time(NULL));
    for(int i = 0; i < SIZE_SESS_ID; i++) {
        s[i] = chars[rand() % strlen(chars)];
    }
    return s;
}

int main(int argc, char **argv) {
    const int signals[] = {SIGINT, SIGTERM};
    struct sigaction s;
    sigset_t blocked_signals;
    struct entry *tmp = malloc(sizeof(struct entry));

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
    /* Create Semaphores */
    if ((sem1 = sem_open(SEM1_NAME, O_CREAT | O_EXCL, PERMISSION, 0)) == SEM_FAILED) {
        error_exit("Couldn't create semaphore 1.");
    }
    if ((sem2 = sem_open(SEM2_NAME, O_CREAT | O_EXCL, PERMISSION, 0)) == SEM_FAILED) {
        error_exit("Couldn't create semaphore 2.");
    }
    if ((sem3 = sem_open(SEM3_NAME, O_CREAT | O_EXCL, PERMISSION, 0)) == SEM_FAILED) {
        error_exit("Couldn't create semaphore 3.");
    }

    DEBUG("Server running ...\n");

    while (1) {
        /* allow one client to send request */
        (void) sem_post(sem1);
        /* wait for request */
        (void) sem_wait(sem2);
        switch (shared->modus) {
            case LOGIN:
                switch (shared->command) {
                    case WRITE:
                        if ((tmp = search(shared)) == NULL) {
                            shared->status = WRITE_SECRET_FAILED;
                        } else if (strcmp(tmp->session_id, shared->session_id) == 0) {
                            /* Save secret in database */
                            (void) strncpy(tmp->secret, shared->secret, MAX_DATA);
                            shared->status = WRITE_SECRET_SUCCESS;
                        } else {
                            shared->status = SESSION_FAILED;
                        }
                        break;
                    case READ:
                        if ((tmp = search(shared)) == NULL) {
                            shared->status = LOGIN_FAILED;
                        } else if (strcmp(tmp->session_id, shared->session_id) == 0) {
                            /* Write secret to fragment */
                            (void) strncpy(shared->secret, tmp->secret, MAX_DATA);
                            shared->status = LOGIN_SUCCESS;
                        } else {
                            shared->status = SESSION_FAILED;
                        }
                        break;
                    case LOGOUT:
                        if ((tmp = search(shared)) == NULL) {
                            shared->status = LOGOUT_FAILED;
                        } else if (strcmp(tmp->session_id, shared->session_id) == 0) {
                            /* destroy session id */
                            memset(tmp->session_id, 0, sizeof tmp->session_id);
                            shared->status = LOGOUT_SUCCESS;
                        } else {
                            shared->status = SESSION_FAILED;
                        }
                        break;
                    default:
                        if ((tmp = search(shared)) == NULL) {
                            shared->status = LOGIN_FAILED;
                        } else {
                            char *id = rdm_id();
                            (void) strncpy(tmp->session_id, id, MAX_DATA);
                            (void) strncpy(shared->session_id, id, MAX_DATA);
                            shared->status = LOGIN_SUCCESS;
                        }
                        break;
                }
                /* tell client to continue */
                (void) sem_post(sem3);
                break;
            case REGISTER:
                if (prepend(shared) == -1) {
                    shared->status = REGISTER_FAILED;
                } else {
                    shared->status = REGISTER_SUCCESS;
                }
                /* tell client to continue */
                (void) sem_post(sem3);
                break;
            default:
                break;
        }
    }
}
