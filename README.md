# syncpoint
Library for syncing threads

Note that the library is not all implemented:
- syncpoint configuration cleanup
- multi process support

# Usage

You can use this library when you need to know when a group of worker threads have all reached a sync point location. Each worker thread should have the same number of sync points.

First, initialize a syncpoint configuration object as in:
```
    #define NB_WORKERS (10)
    syncpoint_cfg_t* p_syncpoint = syncpoint_init(NB_WORKERS);
```

At the given location where you want a worker to stop, call: `syncpoint_wait`
```
    bool rc = syncpoint_wait(p_syncpoint);
    if(!rc) {
        <error handling>
    }
```

In any number of non worker threads, you can wait for all workers to be stopped if you need to do some processing:
```
    bool rc = syncpoint_wait_all_stopped(p_syncpoint);
    if(!rc) {
        <error handling>
    }
```

Only one controlling thread should unblock the workers:
```
	bool rc = syncpoint_cont(p_syncpoint);
	if(!rc) {
		printf("ERROR: Failed to unblock workers\n");
	}
```

See the unittest for a working example.
