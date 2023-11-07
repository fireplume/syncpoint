/*
 * Library for syncing threads
 *
 * Author:  Mathieu Comeau
 * Date:    2023/11/26
 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "syncpoint.h"

typedef struct cond_sync_t {
    pthread_mutex_t m;
    pthread_cond_t c;
} cond_sync_t;

typedef struct stop_sync_t {
    cond_sync_t sync;
    volatile uint32_t nb_workers_ingress;
    volatile uint32_t nb_workers_egress;
    volatile bool ingress_completed;
} stop_sync_t;

struct syncpoint_cfg_t {
    uint32_t nb_workers;
    pthread_barrier_t continue_done;

    // Workers sync'ing
    stop_sync_t worker_wait;
    cond_sync_t client_worker_wait;
    pthread_barrier_t egress_sync;
};

#define PRINT_DEBUG_EN 0
#define PRINT_DBG_F(fmt, ...) //
#define PRINT_DBG(str) //

#if PRINT_DEBUG_EN == 1
    #undef PRINT_DBG_F
    #undef PRINT_DBG

    #define PRINT_DBG_F(fmt, ...)  do { fprintf(stderr, "l:%4d ", __LINE__); fprintf(stderr, "DEBUG: "fmt"\n", __VA_ARGS__); } while(0)
    #define PRINT_DBG(str) do { fprintf(stderr, "l:%4d ", __LINE__); fprintf(stderr, "DEBUG: "str"\n"); } while(0)
#endif

#define PRINT_ERR_F(fmt, ...) do { fprintf(stderr, "l:%4d ", __LINE__); fprintf(stderr, "ERROR: "fmt"\n", __VA_ARGS__); } while(0)
#define PRINT_ERR(str) do { fprintf(stderr, "l:%4d ", __LINE__); fprintf(stderr, "ERROR: "str"\n"); } while(0)


static void
_init_cond(cond_sync_t* const p_cond) {
    pthread_mutex_init(&p_cond->m, NULL);
    pthread_cond_init(&p_cond->c, NULL);
}

syncpoint_cfg_t*
syncpoint_init(const uint32_t nb_workers) {
    syncpoint_cfg_t* p_config = malloc(sizeof(*p_config));
    if(p_config == NULL) {
        return NULL;
    }

    p_config->nb_workers = nb_workers;
    pthread_barrier_init(&p_config->continue_done, NULL, 2);

    p_config->worker_wait.nb_workers_ingress = 0;
    p_config->worker_wait.ingress_completed = false;
    p_config->worker_wait.nb_workers_egress = 0;
    _init_cond(&p_config->worker_wait.sync);
    _init_cond(&p_config->client_worker_wait);

    pthread_barrier_init(&p_config->egress_sync, NULL, nb_workers);

    return p_config;
}

/*
 * Let the workers continue (blocking call)
 *
 * Waits for all workers to be stopped, then waits for all workers
 * to be unblocked.
 */
bool
syncpoint_cont(syncpoint_cfg_t* const p_config) {
    PRINT_DBG("syncpoint_cont");
    int rc;
    bool rcb;

    rcb = syncpoint_wait_all_stopped(p_config);
    if(!rcb) {
        PRINT_ERR("syncpoint_wait_all_stopped failed");
        return rcb;
    }

    rc = pthread_mutex_lock(&p_config->worker_wait.sync.m);
    if(rc != 0) {
        PRINT_ERR_F("pthread_mutex_lock failed: %d(%s)", rc, strerror(rc));
        return false;
    }

    p_config->worker_wait.nb_workers_ingress = 0;
    p_config->worker_wait.ingress_completed = false;
    PRINT_DBG("pthread_cond_broadcast!");
    rc = pthread_cond_broadcast(&p_config->worker_wait.sync.c);
    if(rc != 0) {
        PRINT_ERR_F("pthread_cond_broadcast failed: %d(%s)", rc, strerror(rc));
        return false;
    }

    rc = pthread_mutex_unlock(&p_config->worker_wait.sync.m);
    if(rc != 0) {
        PRINT_ERR_F("pthread_mutex_unlock failed: %d(%s)", rc, strerror(rc));
        return false;
    }

    // Wait for last worker to signal us we have completed a cycle
    PRINT_DBG("syncpoint_cont: wait last worker to complete cycle");
    rc = pthread_barrier_wait(&p_config->continue_done);
    if(rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
        PRINT_ERR_F("pthread_barrier_wait failed: %d(%s)", rc, strerror(rc));
        return false;
    }

    p_config->worker_wait.nb_workers_egress = 0;

    return true;
}

