#include <linux/rotation.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define MAX_DEGREE 360

/* Primitives */
static int device_orientation = 0; // Orientation degree of the device
static int locks_initialized = 0; // Whether the locks have been initialized (bool)
static long next_lock_id = 0; // The next lock ID to use

/* Data structures */
static LIST_HEAD(locks_info); // Information of currently held locks
static struct reader_writer_lock locks[MAX_DEGREE]; // Numbers of readers/writers for each degree
static DECLARE_WAIT_QUEUE_HEAD(requests); // Queue of requests

/* Mutexes */
static DEFINE_MUTEX(orientation_mutex); // Protect `orientation`
static DEFINE_MUTEX(locks_mutex); // Protect `locks_info` and `locks`
static DEFINE_MUTEX(next_lock_id_mutex); // Protect `next_lock_id`

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

/// @brief Unlock `lock` from `locks`.
/// @param lock The lock to be unlocked.
void decrement_active_count(struct lock_info *lock);

/// @brief Set the current device orientation.
/// @param degree The degree to set as the current device orientation. (0 <= degree < MAX_DEGREE)
/// @return 0 on success, -EINVAL on invalid argument.
SYSCALL_DEFINE1(set_orientation, int, degree)
{
	/* Error handling: invalid argument */
	if (degree < 0 || degree >= MAX_DEGREE) {
		return -EINVAL;
	}

	/* Set device orientation */
	mutex_lock(&orientation_mutex);
	device_orientation = degree;
	mutex_unlock(&orientation_mutex);

	/* Wake up all processes waiting for the lock */
	wake_up_all(&requests);

	return 0;
}

/// @brief Claim read or write access in the specified degree range.
/// @param low The beginning of the degree range (inclusive). (0 <= low < MAX_DEGREE)
/// @param high The end of the degree range (inclusive). (0 <= high < MAX_DEGREE)
/// @param type The type of the access claimed. (ROT_READ or ROT_WRITE)
/// @return Non-negative unique lock ID on success, -EINVAL on invalid argument.
SYSCALL_DEFINE3(rotation_lock, int, low, int, high, int, type)
{
	int i; // Degree iteration variable
	struct lock_info *lock; // Lock info to be added
	int is_current_waiting_writer = 0; // Whether current task is in `waiting_writers`
	DEFINE_WAIT(wait); // Wait for current task

	/* Error handling: invalid argument */
	if (low < 0 || low >= MAX_DEGREE || high < 0 || high >= MAX_DEGREE ||
		(type != ROT_READ && type != ROT_WRITE)) {
		return -EINVAL;
	}

	/* Make sure the locks are initialized */
	locks_init();

	/* Create a new lock */
	lock = kmalloc(sizeof(struct lock_info), GFP_KERNEL);
	mutex_lock(&next_lock_id_mutex);
	lock->id = next_lock_id++;
	mutex_unlock(&next_lock_id_mutex);
	lock->pid = current->pid;
	lock->low = low;
	lock->high = high;
	lock->type = type;
	INIT_LIST_HEAD(&lock->list);

	/* Add current task to wait queue */
	add_wait_queue(&requests, &wait);

	/* Wait until the access meets readers-writer lock condition */
	mutex_lock(&orientation_mutex);
	mutex_lock(&locks_mutex);
	while (!lock_available(low, high, type)) {
		mutex_unlock(&orientation_mutex);
		mutex_unlock(&locks_mutex);

		/* Change current task's state to `TASK_INTERRUPTIBLE` */
		prepare_to_wait(&requests, &wait, TASK_INTERRUPTIBLE);

		/* Increment the waiting_writers count */
		if (type == ROT_WRITE && !is_current_waiting_writer) {
			mutex_lock(&locks_mutex);
			for (i = low; i <= high; i++) {
				if (i == MAX_DEGREE) {
					i = 0;
				}
				locks[i].waiting_writers++;
			}
			is_current_waiting_writer = 1;
			mutex_unlock(&locks_mutex);
		}

		/* Go to sleep */
		schedule();

		/* After wakeup, relock for condition check */
		mutex_lock(&orientation_mutex);
		mutex_lock(&locks_mutex);
	}

	/* Delete current task in wait queue */
	finish_wait(&requests, &wait);

	/* Add the lock info to holding lock list */
	list_add(&lock->list, &locks_info);

	/* Decrement the waiting_writers count */
	if (is_current_waiting_writer) {
		for (i = low; i <= high; i++) {
			if (i == MAX_DEGREE) {
				i = 0;
			}
			locks[i].waiting_writers--;
		}
		is_current_waiting_writer = 0;
	}

	/* Increment the active_readers or active_writers count */
	for (i = low; i <= high; i++) {
		if (i == MAX_DEGREE) {
			i = 0;
		}
		if (type == ROT_READ) {
			locks[i].active_readers++;
		} else {
			locks[i].active_writers++;
		}
	}

	mutex_unlock(&orientation_mutex);
	mutex_unlock(&locks_mutex);

	return lock->id;
}

