#include <linux/kernel.h>
#include <linux/syscalls.h>


/// @brief Set the current device orientation.
/// @param degree The degree to set as the current device orientation. Value must be in the range 0 <= degree < 360.
/// @return Zero on success, -EINVAL on invalid argument.
SYSCALL_DEFINE1(set_orientation, int, degree)
{
	// TODO: implement this
	printk(KERN_INFO "set_orientation");
	return 0;
}