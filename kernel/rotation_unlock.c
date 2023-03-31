#include <linux/kernel.h>
#include <linux/syscalls.h>

SYSCALL_DEFINE1(rotation_unlock, long, id)
{
	// TODO: implement this
    printk(KERN_INFO "rotation_unlock");
	return 0;
}