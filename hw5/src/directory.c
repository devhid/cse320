#include "directory.h"
#include "csapp.h"
#include "debug.h"

typedef struct directory DIRECTORY;
typedef struct dir_node DIR_NODE;
typedef struct client_info CLIENT_INFO;

struct directory {
    // Pointer to first node in the list.
    DIR_NODE *head;

    // Length of the linked list.
    uint32_t length;

    // Mutex lock for thread safety.
    pthread_mutex_t lock;
};

struct dir_node {
    CLIENT_INFO *info;

    DIR_NODE *prev;
    DIR_NODE *next;
};

struct client_info {
    char *handle;
    int sockfd;
    MAILBOX *mailbox;
};

static DIR_NODE *get_node(char *handle);
static void insert_into_dir(DIR_NODE *node);
static void remove_from_dir(DIR_NODE *node);
// static void print_all_handles();

// Reference to the directory.
DIRECTORY *directory;

void dir_init(void) {
    debug("--- dir_init() ---");
    directory = calloc(sizeof(struct directory), sizeof(char));
    directory->length = 0;

    if(pthread_mutex_init(&directory->lock, NULL) < 0) {
        debug("dir_init(): pthread_mutex_init() returned -1.");
        free(directory);
        return;
    }
}


void dir_shutdown(void) {
    debug("--- dir_shutdown() ---");
    DIR_NODE *head = directory->head;

    if(pthread_mutex_lock(&directory->lock) < 0) {
        debug("dir_shutdown(): pthread_mutex_lock() returned -1.");
        return;
    }

    while(head != NULL) {
        shutdown(head->info->sockfd, SHUT_RDWR);
        head = head->next;
    }

    if(pthread_mutex_unlock(&directory->lock) < 0) {
        debug("dir_shutdown(): pthread_mutex_unlock() returned -1.");
        return;
    }

}

void dir_fini(void) {
    debug("--- dir_fini() ---");
    pthread_mutex_destroy(&directory->lock);

    free(directory);
}

MAILBOX *dir_register(char *handle, int sockfd) {
    debug("--- dir_register(): ---");

    // If the directory is defunct, return NULL.
    if(directory == NULL) {
        return NULL;
    }

    if(pthread_mutex_lock(&directory->lock) < 0) {
        debug("dir_register(): pthread_mutex_lock() returned -1.");
        return NULL;
    }

    // If the handle is already registered, return NULL.
    DIR_NODE *node = get_node(handle);
    if(node != NULL) {
        pthread_mutex_unlock(&directory->lock);
        return NULL;
    }

    // Reference to head of directory.
    DIR_NODE *head = directory->head;

    // Loop through directory until we find an empty node.
    while(head != NULL) {
        head = head->next;
    }

    // Allocate memory for the node.
    head = calloc(sizeof(struct dir_node), sizeof(char));

    // Initialize a new mailbox for the handle.
    MAILBOX *mailbox = mb_init(handle);

    // Increase reference count of mailbox.
    mb_ref(mailbox);

    // Initiailize client info.
    CLIENT_INFO *info = calloc(sizeof(struct client_info), sizeof(char));

    info->handle = calloc(sizeof(handle), sizeof(char));
    strncpy(info->handle, handle, strlen(handle));

    info->sockfd = sockfd;
    info->mailbox = mailbox;

    // Store the client info in the directory.
    head->info = info;

    // Store the new handle in the directory.
    insert_into_dir(head);

    if(pthread_mutex_unlock(&directory->lock) < 0) {
        debug("dir_register(): pthread_mutex_unlock() returned -1.");
        return NULL;
    }

    // Return the mailbox.
    return mailbox;
}

