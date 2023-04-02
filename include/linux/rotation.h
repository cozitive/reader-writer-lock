#include <linux/list.h>
#include <linux/wait.h>

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
	wait_queue_entry_t wait_entry;
};

struct rotation_lock {
    long id; // ID of lock
    pid_t pid; // Process ID of process that owns the lock
    int low; // low degree of range
    int high; // high degree of range
    int lock_type; // ROT_READ or ROT_WRITE
    struct list_head list;
};

/* degree_to_locks: 효율성을 위해서 도입했으나 일단 지금은 신경쓰기 귀찮으므로, 1차로 구현 끝내고 나서 다시 넣기 */
// struct degree_to_locks_entry {
//     struct rotation_lock *lock;
//     struct list_head list;
// };