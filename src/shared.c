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

sem_t *sem_client;

sem_t *sem_server;

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

void print_shared(struct shared_command *ptr) {
    DEBUG("shared = {\n"                        \
        "    status: %s,\n"                     \
        "    modus: %s,\n"                      \
        "    command: %s,\n"                    \
        "    id: %d,\n"                         \
        "    username: '%s',\n"                 \
        "    password: '%s',\n"                 \
        "    secret: '%s'\n"                    \
        "}\n", STATUS_STRING[ptr->status], MODE_STRING[ptr->modus], CMD_STRING[ptr->command],
          1, ptr->username, ptr->password, ptr->secret);
}