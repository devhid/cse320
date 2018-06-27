#include "thread_counter.h"
#include "csapp.h"
#include "debug.h"

struct thread_counter {
    uint32_t value;
    sem_t sem;
    pthread_mutex_t mutex;
};

THREAD_COUNTER *tcnt_init() {
    THREAD_COUNTER *tc = calloc(sizeof(struct thread_counter), sizeof(char));

    if(pthread_mutex_init(&tc->mutex, NULL) < 0) {
        debug("tcnt_init(): pthread_mutex_init() returned -1.");
        free(tc);
        return NULL;
    }

    if(sem_init(&tc->sem, 0, 1) < 0) {
        debug("tcnt_init(): sem_init() returned -1.");
        free(tc);
        return NULL;
    }

    debug("(Thread Counter) Initializing...");

    return tc;
}

// Didn't error check here because the thread counter is going to terminate anyway.
void tcnt_fini(THREAD_COUNTER *tc) {
    pthread_mutex_unlock(&tc->mutex);
    pthread_mutex_destroy(&tc->mutex);

    sem_destroy(&tc->sem);

    debug("(Thread Counter) Terminating...");

    free(tc);
}

void tcnt_incr(THREAD_COUNTER *tc) {
    // If we aren't waiting for a thread to terminate, then update the value.
    if(pthread_mutex_lock(&tc->mutex) < 0) {
        debug("tcnt_incr(): pthread_mutex_lock() returned -1.");
        return;
    }

    tc->value += 1;
    debug("(Thread Counter) Inc: %i -> %i", tc->value - 1, tc->value);

    if(pthread_mutex_unlock(&tc->mutex) < 0) {
        debug("tcnt_incr(): pthread_mutex_unlock() returned -1.");
        return;
    }
}

void tcnt_decr(THREAD_COUNTER *tc) {
    if(pthread_mutex_lock(&tc->mutex) < 0) {
        debug("tcnt_decr(): pthread_mutex_lock() returned -1.");
        return;
    }

    tc->value -= 1;
    debug("(Thread Counter) Dec: %i -> %i", tc->value + 1, tc->value);

    if(pthread_mutex_unlock(&tc->mutex) < 0) {
        debug("tcnt_decr(): pthread_mutex_unlock() returned -1.");
        return;
    }

    if(tc->value == 0) {
        if(sem_post(&tc->sem) < 0) {
            debug("tcnt_decr(): sem_post() returned -1.");
            return;
        }
    }
}

void tcnt_wait_for_zero(THREAD_COUNTER *tc) {
    if(sem_wait(&tc->sem) < 0) {
        debug("tcnt_wait_for_zero(): sem_wait() returned -1.");
        return;
    }
}
