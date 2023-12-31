#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "common.h"
#include "wrappers.h"

/* Global variables are not good but they are needed for the signal handler!!! */
int fd = -1; // file descriptor
long lock_id = -1; // rotation lock id

void cleanup() {
	if (fd > 0) {
		if (close(fd) < 0) {
			perror("close");
			exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char *argv[]) {
	int curr = 0;

	if (argc != 2) {
		printf("%s\n", "Usage: ./professor STARTING_INTEGER");
		return EXIT_FAILURE;
	}

	sscanf(argv[1], "%d", &curr);

	while (1) {
		if ((lock_id = rotation_lock(0, 180, ROT_WRITE)) < 0) {
			perror("rotation_lock");
			return EXIT_FAILURE;
		}
		fd = open("quiz", O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0644);
		if (fd < 0) {
			perror("open");
			cleanup();
			return EXIT_FAILURE;
		}

		char buf[30];
		sprintf(buf, "%d", curr);

		if (write(fd, buf, strlen(buf)) < 0) {
			perror("write");
			cleanup();
			return EXIT_FAILURE;
		}

		if (close(fd) < 0) {
			perror("close");
			return EXIT_FAILURE;
		}
		fd = -1;

		if (rotation_unlock(lock_id) < 0) {
			perror("rotation_unlock");
			return EXIT_FAILURE;
		}
		lock_id = -1;

		printf("professor: %d\n", curr);
		curr++;

		sleep(1);
	}
	return EXIT_SUCCESS;
}