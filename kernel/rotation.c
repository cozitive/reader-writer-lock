#include <linux/rotation.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define MAX_DEGREE 360

static int device_orientation = 0;
static DEFINE_MUTEX(orientation_lock);

static LIST_HEAD(locks_info); // Information of currently held locks
static struct reader_writer_lock locks_counts[MAX_DEGREE];
static DEFINE_MUTEX(locks_info_lock); // Mutex to protect `locks_info` and `locks_counts`
static int is_locks_counts_initialized = 0; // Whether the locks have been initialized

static DECLARE_WAIT_QUEUE_HEAD(requests); // Queue of requests

static long next_lock_id = 0; // The next lock ID to use

/// @brief Initialize `locks` if they haven't yet.
void locks_init(void);

/// @brief Check if current device orientation is in the specified degree range.
/// @param low
/// @param high
/// @return 1 if yes, 0 if no.
int orientation_in_range(int low, int high);

/// @brief Check if conditions meet to acquire a lock.
/// @return 1 if yes, 0 if no.
int lock_available(int low, int high, int type);

/// @brief Find a lock by ID.
/// @param id The ID to search for.
/// @return The lock, or NULL if not found.
struct lock_info *find_lock(long id);

/// @brief Set the current device orientation.
/// @param degree The degree to set as the current device orientation. Value must be in the range 0 <= degree < 360.
/// @return Zero on success, -EINVAL on invalid argument.
SYSCALL_DEFINE1(set_orientation, int, degree) {
	if (degree < 0 || degree >= MAX_DEGREE) {
		return -EINVAL;
	}

	mutex_lock(&orientation_lock);
	device_orientation = degree;
	mutex_unlock(&orientation_lock);

	return 0;
}

/// @brief Claim read or write access in the specified degree range.
/// @param low The beginning of the degree range (inclusive). Value must be in the range 0 <= low < 360.
/// @param high The end of the degree range (inclusive). Value must be in the range 0 <= high < 360.
/// @param type The type of the access claimed. Value must be either ROT_READ or ROT_WRITE.
/// @return On success, returns a non-negative lock ID that is unique for each call to rotation_lock. On invalid argument, returns -EINVAL.
SYSCALL_DEFINE3(rotation_lock, int, low, int, high, int, type) {
	int i;

	/* Make sure the locks are initialized */
	locks_init();

	if (low < 0 || low >= 360 || high < 0 || high >= 360 ||
	    (type != ROT_READ && type != ROT_WRITE))
		return -EINVAL;

	/* Create a new lock */
	struct lock_info *lock = kmalloc(sizeof(struct lock_info), GFP_KERNEL);
	lock->id = next_lock_id++;
	lock->pid = current->pid;
	lock->low = low;
	lock->high = high;
	lock->type = type;

	DEFINE_WAIT(wait);
	add_wait_queue(&requests, &wait);
	int writer_waiting = 0; // Flag to keep track of whether `waiting_writers` is containing current process

	mutex_lock(&orientation_lock);
	mutex_lock(&locks_info_lock);
	while (!lock_available(low, high, type)) {
		mutex_unlock(&orientation_lock);
		mutex_unlock(&locks_info_lock);
		prepare_to_wait(&requests, &wait, TASK_INTERRUPTIBLE);

		/* Increment the waiting_writers count */
		if (type == ROT_WRITE && !writer_waiting) {
			mutex_lock(&locks_info_lock);
			for (i = low; i <= high; i++) {
				locks_counts[i].waiting_writers++;
			}
			writer_waiting = 1;
			mutex_unlock(&locks_info_lock);
		}
		schedule();
	}
	finish_wait(&requests, &wait);
	mutex_unlock(&orientation_lock);

	/* Add the lock to the list */
	list_add(&lock->list, &locks_info);

	/* Decrement the waiting_writers count */
	if (writer_waiting) {
		mutex_lock(&locks_info_lock);
		for (i = low; i <= high; i++) {
			locks_counts[i].waiting_writers--;
		}
		writer_waiting = 0;
		mutex_unlock(&locks_info_lock);
	}

	/* Increment the active_xxx count */
	for (i = low; i <= high; i++) {
		if (type == ROT_READ) {
			locks_counts[i].active_readers++;
		} else {
			locks_counts[i].active_writers++;
		}
	}

	mutex_unlock(&locks_info_lock);

	return lock->id;
}

/// @brief Revoke access claimed by a call to rotation_lock.
/// @param id The ID associated with the access to revoke.
/// @return On success, returns 0. On invalid argument, returns -EINVAL. On permission error, returns -EPERM.
SYSCALL_DEFINE1(rotation_unlock, long, id) {
	int i;

	/* Negative id, return -EINVAL */
	if (id < 0)
		return -EINVAL;

	struct lock_info *lock = find_lock(id); // TODO: find_lock() 구현

	/* No such lock, return -EINVAL */
	if (lock == NULL)
		return -EINVAL;

	/* Process is not the owner of the lock, return -EPERM */
	if (lock->pid != current->pid)
		return -EPERM;

	/* Delete the lock from list */
	mutex_lock(&locks_info_lock);
	list_del(&lock->list);
	for (i = lock->low; i <= lock->high; i++) {
		if (lock->type == ROT_READ) {
			locks_counts[i].active_readers--;
		} else {
			locks_counts[i].active_writers--;
		}
	}
	mutex_unlock(&locks_info_lock);

	kfree(lock);

	/* Wake up all processes waiting for the lock */
	wake_up_all(&requests);

	return 0;
}

void locks_init(void) {
	int i;
	if (is_locks_counts_initialized) {
		return;
	}

	for (i = 0; i < 360; i++) {
		locks_counts[i].active_readers = 0;
		locks_counts[i].active_writers = 0;
		locks_counts[i].waiting_writers = 0;
	}
	is_locks_counts_initialized = 1;
}

int orientation_in_range(int low, int high) {
	if (low <= high)
		return device_orientation >= low && device_orientation <= high;
	else
		return device_orientation >= low || device_orientation <= high;
}

int lock_available(int low, int high, int type) {
	int i;

	if (!orientation_in_range(low, high))
		return 0;

	if (type == ROT_READ) {
		/* Reader */
		for (i = low; i <= high; i++) {
			if (locks_counts[i].active_writers > 0 ||
			    locks_counts[i].waiting_writers > 0) {
				return 0;
			}
		}
		return 1;
	} else {
		/* Writer */

		int flag = 1; // Whether the lock is available
		for (i = low; i <= high; i++) {
			if (locks_counts[i].active_readers > 0 ||
			    locks_counts[i].active_writers > 0) {
				return 0;
			}
		}
		return 1;
	}
}

struct lock_info *find_lock(long id) {

}