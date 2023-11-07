/*
 * Library for syncing threads
 *
 * Author:  Mathieu Comeau
 * Date:    2023/11/26
 */

#ifndef SYNCPOINT_H
#define SYNCPOINT_H

#include <stdint.h>
#include <stdbool.h>

// Opaque client pointer to syncpoint configuration
typedef struct syncpoint_cfg_t syncpoint_cfg_t;

/*
 * Worker side function
 */
extern bool
syncpoint_wait(syncpoint_cfg_t* const p_config);

/*
 * Manager side functions
 */
extern syncpoint_cfg_t*
syncpoint_init(const uint32_t nb_workers);


extern uint32_t
syncpoint_wait_count(const syncpoint_cfg_t* const p_config);

/*
 * Wait for all workers to be stopped and then let them continue
 * Blocking call
 */
extern bool
syncpoint_cont(syncpoint_cfg_t* const p_config);

/*
 * Wait for all workers to be stopped
 * Blocking call
 */
extern bool
syncpoint_wait_all_stopped(syncpoint_cfg_t* const p_config);

#endif