void dir_unregister(char *handle) {
    debug("--- dir_unregister(): ---");
    // Reference to head of the directory linked list.
    DIR_NODE *head = directory->head;

    if(pthread_mutex_lock(&directory->lock) < 0) {
        debug("dir_unregister(): pthread_mutex_lock() returned -1.");
        return;
    }

    // Find the node whose handle is equal to the specified handle.
    for(; head != NULL && strcmp(head->info->handle, handle) != 0; head = head->next);

    // If such a handle was found in the directory, unregister it.
    if(head != NULL) {
        // Shut down the client socket.
        shutdown(head->info->sockfd, SHUT_RDWR);

        // Shut the mailbox down and remove any reference to it.
        mb_shutdown(head->info->mailbox);
        mb_unref(head->info->mailbox);

        // Remove the directory from linked list.
        remove_from_dir(head);

        // Free the node.
        free(head->info->handle);
        free(head->info);
        free(head);

        if(directory->head == head) {
            directory->head = NULL;
        } else {
            head = NULL;
        }
    }

    if(pthread_mutex_unlock(&directory->lock) < 0) {
        debug("dir_unregister(): pthread_mutex_unlock() returned -1.");
        return;
    }
}

MAILBOX *dir_lookup(char *handle) {
    debug("--- dir_lookup() ---");

    DIR_NODE *head = directory->head;

    if(pthread_mutex_lock(&directory->lock) < 0) {
        debug("dir_lookup(): pthread_mutex_lock() returned -1.");
        return NULL;
    }

    for(; head != NULL; head = head->next) {
        if(strcmp(head->info->handle, handle) == 0) {
            debug("dir_lookup(): Matching handle found: %s", handle);

            MAILBOX *mailbox = head->info->mailbox;
            mb_ref(mailbox);

            pthread_mutex_unlock(&directory->lock);

            return mailbox;
        }
    }

    if(pthread_mutex_unlock(&directory->lock) < 0) {
        debug("dir_lookup(): pthread_mutex_unlock() returned -1.");
        return NULL;
    }

    return NULL;
}


char **dir_all_handles(void) {
    debug("--- dir_all_handles() ---");

    if(pthread_mutex_lock(&directory->lock) < 0) {
        debug("dir_all_handles(): pthread_mutex_lock() returned -1.");
        return NULL;
    }

    char **handles = calloc(directory->length + 1, sizeof(char *));
    for(int i = 0; i < directory->length; i++) {
        handles[i] = calloc(sizeof(char *), sizeof(char));
    }

    DIR_NODE *node = directory->head;
    for(int i = 0; node != NULL; node = node->next) {
        strncpy(handles[i], node->info->handle, strlen(node->info->handle));
        i++;
    }

    handles[directory->length] = NULL;

    if(pthread_mutex_unlock(&directory->lock) < 0) {
        debug("dir_all_handles(): pthread_mutex_unlock() returned -1.");
        return NULL;
    }

    return handles;
}

static DIR_NODE *get_node(char *handle) {
    DIR_NODE *head = directory->head;

    for(; head != NULL && strcmp(head->info->handle, handle) != 0; head = head->next);

    return head;
}

static void insert_into_dir(DIR_NODE *node) {
    // Case #1: We are inserting the head.
    if(directory->length == 0) {
        directory->head = node;
    } else {
        DIR_NODE *head = directory->head;

        // Case #2: Every other node would be added to the end of the linked list.
        while(head->next != NULL) {
            head = head->next;
        }

        head->next = node;
        node->prev = head;
    }

    // Increment the directory length.
    (directory->length)++;
}

static void remove_from_dir(DIR_NODE *node) {
    DIR_NODE *head = directory->head;

    // Case #1: We are removing the head.
    if(head == node) {
        if(node->next != NULL) {
            directory->head = node->next;
            directory->head->prev = NULL;
        }
    } else {
        // Case #2: We are removing a node in between two other nodes.
        if(node->next != NULL && node->prev != NULL) {
            node->prev->next = node->next;
            node->next->prev = node->prev;
        } else {
            // Case #3: We are removing the tail.
            node->prev->next = NULL;
            node->prev = NULL;
        }
    }

    (directory->length)--;
}

// static void print_all_handles() {
//     DIR_NODE *node = directory->head;
//
//     while(node != NULL) {
//         debug("print_all_handles(): %s", node->info->handle);
//         node = node->next;
//     }
// }
