/* Wrapper functions for custom syscalls. */

#define SYSCALL_SET_ORIENTATION 294
#define SYSCALL_ROTATION_LOCK 295
#define SYSCALL_ROTATION_UNLOCK 296

/// @brief Wrapper function for sys_set_orientation. (Set the current device orientation.)
/// @param degree The degree to set as the current device orientation. Value must be in the range 0 <= degree < 360.
/// @return 0 on success, -1 on error. errno is set to indicate the error.
long set_orientation(int degree);

/// @brief Wrapper function for sys_rotation_lock. (Claim read or write access in the specified degree range.)
/// @param low The beginning of the degree range (inclusive). Value must be in the range 0 <= low < 360.
/// @param high The end of the degree range (inclusive). Value must be in the range 0 <= high < 360.
/// @param type The type of the access claimed. Value must be either ROT_READ or ROT_WRITE.
/// @return Lock ID on success, -1 on error. errno is set to indicate the error.
long rotation_lock(int low, int high, int type);

/// @brief Wrapper function for sys_rotation_unlock. (Revoke access claimed by a call to rotation_lock.)
/// @param id The ID associated with the access to revoke.
/// @return 0 on success, -1 on error. errno is set to indicate the error.
long rotation_unlock(long id);