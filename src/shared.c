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

/* === Implementations === */