bool
syncpoint_wait_all_stopped(syncpoint_cfg_t* const p_config) {
    PRINT_DBG("syncpoint_wait_all_stopped: BEGIN");
    int rc;

    rc = pthread_mutex_lock(&p_config->client_worker_wait.m);
    if(rc != 0) {
        PRINT_ERR_F("pthread_mutex_lock failed: %d(%s)", rc, strerror(rc));
        return false;
    }

    while(!p_config->worker_wait.ingress_completed) {
        rc = pthread_cond_wait(&p_config->client_worker_wait.c, &p_config->client_worker_wait.m);
        if(rc != 0) {
            PRINT_ERR_F("pthread_cond_wait failed: %d(%s)", rc, strerror(rc));
            return false;
        }
    }

    rc = pthread_mutex_unlock(&p_config->client_worker_wait.m);
    if(rc != 0) {
        PRINT_ERR_F("pthread_mutex_unlock failed: %d(%s)", rc, strerror(rc));
        return false;
    }

    PRINT_DBG("syncpoint_wait_all_stopped: END");

    return true;
}

uint32_t
syncpoint_wait_count(const syncpoint_cfg_t* const p_config) {
    return p_config->worker_wait.nb_workers_ingress;
}

/*
 * Client sync function
 */

bool
syncpoint_wait(syncpoint_cfg_t* const p_config) {
    PRINT_DBG_F("syncpoint_wait[tid=%"PRIu64"]", pthread_self());
    int rc;

    rc = pthread_mutex_lock(&p_config->worker_wait.sync.m);
    if(rc != 0) {
        PRINT_ERR_F("pthread_mutex_lock failed: %d(%s)", rc, strerror(rc));
        return false;
    }

    p_config->worker_wait.nb_workers_ingress++;
    if(p_config->worker_wait.nb_workers_ingress == p_config->nb_workers) {
        p_config->worker_wait.ingress_completed = true;
        rc = pthread_cond_broadcast(&p_config->client_worker_wait.c);
        if(rc != 0) {
            PRINT_ERR_F("pthread_cond_broadcast failed: %d(%s)", rc, strerror(rc));
            return false;
        }
    }
    PRINT_DBG_F("syncpoint_wait: %d blocked!", p_config->worker_wait.nb_workers_ingress);

    while(p_config->worker_wait.nb_workers_ingress) {
        rc = pthread_cond_wait(&p_config->worker_wait.sync.c, &p_config->worker_wait.sync.m);
        if(rc != 0) {
            PRINT_ERR_F("pthread_cond_wait failed: %d(%s)", rc, strerror(rc));
            return false;
        }
    }
    p_config->worker_wait.nb_workers_egress++;
    PRINT_DBG_F("syncpoint_wait: %d/%d unblocked!", p_config->worker_wait.nb_workers_egress, p_config->nb_workers);

    if(p_config->worker_wait.nb_workers_egress == p_config->nb_workers) {
        PRINT_DBG("syncpoint_wait: last worker continue!");
        rc = pthread_barrier_wait(&p_config->continue_done);
        if(rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
            PRINT_ERR_F("pthread_barrier_wait failed: %d(%s)", rc, strerror(rc));
            return false;
        }
    }

    rc = pthread_mutex_unlock(&p_config->worker_wait.sync.m);
    if(rc != 0) {
        PRINT_ERR_F("pthread_mutex_unlock failed: %d(%s)", rc, strerror(rc));
        return false;
    }

    rc = pthread_barrier_wait(&p_config->egress_sync);
    if(rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
        PRINT_ERR_F("pthread_barrier_wait failed: %d(%s)", rc, strerror(rc));
        return false;
    }

    return true;
}

