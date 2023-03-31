#include <linux/kernel.h>
#include <linux/syscalls.h>

SYSCALL_DEFINE3(rotation_lock, int, low, int, high, int, type)
{
	// TODO: implement this
    printk(KERN_INFO "rotation_lock");
	return 0;
}