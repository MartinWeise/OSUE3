/**
 * @file shared.h
 * @author Martin Weise <e1429167@student.tuwien.ac.at>
 * @date 03.01.2017
 *
 * @brief Shared memory + debug utils header file.
 *
 **/

/* === Constants === */

/** @brief File name of the shared fragment */
#define SHM_NAME "/1429167fragment"
/** @brief Permission of the semaphor files created in /dev/shm/ */
#define PERMISSION (0660)
/** @brief Maximum length of all input data strings. */
#define MAX_DATA (100)

/** @brief File name of semaphor 1. */
#define SEM1_NAME "/1429167sem1"
/** @brief File name of semaphor 2. */
#define SEM2_NAME "/1429167sem2"
/** @brief File name of semaphor 3. */
#define SEM3_NAME "/1429167sem3"

/* === Enums === */

/** @brief Possible commands when the client is logged-in. */
typedef enum {
    COMMAND_NONE, WRITE, READ, LOGOUT
} cmd;
/** @brief Possible operating modes of the client. */
typedef enum {
    MODE_UNSET, REGISTER, LOGIN
} mode;
/** @brief Possible status codes in shared_command. */
typedef enum {
    STATUS_NONE, SESSION_FAILED, LOGIN_SUCCESS, LOGIN_FAILED, REGISTER_SUCCESS, LOGOUT_SUCCESS,
    LOGOUT_FAILED, REGISTER_FAILED, WRITE_SECRET_SUCCESS, WRITE_SECRET_FAILED
} status;

/* === Structs === */

/**
 * @brief Defines an entry in the database of the server.
 */
struct entry {
    char username[MAX_DATA];
    char password[MAX_DATA];
    char secret[MAX_DATA];
    char session_id[MAX_DATA];
    struct entry* next;
};

/**
 * @brief Defines a shared memory consisting a command and data from the client.
 */
struct shared_command {
    status status;
    mode modus;
    cmd command;
    char session_id[MAX_DATA];
    char username[MAX_DATA];
    char password[MAX_DATA];
    char secret[MAX_DATA];
};

/* === Macros === */

/**
 * @brief Provides a debugging function to output status messages
 * @details Activate/Deactivate by adding/removing -DENDEBUG to DEFS in Makefile.
 */
#ifdef ENDEBUG
#define DEBUG(...) do { fprintf(stderr, __VA_ARGS__); } while(0)
#else
#define DEBUG(...)
#endif
