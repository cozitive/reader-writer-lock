#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "common.h"
#include "wrappers.h"

int main(int argc, char *argv[])
{
	int curr = 0;

	if (argc != 2) {
		printf("%s\n", "Usage: ./professor STARTING_INTEGER");
		return EXIT_FAILURE;
	}

	sscanf(argv[1], "%d", &curr);

	while (1) {
		long lock_id;
		if (lock_id = rotation_lock(0, 180, ROT_WRITE) < 0) {
			perror("rotation_lock");
			return EXIT_FAILURE;
		}
		int fd = open("quiz", O_WRONLY | O_CREAT, 0644);
		if (fd < 0) {
			perror("open");
			return EXIT_FAILURE;
		}

		char buf[30];
		sprintf(buf, "%d", curr);

		if (write(fd, buf, strlen(buf)) < 0) {
			perror("write");
			close(fd);
			return EXIT_FAILURE;
		}

		close(fd);

		if (rotation_unlock(lock_id) < 0) {
			perror("rotation_unlock");
			return EXIT_FAILURE;
		}

		printf("professor: %d\n", curr);
		curr++;

		sleep(1);
	}
	return EXIT_SUCCESS;
}