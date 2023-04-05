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
static DEFINE_MUTEX(orientation_mutex);

static LIST_HEAD(locks_info); // Information of currently held locks
static struct reader_writer_lock locks[MAX_DEGREE]; // Numbers of readers/writers for each degree
static DEFINE_MUTEX(locks_mutex); // Mutex to protect `locks_info` and `locks`
static int locks_initialized = 0; // Whether the locks have been initialized

static DECLARE_WAIT_QUEUE_HEAD(requests); // Queue of requests

static long next_lock_id = 30; // The next lock ID to use
static DEFINE_MUTEX(next_lock_id_mutex); // Mutex to protect `next_lock_id`

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
/// @param degree The degree to set as the current device orientation. Value must be in the range 0 <= degree < MAX_DEGREE.
/// @return Zero on success, -EINVAL on invalid argument.
SYSCALL_DEFINE1(set_orientation, int, degree)
{
	if (degree < 0 || degree >= MAX_DEGREE) {
		return -EINVAL;
	}

	mutex_lock(&orientation_mutex);
	device_orientation = degree;
	mutex_unlock(&orientation_mutex);

	wake_up_all(&requests);

	return 0;
}

/// @brief Claim read or write access in the specified degree range.
/// @param low The beginning of the degree range (inclusive). Value must be in the range 0 <= low < MAX_DEGREE.
/// @param high The end of the degree range (inclusive). Value must be in the range 0 <= high < MAX_DEGREE.
/// @param type The type of the access claimed. Value must be either ROT_READ or ROT_WRITE.
/// @return On success, returns a non-negative lock ID that is unique for each call to rotation_lock. On invalid argument, returns -EINVAL.
SYSCALL_DEFINE3(rotation_lock, int, low, int, high, int, type)
{
	int i;

	/* Return -EINVAL if arguments are invalid */
	if (low < 0 || low >= MAX_DEGREE || high < 0 || high >= MAX_DEGREE ||
	    (type != ROT_READ && type != ROT_WRITE)) {
		return -EINVAL;
	}

	/* Make sure the locks are initialized */
	locks_init();

	/* Create a new lock */
	struct lock_info *lock = kmalloc(sizeof(struct lock_info), GFP_KERNEL);
	mutex_lock(&next_lock_id_mutex);
	lock->id = next_lock_id++;
	mutex_unlock(&next_lock_id_mutex);
	lock->pid = current->pid;
	lock->low = low;
	lock->high = high;
	lock->type = type;
	INIT_LIST_HEAD(&lock->list);

	/* Add current task to wait queue */
	DEFINE_WAIT(wait);
	add_wait_queue(&requests, &wait);
	int writer_waiting = 0; // Flag to keep track of whether `waiting_writers` is containing current process

	/* Wait until the access meets readers-writer lock condition */
	mutex_lock(&orientation_mutex);
	mutex_lock(&locks_mutex);
	while (!lock_available(low, high, type)) {
		mutex_unlock(&orientation_mutex);
		mutex_unlock(&locks_mutex);
		prepare_to_wait(&requests, &wait, TASK_INTERRUPTIBLE);

		/* Increment the waiting_writers count */
		if (type == ROT_WRITE && !writer_waiting) {
			mutex_lock(&locks_mutex);
			for (i = low; i <= high; i++) {
				if (i == MAX_DEGREE) {
					i = 0;
				}
				locks[i].waiting_writers++;
			}
			writer_waiting = 1;
			mutex_unlock(&locks_mutex);
		}
		schedule(); // Go to sleep
		mutex_lock(&orientation_mutex);
		mutex_lock(&locks_mutex);
	}

	/* Delete current task in wait queue */
	finish_wait(&requests, &wait);
	mutex_unlock(&orientation_mutex);

	/* Add the lock info to holding lock list */
	list_add(&lock->list, &locks_info);

	/* Decrement the waiting_writers count */
	if (writer_waiting) {
		for (i = low; i <= high; i++) {
			if (i == MAX_DEGREE) {
				i = 0;
			}
			locks[i].waiting_writers--;
		}
		writer_waiting = 0;
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

	mutex_unlock(&locks_mutex);
	return lock->id;
}

/// @brief Revoke access claimed by a call to rotation_lock.
/// @param id The ID associated with the access to revoke.
/// @return On success, returns 0. On invalid argument, returns -EINVAL. On permission error, returns -EPERM.
SYSCALL_DEFINE1(rotation_unlock, long, id)
{
	int i;

	/* Return -EINVAL if id is negative */
	if (id < 0) {
		return -EINVAL;
	}

	mutex_lock(&locks_mutex);

	/* Find the corresponding lock */
	struct lock_info *lock = find_lock(id);

	/* Return -EINVAL if no such lock */
	if (lock == NULL) {
		mutex_unlock(&locks_mutex);
		return -EINVAL;
	}

	/* Return -EPERM if the process doesn't own the lock */
	if (lock->pid != current->pid) {
		mutex_unlock(&locks_mutex);
		return -EPERM;
	}

	/* Delete the lock from holding lock list */
	list_del(&lock->list);
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

	mutex_unlock(&locks_mutex);

	kfree(lock);

	/* Wake up all processes waiting for the lock */
	wake_up_all(&requests);

	return 500;
}

void locks_init(void)
{
	int i;
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
	int i;

	if (!orientation_in_range(low, high)) {
		return 0;
	}

	if (type == ROT_READ) {
		/* Reader */
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
		/* Writer */
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
	struct lock_info *lock;
	list_for_each_entry(lock, &locks_info, list) {
		if (lock->id == id) {
			return lock;
		}
	}
	return NULL;
}