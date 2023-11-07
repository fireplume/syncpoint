#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "syncpoint.h"

#define NB_THREADS (15)
#define NB_SYNC_WAIT (15)

typedef struct {
    uint32_t manager_delay;
    uint32_t worker_delay;
    uint32_t poker_delay;
    syncpoint_cfg_t* p_syncpoint;
    volatile uint32_t poker_wait_calls;
    volatile intptr_t worker_expected_value;
} test_data_t;

static void*
worker(void* p_arg) {
    intptr_t ret = -1;

    test_data_t* p_data = p_arg;
    syncpoint_cfg_t* p_syncpoint = p_data->p_syncpoint;

    int oldret = 0;
    for(int i=0; i<NB_SYNC_WAIT; i++) {
        if(oldret && (oldret != p_data->worker_expected_value)) {
            printf("CORRUPTION!\n");
            abort();
        }

        // Emulate some workload related delay
        if(p_data->worker_delay) {
            usleep(rand() % p_data->worker_delay);
        }

        bool rc = syncpoint_wait(p_syncpoint);
        if(!rc) {
            abort();
        }
        oldret = p_data->worker_expected_value;
    }
    ret = oldret;

    return (void*)ret;
}

/*
 * 'poker' thread function used to show multiple clients waiting for all threads to be stopped.
 * The other client is in 'check_syncpoints's main thread.
 */
static void*
poker(void* p_arg) {
    test_data_t* p_data = p_arg;
    syncpoint_cfg_t* p_syncpoint = p_data->p_syncpoint;
    int rc;

    pthread_detach(pthread_self());

    while(1) {
        rc = syncpoint_wait_all_stopped(p_syncpoint);
        if(!rc) {
            printf("ERROR: Failed to wait for all workers\n");
        }
        p_data->poker_wait_calls++;
        if(p_data->poker_delay) {
            usleep(rand() % p_data->poker_delay);
        }
    }

    return NULL;
}

static void
test_syncpoints(test_data_t* const p_data) {
    syncpoint_cfg_t* p_syncpoint = p_data->p_syncpoint;
    bool rc;

    printf("Wait all stopped\n");
    rc = syncpoint_wait_all_stopped(p_syncpoint);
    if(!rc) {
        printf("ERROR: Failed to wait for all workers\n");
    }
    printf("Waiting workers: %d\n", syncpoint_wait_count(p_syncpoint));

    // update worker return value
    p_data->worker_expected_value = 10;

    for(int i=0; i<NB_SYNC_WAIT; i++) {
        printf("continue[%d]\n", i);
        if(i%2 == 0) {
            rc = syncpoint_wait_all_stopped(p_syncpoint);
            if(!rc) {
                printf("ERROR: Failed to unblock workers\n");
            }

            // Do something while all workers are blocked
            p_data->worker_expected_value++;
        }

        // Emulate some processing delay
        if(p_data->manager_delay) {
            usleep(rand() % p_data->manager_delay);
        }

        // Let the workers continue
        rc = syncpoint_cont(p_syncpoint);
        if(!rc) {
            printf("ERROR: Failed to unblock workers\n");
        }
    }
}

static void
unittest(test_data_t* const p_data) {
    pthread_t workers_tid[NB_THREADS];
    pthread_t poker_tid;
    int rc;

    printf("-------------------------------------------------------------------------\n");
    printf("CASE: Max delays(us) manager: %"PRIu32" worker: %"PRIu32" poker: %"PRIu32"\n",
           p_data->manager_delay,
           p_data->worker_delay,
           p_data->poker_delay);

    syncpoint_cfg_t* p_syncpoint = syncpoint_init(NB_THREADS);
    if(!p_syncpoint) {
        printf("ERROR: syncpoint_init failed!\n");
        return;
    } else {
        printf("syncpoint_init done\n");
    }
    p_data->p_syncpoint = p_syncpoint;

    time_t seed = time(NULL);
    srand(seed);
    printf("SEED: %"PRIu64"\n", seed);

    for(int i=0; i<NB_THREADS; i++) {
        rc = pthread_create(&workers_tid[i], NULL, worker, p_data);
        assert(rc == 0);
    }

    rc = pthread_create(&poker_tid, NULL, poker, p_data);
    assert(rc == 0);

    test_syncpoints(p_data);

    intptr_t status;
    unsigned failures = 0;
    for(int i=0; i<NB_THREADS; i++) {
        rc = pthread_join(workers_tid[i], (void**)&status);
        assert(rc == 0);
        if(status != p_data->worker_expected_value) {
            failures++;
            printf("ERROR: tid[%lu] returned %"PRId64"\n", workers_tid[i], status);
        }
    }

    printf("NOTE: Poker nb wait calls: %"PRIu32" / %"PRIu32" cycles\n", p_data->poker_wait_calls, NB_SYNC_WAIT);

    if(failures) {
        printf("FAIL\n");
    } else {
        printf("PASS\n");
    }
}


int
main(int argc, char* argv[]) {
    test_data_t test_delays[] = {
        {
            .manager_delay = 50E3,
            .worker_delay = 500E3,
            .poker_delay = 250E3
        },
        {
            .manager_delay = 50E3,
            .worker_delay = 250E3,
            .poker_delay = 125E3
        },
        {
            .manager_delay = 0,
            .worker_delay = 0,
            .poker_delay = 0
        },
    };

    for(int i=0; i<sizeof(test_delays)/sizeof(test_delays[0]); i++) {
        unittest(&test_delays[i]);
    }

    return 0;
}
