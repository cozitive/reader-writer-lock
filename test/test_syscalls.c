#include <sys/syscall.h>
#include <unistd.h>
#include "common.h"

int main()
{
    syscall(SYSCALL_SET_ORIENTATION, 0);
    syscall(SYSCALL_ROTATION_LOCK, 0, 0, 0);
    syscall(SYSCALL_ROTATION_UNLOCK, 0);
    return 0;
}