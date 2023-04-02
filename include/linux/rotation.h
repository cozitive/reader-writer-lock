#include <linux/list.h>

/*
 * Readers and writers.
 *
 */
#define ROT_READ 0
#define ROT_WRITE 1

struct access_request {
	pid_t pid; // Process ID of requesting process
	int low; // low degree of range
	int high; // high degree of range
	int lock_type; // ROT_READ or ROT_WRITE
	struct list_head list;
};

struct rotation_lock {
    long id; // ID of lock
    pid_t pid; // Process ID of process that owns the lock
    int low; // low degree of range
    int high; // high degree of range
    int lock_type; // ROT_READ or ROT_WRITE
    struct list_head list;
};