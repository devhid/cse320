#include "mailbox.h"
#include "csapp.h"
#include "debug.h"

typedef struct mailbox_node MB_NODE;

struct mailbox {
    MB_NODE *head;

    pthread_mutex_t mutex;
    sem_t sem;

    char *handle;

    uint32_t ref_count;
    uint32_t length;

    uint8_t defunct;
    MAILBOX_DISCARD_HOOK *hook;
};

struct mailbox_node {
    MAILBOX_ENTRY *entry;
    MB_NODE *next;
};

static void insert_into_mailbox(MAILBOX *mailbox, MB_NODE *node);
static MAILBOX_ENTRY *remove_from_mailbox(MAILBOX *mailbox);

void discard(MAILBOX_ENTRY *entry) {
    if(entry->type == MESSAGE_ENTRY_TYPE) {
        MAILBOX *from = entry->content.message.from;

        if(from != NULL) {
            mb_ref(from);
            mb_unref(entry->content.message.from);

            char *body = calloc(sizeof(char *), sizeof(char));
            char *notice = "Your message could not be delivered.";

            strncpy(body, notice, strlen(notice));
            debug("discard(): %s", body);

            mb_add_notice(from, NOTICE_ENTRY_TYPE, entry->content.notice.msgid, (void *) body, strlen(body));
        }
    }
}


MAILBOX *mb_init(char *handle) {
    debug("--- mb_init() ---");

    MAILBOX *mailbox = calloc(sizeof(struct mailbox), sizeof(char));
    mailbox->ref_count = 1;
    mailbox->defunct = 0;
    mailbox->length = 0;

    mailbox->handle = calloc(sizeof(handle), sizeof(char));
    strncpy(mailbox->handle, handle, strlen(handle));

    if(pthread_mutex_init(&mailbox->mutex, NULL) < 0) {
        debug("mb_init(): pthread_mutex_init() returned -1.");
        return NULL;
    }

    if(sem_init(&mailbox->sem, 0, 0) < 0) {
        debug("mb_init(): sem_init() returned -1.");
        return NULL;
    }

    return mailbox;
}


void mb_set_discard_hook(MAILBOX *mb, MAILBOX_DISCARD_HOOK *hook) {
    mb->hook = hook;
}


void mb_ref(MAILBOX *mb) {
    if(pthread_mutex_lock(&mb->mutex) < 0) {
        debug("mb_ref(): pthread_mutex_lock() returned -1.");
        return;
    }

    debug("mb_ref(%d->%d): '%s'", mb->ref_count, mb->ref_count + 1, mb->handle);
    (mb->ref_count)++;

    if(pthread_mutex_unlock(&mb->mutex) < 0) {
        debug("mb_ref(): pthread_mutex_unlock() returned -1.");
        return;
    }
}


void mb_unref(MAILBOX *mb) {
    if(pthread_mutex_lock(&mb->mutex) < 0) {
        debug("mb_unref(): pthread_mutex_lock() returned -1.");
        return;
    }

    debug("mb_unref(%d->%d): '%s'", mb->ref_count, mb->ref_count - 1, mb->handle);
    (mb->ref_count)--;

    if(pthread_mutex_unlock(&mb->mutex) < 0) {
        debug("mb_unref(): pthread_mutex_unlock() returned -1.");
        return;
    }

    if(mb->ref_count == 0) {
        free(mb->handle);
        free(mb);
    }
}


void mb_shutdown(MAILBOX *mb) {
    debug("--- mb_shutdown() ---");

    mb->defunct = 1; // Mark the mailbox as 'defunct'.

    sem_post(&mb->sem);
}


char *mb_get_handle(MAILBOX *mb) {
    debug("--- mb_get_handle() ---");
    return mb->handle;
}


void mb_add_message(MAILBOX *mb, int msgid, MAILBOX *from, void *body, int length) {
    debug("--- mb_add_message() ---");

    if(pthread_mutex_lock(&mb->mutex) < 0) {
        debug("mb_add_message(): pthread_mutex_lock() returned -1.");
        return;
    }

    MAILBOX_ENTRY *entry = calloc(sizeof(struct mailbox_entry), sizeof(char));
    entry->type = MESSAGE_ENTRY_TYPE;
    entry->length = length;
    entry->content.message.msgid = msgid;
    entry->content.message.from = from;

    entry->body = calloc(length, sizeof(char));
    memcpy(entry->body, body, length);

    MB_NODE *node = calloc(sizeof(struct mailbox_node), sizeof(char));
    node->entry = entry;

    insert_into_mailbox(mb, node);

    if(pthread_mutex_unlock(&mb->mutex) < 0) {
        debug("mb_add_message(): pthread_mutex_unlock() returned -1.");
        return;
    }

    sem_post(&mb->sem);
}


void mb_add_notice(MAILBOX *mb, NOTICE_TYPE ntype, int msgid, void *body, int length) {
    debug("--- mb_add_notice() ---");

    if(pthread_mutex_lock(&mb->mutex) < 0) {
        debug("mb_add_notice(): pthread_mutex_lock() returned -1.");
        return;
    }

    MAILBOX_ENTRY *entry = calloc(sizeof(struct mailbox_entry), sizeof(char));
    entry->type = NOTICE_ENTRY_TYPE;
    entry->length = length;
    entry->content.notice.msgid = msgid;
    entry->content.notice.type = ntype;

    entry->body = calloc(length, sizeof(char));
    memcpy(entry->body, body, length);

    MB_NODE *node = calloc(sizeof(struct mailbox_node), sizeof(char));
    node->entry = entry;

    insert_into_mailbox(mb, node);

    if(pthread_mutex_unlock(&mb->mutex) < 0) {
        debug("mb_add_notice(): pthread_mutex_unlock() returned -1.");
        return;
    }

    sem_post(&mb->sem);
}

MAILBOX_ENTRY *mb_next_entry(MAILBOX *mb) {
    debug("--- mb_next_entry() ---");

    sem_wait(&mb->sem);

    if(mb->defunct) {
        return NULL;
    }

    if(pthread_mutex_lock(&mb->mutex) < 0) {
        debug("mb_next_entry(): pthread_mutex_lock() returned -1.");
        return NULL;
    }

    MAILBOX_ENTRY *entry = remove_from_mailbox(mb);

    if(pthread_mutex_unlock(&mb->mutex) < 0) {
        debug("mb_next_entry(): pthread_mutex_unlock() returned -1.");
        return NULL;
    }

    return entry;
}

static void insert_into_mailbox(MAILBOX *mailbox, MB_NODE *node) {
    // Case #1: We are inserting the head.
    if(mailbox->length == 0) {
        mailbox->head = node;
    } else {
        MB_NODE *head = mailbox->head;

        // Case #2: Every other node would be added to the end of the queue.
        while(head->next != NULL) {
            head = head->next;
        }

        head->next = node;
    }

    // Increment the directory length.
    (mailbox->length)++;
}

static MAILBOX_ENTRY *remove_from_mailbox(MAILBOX *mailbox) {
    if(mailbox->head == NULL) {
        return NULL;
    }

    MB_NODE *head = mailbox->head;
    MAILBOX_ENTRY *entry = head->entry;

    // We will always be removing the head, since this is a queue.
    if(head->next != NULL) {
        free(mailbox->head);
        mailbox->head = head->next;
    } else {
        free(mailbox->head);
        mailbox->head = NULL;
    }

    (mailbox->length)--;

    return entry;
}
