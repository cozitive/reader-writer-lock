#include <sys/syscall.h>
#include <unistd.h>
#include "wrappers.h"

long set_orientation(int degree)
{
	return syscall(SYSCALL_SET_ORIENTATION, degree);
}

long rotation_lock(int low, int high, int type)
{
	return syscall(SYSCALL_ROTATION_LOCK, low, high, type);
}

long rotation_unlock(long id)
{
	return syscall(SYSCALL_ROTATION_UNLOCK, id);
}