/// @brief Revoke access claimed by a call to rotation_lock.
/// @param id The ID associated with the access to revoke.
/// @return On success, returns 0. On invalid argument, returns -EINVAL. On permission error, returns -EPERM.
SYSCALL_DEFINE1(rotation_unlock, long, id)
{
	struct lock_info *lock; // Lock info to be deleted

	/* Error handling: negative lock ID */
	if (id < 0) {
		return -EINVAL;
	}

	mutex_lock(&locks_mutex);

	/* Find the corresponding lock */
	lock = find_lock(id);

	/* Error handling: invalid lock ID */
	if (lock == NULL) {
		mutex_unlock(&locks_mutex);
		return -EINVAL;
	}

	/* Error handling: lock belongs to other task */
	if (lock->pid != current->pid) {
		mutex_unlock(&locks_mutex);
		return -EPERM;
	}

	/* Delete the lock from holding lock list */
	list_del(&lock->list);

	/* Decrement the active_readers or active_writers count */
	decrement_active_count(lock);

	mutex_unlock(&locks_mutex);

	/* Free memory AFTER RELEASING MUTEX */
	kfree(lock);

	/* Wake up all processes waiting for the lock */
	wake_up_all(&requests);

	return 0;
}

void locks_init(void)
{
	int i; // Degree iteration variable

	// Initalize `locks` only once
	if (locks_initialized) {
		return;
	}
	for (i = 0; i < MAX_DEGREE; i++) {
		locks[i].active_readers = 0;
		locks[i].active_writers = 0;
		locks[i].waiting_writers = 0;
	}
	locks_initialized = 1;
}

int orientation_in_range(int low, int high)
{
	if (low <= high) {
		return device_orientation >= low && device_orientation <= high;
	} else {
		return device_orientation >= low || device_orientation <= high;
	}
}

int lock_available(int low, int high, int type)
{
	int i; // Degree iteration variable

	if (!orientation_in_range(low, high)) {
		return 0;
	}

	if (type == ROT_READ) {
		/* Reader: wait for active/waiting writers */
		for (i = low; i <= high; i++) {
			if (i == MAX_DEGREE) {
				i = 0;
			}
			if (locks[i].active_writers > 0 ||
				locks[i].waiting_writers > 0) {
				return 0;
			}
		}
		return 1;
	} else {
		/* Writer: wait for active readers/writers */
		for (i = low; i <= high; i++) {
			if (i == MAX_DEGREE) {
				i = 0;
			}
			if (locks[i].active_readers > 0 ||
				locks[i].active_writers > 0) {
				return 0;
			}
		}
		return 1;
	}
}

struct lock_info *find_lock(long id)
{
	/* Find corresponding lock in holding lock list */
	struct lock_info *lock;
	list_for_each_entry(lock, &locks_info, list) {
		if (lock->id == id) {
			return lock;
		}
	}
	return NULL;
}

void exit_rotlock(struct task_struct *tsk)
{
	struct lock_info *lock;// Lock list iteration variable
	struct lock_info *before = NULL; // Lock list iteration variable
	LIST_HEAD(garbage); // List of locks to be freed
	struct list_head *node; // Garbage collection variable
	struct lock_info *element; // Garbage collection variable

	mutex_lock(&locks_mutex);

	/* unlock all locks belonging to exiting thread */
	list_for_each_entry(lock, &locks_info, list) {
		if (lock->pid == tsk->pid) {
			if (before != NULL) {
				list_del(&before->list);
				list_add(&before->list, &garbage);
			}
			decrement_active_count(lock);
			before = lock;
		}
	}
	if (before != NULL) {
		list_del(&before->list);
		list_add(&before->list, &garbage);
	}

	mutex_unlock(&locks_mutex);

	/* Wake up all processes waiting for the lock */
	wake_up_all(&requests);

	/* Clean up garbage */
	while (!list_empty(&garbage)) {
		node = garbage.next;
		element = list_entry(node, struct lock_info, list);
		list_del(node);
		if (element != NULL) {
			kfree(element);
		}
	}
}

void decrement_active_count(struct lock_info *lock) {
	int i; // Degree iteration variable

	/* Decrement the active_readers or active_writers count */
	for (i = lock->low; i <= lock->high; i++) {
		if (i == MAX_DEGREE) {
			i = 0;
		}
		if (lock->type == ROT_READ) {
			locks[i].active_readers--;
		} else {
			locks[i].active_writers--;
		}
	}
}