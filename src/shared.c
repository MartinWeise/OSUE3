/**
 * @file shared.c
 * @author Martin Weise <e1429167@student.tuwien.ac.at>
 * @date 03.01.2017
 *
 * @brief Shared memory + debug utils file.
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

/** @brief Semaphor to allow client the initial request. @details Is a binary semaphor. */
sem_t *sem1;
/** @brief Semaphor to allow client further commands when logged in. @details Is a binary semaphor. */
sem_t *sem2;
/** @brief Semaphor to sync waiting for response from server. @details Is a binary semaphor. */
sem_t *sem3;
