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
#define MAX_DATA (50)

/* === Structs === */

struct entry {
    char *username;
    char *password;
    char *secret;
    struct entry* next;
};

struct shm {
    unsigned int state;
    unsigned int data[MAX_DATA];
};