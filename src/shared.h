/**
 * @file shared.h
 * @author Martin Weise <e1429167@student.tuwien.ac.at>
 * @date 13.12.2016
 *
 * @brief Shared memory header file.
 *
 **/

/* === Constants === */

#define SHM_NAME "/1429167fragment"
#define PERMISSION (0600)
#define MAX_DATA (100)

#define SEM_NAME "/sem_1"

/* === Enums === */

typedef enum {
    WRITE, READ, LOGOUT
} cmd;

typedef enum {
    REGISTER, LOGIN
} mode;

/* === Structs === */

struct entry {
    char *username;
    char *password;
    char *secret;
    struct entry* next;
};

/**
 * Defines a shared memory consisting a command and data from the client.
 */
struct shared_command {
    struct entry *data;
    mode modus;
    cmd command;
};
