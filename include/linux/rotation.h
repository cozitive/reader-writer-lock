#include <linux/list.h>

#define ROT_READ 0
#define ROT_WRITE 1

struct lock_info {
    long id; // ID of lock
    pid_t pid; // Process ID of process that owns the lock
    int low; // low degree of range
    int high; // high degree of range
    int type; // ROT_READ or ROT_WRITE
    struct list_head list;
};

struct reader_writer_lock {
    int active_readers; // Number of active readers
    int active_writers; // Number of active writers
    int waiting_writers; // Number of waiting writers
};