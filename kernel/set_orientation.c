#include <linux/kernel.h>
#include <linux/syscalls.h>

SYSCALL_DEFINE1(set_orientation, int, degree)
{
	// TODO: implement this
	printk(KERN_INFO "set_orientation");
	return 0;
}