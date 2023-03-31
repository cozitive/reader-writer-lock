#include <linux/rotation.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/mutex.h>

/*
 * Readers and writers.
 *
 */
#define ROT_READ 0
#define ROT_WRITE 1

/// @brief Set the current device orientation.
/// @param degree The degree to set as the current device orientation. Value must be in the range 0 <= degree < 360.
/// @return Zero on success, -EINVAL on invalid argument.
SYSCALL_DEFINE1(set_orientation, int, degree)
{
	if (degree < 0 || degree >= 360)
		return -EINVAL;
}

/// @brief Claim read or write access in the specified degree range.
/// @param low The beginning of the degree range (inclusive). Value must be in the range 0 <= low < 360.
/// @param high The end of the degree range (inclusive). Value must be in the range 0 <= high < 360.
/// @param type The type of the access claimed. Value must be either ROT_READ or ROT_WRITE.
/// @return On success, returns a non-negative lock ID that is unique for each call to rotation_lock. On invalid argument, returns -EINVAL.
SYSCALL_DEFINE3(rotation_lock, int, low, int, high, int, type)
{
	// TODO: implement this
	printk(KERN_INFO "rotation_lock");
	return 0;
}

/// @brief Revoke access claimed by a call to rotation_lock.
/// @param id The ID associated with the access to revoke.
/// @return On success, returns 0. On invalid argument, returns -EINVAL. On permission error, returns -EPERM.
SYSCALL_DEFINE1(rotation_unlock, long, id)
{
	// TODO: implement this
	printk(KERN_INFO "rotation_unlock");
	return 0;
}