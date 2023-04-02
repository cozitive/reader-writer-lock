#include <linux/rotation.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/mutex.h>
#include <linux/wait.h>

static int device_orientation = 0;
static DEFINE_MUTEX(orientation_lock);

/// @brief Set the current device orientation.
/// @param degree The degree to set as the current device orientation. Value must be in the range 0 <= degree < 360.
/// @return Zero on success, -EINVAL on invalid argument.
SYSCALL_DEFINE1(set_orientation, int, degree)
{
	if (degree < 0 || degree >= 360)
		return -EINVAL;

	mutex_lock(&orientation_lock);
	device_orientation = degree;
	mutex_unlock(&orientation_lock);

	return 0;
}

static LIST_HEAD(locks); // List of currently held locks
static DEFINE_MUTEX(locks_lock); // Mutex to protect `locks`

/* degree_to_locks: 효율성을 위해서 도입했으나 일단 지금은 신경쓰기 귀찮으므로, 1차로 구현 끝내고 나서 다시 넣기 */
// static struct list_head degree_to_locks[360]; // List of locks for each degree
// for (int i = 0; i < 360; i++) INIT_LIST_HEAD(&degree_to_locks[i]);
// static DEFINE_MUTEX(degree_to_locks_lock); // Mutex to protect `degree_to_locks`

static wait_queue_head_t requests; // Wait queue for blocking requests
init_waitqueue_head(&requests);

static long next_lock_id = 0; // The next lock ID to use

/// @brief Add a request to the list of requests.
/// @param[in] request The request to add.
/// @param[out] lock_id The ID of the lock associated with the request.
void add_request(struct access_request *request);

/// @brief Remove a request.
/// @param request The request to remove.
void remove_request(struct access_request *request);

/// @brief Find a request by process ID.
/// @param pid The process ID to search for.
/// @return The request, or NULL if not found.
struct access_request *find_request(pid_t pid);

/// @brief Filter the list of requests by degree and lock type.
/// @param degree The degree.
/// @param lock_type The lock type.
/// @return The filtered list.
struct list_head *filter_requests(int degree, int lock_type);

/// @brief Find a lock by ID.
/// @param id The ID to search for.
/// @return The lock, or NULL if not found.
struct rotation_lock *find_lock(long id);

/// @brief Check if current device orientation is in the specified degree range.
/// @param low
/// @param high
/// @return 1 if yes, 0 if no.
int orientation_in_range(int low, int high);

/// @brief Check if a lock is available.
/// @param request The request to check.
/// @return 1 if yes, 0 if no.
int lock_available(struct access_request *request)

/// @brief Claim read or write access in the specified degree range.
/// @param low The beginning of the degree range (inclusive). Value must be in the range 0 <= low < 360.
/// @param high The end of the degree range (inclusive). Value must be in the range 0 <= high < 360.
/// @param type The type of the access claimed. Value must be either ROT_READ or ROT_WRITE.
/// @return On success, returns a non-negative lock ID that is unique for each call to rotation_lock. On invalid argument, returns -EINVAL.
SYSCALL_DEFINE3(rotation_lock, int, low, int, high, int, type)
{
	if (low < 0 || low >= 360 || high < 0 || high >= 360 ||
	    (type != ROT_READ && type != ROT_WRITE))
		return -EINVAL;

	long id = 0; // The ID of the lock (return value)

	struct access_request *request =
		kmalloc(sizeof(struct access_request), GFP_KERNEL);
	if (request == NULL)
		return -ENOMEM;

	request->pid = current->pid;
	request->low = low;
	request->high = high;
	request->lock_type = type;

	init_waitqueue_entry(&request->wait_entry, current);

	add_wait_queue(&requests, &request->wait_entry);
	while (!lock_available(request)) {
		prepare_to_wait(&requests, &request->wait_entry, TASK_INTERRUPTIBLE);
		schedule();
	}
	// Be careful for races!!!!!!! (나중에 다시 확인)
	// 여러 프로세스들이 기다리던 중에 동시에 lock_available이 되었을때 동시에 여기로 빠져나올수 있지만 실제로는 한 프로세스에게만 락을 줘야함.
	finish_wait(&requests, &request->wait_entry);


	/* Give lock to the process */
	// TODO
}

/// @brief Revoke access claimed by a call to rotation_lock.
/// @param id The ID associated with the access to revoke.
/// @return On success, returns 0. On invalid argument, returns -EINVAL. On permission error, returns -EPERM.
SYSCALL_DEFINE1(rotation_unlock, long, id)
{
	/* Negative id, return -EINVAL */
	if (id < 0)
		return -EINVAL;

	struct rotation_lock *lock = find_lock(id);

	/* No such lock, return -EINVAL */
	if (lock == NULL)
		return -EINVAL;

	/* Process is not the owner of the lock, return -EPERM */
	if (lock->pid != current->pid)
		return -EPERM;

	/* Delete the lock from list */
	mutex_lock(&locks_lock);
	list_del(&lock->list);
	mutex_unlock(&locks_lock);

	return 0;
}

int orientation_in_range(int low, int high)
{
	if (low <= high)
		return device_orientation >= low && device_orientation <= high;
	else
		return device_orientation >= low || device_orientation <= high;
}

int lock_available(struct access_request *request)
{
	// TODO
	return 0;
}