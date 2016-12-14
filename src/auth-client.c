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
#include <errno.h>
#include <memory.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <assert.h>
#include "shared.h"


/* === Constants === */

/** The buffer size. */
#define BUFFER_SIZE 512

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

/* === Global Variables === */

char *progname;

/* === Prototypes === */

static void usage();
static void parse_args(int argc, char **argv);


/* === Implementations === */

static void usage() {
    fprintf(stderr, "USAGE: %s { -r | -l } username password\n", progname);
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
    DEBUG("argc %d, optind %d\n",argc, optind);
    while ((opt = getopt (argc, argv, "r:l:")) != -1) {
        switch (opt) {
            case 'l':
                if (flag_l != -1 || optarg == NULL) {
                    usage();
                }
                flag_l = 1;
                break;
            case 'r':
                if (flag_r != -1 || optarg == NULL) {
                    usage();
                }
                flag_r = 1;
                break;
            default: // ?
                break;
        }
    }
}

int main(int argc, char **argv) {
    parse_args(argc, argv);
}
