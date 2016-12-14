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
static void prepend(struct entry *data);

/* === Global Variables === */

static char *progname;
static int shmfd = -1;
static char *dbname = NULL;
static struct shm *shared;
static struct entry *first = NULL;

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

/**
 * OMG use strdup u stupid prick
 */
static void parse_database(void) {
    FILE *database;
    char line[1024];
    int i;

    if (dbname != NULL) {

        if ((database = fopen(dbname, "r")) == NULL) {
            error_exit("Couldn't open file.");
        }

        struct entry *data;

        while (fgets(line, 1024, database)) {
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
            prepend(data);
            free(tmp);
        }
    }
}

static void prepend(struct entry *data) {
    data->next = first;
    first = data;
}

static void free_resources(void) {
    struct entry *temp;

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
//    /* Remove shared memory object */
//    if (shm_unlink(SHM_NAME) == -1) {
//        error_exit("Couldn't remove shared memory.");
//    }
}

int main(int argc, char **argv) {
    progname = argv[0];
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

    /* Create a new mapping, let the kernel choose the address at which to create the memory  */
    if ((shared = mmap(NULL, sizeof *shared, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0)) == MAP_FAILED) {
        error_exit("Couldn't create mapping.");
    }


    free_resources();
    return EXIT_SUCCESS;
}
