
/* === Constants === */

#define SHM_NAME "/1429167fragment"
#define PERMISSION (0600)
#define MAX_DATA (100)

#define SEM_NAME "/1429167sem"

/* === Enums === */

typedef enum {
    WRITE, READ, LOGOUT
} cmd;

typedef enum {
    REGISTER, LOGIN
} mode;

/* === Structs === */

struct entry {
    char username[MAX_DATA];
    char password[MAX_DATA];
    char secret[MAX_DATA];
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

void error_exit (const char *fmt, ...);
void free_resources(void);
void signal_handler(int sig);
