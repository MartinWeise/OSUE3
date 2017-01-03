/**
 * @file shared.c
 * @author Martin Weise <e1429167@student.tuwien.ac.at>
 * @date 17.12.2016
 *
 * @brief Shared memory file.
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

/* === Global Variables === */

volatile sig_atomic_t terminating = 0;

struct entry *first = NULL;

int shmfd = -1;

/** Action synchronize */
sem_t *sem1;
sem_t *sem2;
sem_t *sem3;

struct shared_command *shared = NULL;

char *progname;

static const char *STATUS_STRING[] = {
    "STATUS_NONE", "LOGIN_SUCCESS", "LOGIN_FAILED", "REGISTER_SUCCESS", "LOGOUT_SUCCESS",
    "LOGOUT_FAILED", "REGISTER_FAILED", "WRITE_SECRET_SUCCESS", "WRITE_SECRET_FAILED",
};

static const char *MODE_STRING[] = {
    "MODE_UNSET", "REGISTER", "LOGIN",
};

static const char *CMD_STRING[] = {
    "COMMAND_NONE", "WRITE", "READ", "LOGOUT",
};
/* === Implementations === */

void debug_info(struct shared_command *ptr) {
    int s1, s2, s3;
    sem_getvalue(sem1, &s1);
    sem_getvalue(sem2, &s2);
    sem_getvalue(sem3, &s3);

    DEBUG("======================\n"
        "       DEBUG INFO\n"
        "======================\n"
        "sem1: %d,\n"
        "sem2: %d,\n"
        "sem3: %d\n", s1, s2, s3);
    DEBUG("shared = {\n"                        \
        "    status: %s,\n"                     \
        "    modus: %s,\n"                      \
        "    command: %s,\n"                    \
        "    id: '%s',\n"                       \
        "    username: '%s',\n"                 \
        "    password: '%s',\n"                 \
        "    secret: '%s'\n"                    \
        "}\n", STATUS_STRING[ptr->status], MODE_STRING[ptr->modus], CMD_STRING[ptr->command],
          ptr->session_id, ptr->username, ptr->password, ptr->secret);
}