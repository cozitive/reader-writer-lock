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

static long deg_to_lock_id[360]; // Maps degrees to lock IDs
memset(deg_to_lock_id, -1, sizeof(long)); // Initialize all entries to -1
static DEFINE_MUTEX(deg_to_lock_id_lock); // Mutex to protect `deg_to_lock_id`

static LIST_HEAD(requests); // List of pending requests
static DEFINE_MUTEX(requests_lock); // Mutex to protect `requests`

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

/// @brief Claim read or write access in the specified degree range.
/// @param low The beginning of the degree range (inclusive). Value must be in the range 0 <= low < 360.
/// @param high The end of the degree range (inclusive). Value must be in the range 0 <= high < 360.
/// @param type The type of the access claimed. Value must be either ROT_READ or ROT_WRITE.
/// @return On success, returns a non-negative lock ID that is unique for each call to rotation_lock. On invalid argument, returns -EINVAL.
SYSCALL_DEFINE3(rotation_lock, int, low, int, high, int, type)
{
	if (low < 0 || low >= 360 || high < 0 || high >= 360 || (type != ROT_READ && type != ROT_WRITE))
		return -EINVAL;

	struct access_request *request = kmalloc(sizeof(struct access_request), GFP_KERNEL);
	if (request == NULL)
		return -ENOMEM;

	request->pid = current->pid;
	request->low = low;
	request->high = high;
	request->lock_type = type;

	long id = 0;

	// 여기서는 지금 "너한테 줄수 있느냐?"만 따짐

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

	mutex_lock(&locks_lock);
	list_del(&lock->list);
	mutex_unlock(&locks_lock);

	mutex_lock(&deg_to_lock_id_lock);
	for (int i = lock->low; i <= lock->high; i++)
		deg_to_lock_id[i] = -1;
	mutex_unlock(&deg_to_lock_id_lock);

	// 제거하고나서, wait queue에 있는 남은 애들 중에서 들어갈수 있는 애가 있는지 확인하고, 있으면 걔한테 줌

	return 0;
}