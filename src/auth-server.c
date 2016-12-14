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

/* === Structs === */

struct entry {
    char *username;
    char *password;
    char *secret;
    struct entry* next;
};

/* === Prototypes === */

static void usage(void);
static int parse_args(int argc, char **argv);
static void error_exit (const char *fmt, ...);
static void free_resources(void);
static void parse_database(void);
static void prepend(struct entry *data);
static void printlist(void);

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

static void free_resources(void) {
    if (shmfd != -1) {
        (void) close (shmfd);
    }
}

static void parse_database(void) {
    FILE *database;
    char line[1024];
    int i;

    if (dbname != NULL) {

        if ((database = fopen(dbname, "r")) == NULL) {
            error_exit("Couldn't open file.");
        }

        struct entry *data = malloc(sizeof(struct entry));

        while (fgets(line, 1024, database)) {
            char* tmp = strdup(line);
            char* tok;
            for (i=0, tok = strtok(line, ";"); tok && *tok; tok = strtok(NULL, ";\n"), i++) {
                switch(i) {
                    case 0:
                        data = malloc(sizeof(struct entry));
                        data->username = tok;
                        break;
                    case 1:
                        data->password = tok;
                        break;
                    case 2:
                        data->secret = tok;
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
    DEBUG("(%s,%s,%s)\n", data->username, data->password, data->secret);
//    data->next = first;
//    first = data;
}

static void printlist(void) {
    DEBUG("\n");
    struct entry *ptr = first;
    while (ptr != NULL) {
        DEBUG("[%s;%s;%s]\n", ptr->username, ptr->password, ptr->secret);
        ptr = ptr->next;
    }
}

int main(int argc, char **argv) {
    progname = argv[0];
    if (parse_args(argc, argv) == -1) {
        usage();
    }
    parse_database();
    struct entry *data = malloc (sizeof(struct entry));
//    data->username = "hello";
//    data->password = "pass";
//    data->secret = "secret";
//    data->next = malloc (sizeof(struct entry));
//    data->next->username = "world";
//    data->next->password = "pass2";
//    data->next->secret = NULL;
//    data->next->next = NULL;
//    data->next->next = malloc (sizeof(struct entry));
//    data->next->next->username = "test";
//    data->next->next->password = "pass3";
//    data->next->next->secret = "secret 2";
//    data->next->next->next = NULL;
    first = data;
    printlist();

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
