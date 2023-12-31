#include <linux/list.h>

#define ROT_READ 0
#define ROT_WRITE 1

struct lock_info {
	long id; // Unique ID of lock
	pid_t pid; // Process ID owning the lock
	int low; // The beginning of degree range
	int high; // The end of degree range
	int type; // ROT_READ or ROT_WRITE
	struct list_head list; // Pointer to list node
};

struct reader_writer_lock {
	int active_readers; // Number of active readers
	int active_writers; // Number of active writers
	int waiting_writers; // Number of waiting writers
};