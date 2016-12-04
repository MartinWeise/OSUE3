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


/* === Constants === */


#define SHM_NAME "/1429167database"
#define MAX_DATA (50)
#define PERMISSION (0600)

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

/* === Structs === */

struct myshm {
    unsigned int state;
    unsigned int data[MAX_DATA];
};

/* === Global Variables === */

static char *progname;
static int shmfd = -1;

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
                DEBUG("database: %s\n", optarg);
                if (optarg == NULL) {
                    return -1;
                }
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

int main(int argc, char **argv) {
    struct myshm *shared;

    progname = argv[0];
    if (parse_args(argc, argv) == -1) {
        usage();
    }

    /* Open shared memory object SHM_NAME in for reading and writing,
     * create it if it does not exist */
    if ((shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT, PERMISSION)) == -1) {
        error_exit("Couldn't init shared.");
    }

    /* Extend set size */
    if (ftruncate(shmfd, sizeof *shared) == -1) {
        error_exit("Couldn't extend shared size.");
    }

    free_resources();
    return EXIT_SUCCESS;
}
