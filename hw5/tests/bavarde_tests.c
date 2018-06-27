#include <criterion/criterion.h>
#include "thread_counter.h"
#include "semaphore.h"
#include "pthread.h"
#include "stdint.h"
#include "csapp.h"

/*struct thread_counter {
    uint32_t value;
    uint32_t flag;
    sem_t mutex;
    pthread_mutex_t lock;
};

void *test_tc_incr(void *thread_counter) {
    THREAD_COUNTER *tc = (THREAD_COUNTER *) thread_counter;

    tcnt_incr(tc);

    return NULL;
}

void *test_tc_decr(void *thread_counter) {
    THREAD_COUNTER *tc = (THREAD_COUNTER *) thread_counter;

    tcnt_decr(tc);

    return NULL;
}*/

/* Thread Counter Tests */

// Uses one thread to increment and decrement the counter.
/*Test(tc_suite, race_condition_check) {
    THREAD_COUNTER *tc = tcnt_init();
    for(int i = 0; i < 10000; i++) {
        tcnt_incr(tc);
        tcnt_decr(tc);
    }

    cr_assert_eq(tc->value, 0);
}*/

/* Will spawn multiple worker threads to increment and decrement the thread counter. */

/*Test(tc_suite, race_condition_check2) {
    pthread_t tid;
    void *tc = tcnt_init();

    tcnt_wait_for_zero(tc);

    for(int i = 0; i < 10; i++) {
        Pthread_create(&tid, NULL, test_tc_incr, tc);
        Pthread_join(tid, NULL);
    }

    cr_assert_eq( ((THREAD_COUNTER *) tc)->value, 0);
}*/
