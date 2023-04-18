# Orientation-Based Reader-Writer Lock

This project implements a reader-writer lock based on the device's current orientation.

## Struct Definition

### `struct lock_info`

It contains information about the lock.

- `id`: lock ID
- `pid`: process ID owning the lock
- `low`: beginning of degree range
- `high`: end of degree range
- `type`: read or write lock
- `list`: pointer to lock list node

### `struct reader_writer_lock`

It represents numbers of readers and writers of each degree.

- `active_readers`: number of active readers
- `active_writers`: Number of active writers
- `waiting_writers`: Number of waiting writers

## Shared Variables & Mutexes

- `device_orientation`: current device orientation
  - `orientation_mutex`: protects `device_orientation`
- `locks_initialized`: `1` if initialized, else `0`
  - `initialized_mutex`: protects `locks_initialized`

- `next_lock_id`: next ID number to be allocated to new lock
  - `next_lock_id_mutex`: protects `next_lock_id`
- `locks_info`: list of currently held locks' `lock_info`
- `locks`: array of readers/writers for each degree
  - `locks_mutex`: protects both `locks_info` and `locks`
- `requests`: wait queue of `rotation_lock` requests

## System Calls

Three system calls are used in the synchronization system:

- `set_orientation`
- `rotation_lock`
- `rotation_unlock`

### `set_orientation` (#294)

It sets current device orientation.

```c
long set_orientation(int degree);
```

#### Arguments

- `degree`: degree to be set

#### Return Value

- success: `0`
- invalid argument: `-EINVAL`

#### Implementation

1. Check validity of input arguments.
   - If degree is out of range, return `-INVAL`.
2. Lock `orientation_mutex`.
3. Set `device_orientation` to `degree`.
4. Unlock `orientation_mutex`.
5. Wake up all processes waiting for rotation lock acquire.
6. Return 0.

### `rotation_lock` (#295)

It requests a read or write access in the specified degree range.

```c
long rotation_lock(int low, int high, int type);
```

#### Arguments

- `low`: beginning of degree range
- `high`: end of degree range
- `type`: `ROT_READ` or `ROT_WRITE`

#### Return Value

- success: unique, non-negative lock ID
- invalid argument: `-EINVAL`

#### Implementation

1. Check validity of input arguments.
   - If degree is out of range or type is invalid, return `-EINVAL`.
2. Initialize `lock` array if not initialized.
   1. Lock `initialized_mutex`.
   2. If already initialized, return.
   3. Lock `locks_mutex`.
   4. Set all counts of `locks` to 0.
   5. Unlock `locks_mutex`.
   6. Set `locks_initialized` to 1(mark as initialized).
   7. Unlock `initialized_mutex`.
3. Create a new `lock_info` struct.
   1. Lock `next_lock_id_mutex`.
   2. Allocate current `next_lock_id` to the new lock, and increment it.
   3. Unlock `next_lock_id_mutex`.
   4. Initialize other values (`pid`, `low`, `high`, `type`, `list`) of the new lock.
4. Add current task to the shared wait queue(`requests`).
5. Wait until the access meets the reader-writer lock condition.
   1. Lock `orientation_mutex` and `locks_mutex`.
   2. Iterate the loop until the requested access becomes acceptable.
      1. Unock `orientation_mutex` and `locks_mutex`.
      2. Change current task's state to `TASK_INTERRUPTIBLE`.
      3. If the requested access is write access, 
         1. Lock `locks_mutex`.
         2. increment `waiting_writers` count of corresponding degrees.
         3. Unock `locks_mutex`.
      4. Call `schedule()` and go to sleep.
      5. After wakeup, relock `orientation_mutex` and `locks_mutex` for condition check.
6. Delete current task in the shared wait queue.
7. Add new `lock_info` to the holding lock list(`locks_info`).
8. If the requested access is write access, decrement `waiting_writers` count.
9. Increment `active_readers` or `active_writers` count.
10. Unlock `orientation_mutex` and `locks_mutex`.
11. Return the lock ID.

#### Fairness Policy

If degree range of waiting readers and waiting writers overlap, the readers must not access before the waiting writer.

- read access should wait if active/waiting writer exists.
- write access should wait if active reader/writer exists.

Access acceptance condition is checked in `lock_available()` helper function.

```c
if (type == ROT_READ) {
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
```

### `rotation_unlock` (#296)

It revokes the access created by `rotation_lock`.

```c
long rotation_unlock(long id);
```

#### Arguments

- `id`: lock ID

#### Return Value

- success: `0`
- invalid argument: `-EINVAL`
- no permission: `-EPERM`

#### Implementation

1. Check validity of input arguments.
   - If `id` is negative, return `-EINVAL`.
2. Lock `locks_mutex`.
3. Find the lock by `id` in `locks_info` list.
   - If corresponding lock does not exist, unlock `locks_mutex` and return `-INVAL`.
   - If the lock is owned by other process, unlock `locks_mutex` and return `-EPERM`.
4. Delete the lock from `locks_info` list.
5. Decrement `active_readers` or `active_writers` count.
6. Unlock `locks_mutex`.
7. Free the allocated memory for the lock.
   - `kfree()` should be called after releasing mutex to prevent deadlock.
8. Wake up all processes waiting for rotation lock acquire.
9. Return 0.

## Removing Requests at Process Termination

If the process terminates with waiting or active access requests, the remaining requests should be removed. For cleanup, `exit_rotlock()` is called in `do_exit()`, a function called at process termination.

### `exit_rotlock`

It revokes all accesses created by the terminated process.

```c
void exit_rotlock(struct task_struct *tsk)
```

#### Arguments

- `tsk`: current task

#### Implementation

1. Lock `locks_mutex`.
2. For all `lock_info` structs `locks_info` list, check if the lock is owned by the current task.
   If current task owns the lock,
   1. Delete it from `locks_info` list.
   2. Add it to garbage collection list(`garbage`).
   3. Decrement `active_readers` or `active_writers` count.
3. Unlock `locks_mutex`.
4. Wake up all processes waiting for rotation lock acquire.
5. Free all locks in the GC list.
   - `kfree()` should be called after releasing mutex to prevent deadlock.

## Lessons

### Wait Queue API

Wait queue API can put a task in a wait queue(`add_wait_queue`), let the current task sleep(`schedule`), and wake up all tasks in the wait queue(`wake_up_all`).

### Cleanup at Process Termination

In our early implementation, the cleanup function `exit_rotlock` is called in `SIGINT` signal handler. The cleanup process worked well at our simple test, terminating the process by pressing `Ctrl+C`; however, if the process terminated by other factor, access requests of the processes remained in `locks_info` list. To resolve the problem, `exit_rotlock` call is moved into `do_exit` function.

### Shared Variable MUST BE PROTECTED BY MUTEX

Mutex for `locks_initialized` is omitted first, since we overlooked possible race condition. However, not protecting `locks_initialized` can cause double initialization of `lock`, so the corresponding mutex is added.

### No Double Mutex Lock/Unlock

When `mutex_lock` is called twice without unlock (and vice versa), Tizen kernel became stuck. Mutex lock/unlock should be located properly to prevent double lock/unlock.