#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
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
	if (lock_id > 0) {
		if (rotation_unlock(lock_id) < 0) {
			perror("rotation_unlock");
			exit(EXIT_FAILURE);
		}
	}
}

/// @brief Factorize a number using Trial Division
/// @param num The number to factorize
/// @return An array of factors. The last element is -1.
int *factorize(int num) {
	int *factors = malloc(sizeof(int) * 100);
	if (factors == NULL) {
		perror("malloc");
		cleanup();
		exit(EXIT_FAILURE);
	}
	int i = 2;
	int j = 0;
	while (num > 1) {
		if (num % i == 0) {
			factors[j] = i;
			num /= i;
			j++;
		} else {
			i++;
		}
	}
	factors[j] = -1;
	return factors;
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		printf("Usage: ./student LOW HIGH\n");
		return EXIT_FAILURE;
	}

	int low = 0;
	int high = 0;

	sscanf(argv[1], "%d", &low);
	sscanf(argv[2], "%d", &high);

	if (low > high || low < 0 || high > 180) {
		printf("[low, high] should be within [0, 180]\n");
		return EXIT_FAILURE;
	}

	while (1) {
		if ((lock_id = rotation_lock(low, high, ROT_READ)) < 0) {
			perror("rotation_lock");
			cleanup();
			return EXIT_FAILURE;
		}

		fd = open("quiz", O_RDONLY);
		if (fd < 0) {
			perror("open");
			cleanup();
			return EXIT_FAILURE;
		}

		char buf[30];
		if (read(fd, buf, 30) < 0) {
			perror("read");
			cleanup();
			return EXIT_FAILURE;
		}

		int num = 0;
		sscanf(buf, "%d", &num);

		printf("student-%d-%d: %d = ", low, high, num);

		int *factors = factorize(num);
		int i = 0;
		while (factors[i] != -1) {
			printf("%d", factors[i]);
			i++;
			if (factors[i] != -1) {
				printf(" * ");
			}
		}
		free(factors);
		printf("\n");
		if (close(fd) < 0) {
			perror("close");
			cleanup();
			return EXIT_FAILURE;
		}
		fd = -1;

		if (rotation_unlock(lock_id) < 0) {
			perror("rotation_unlock");
			return EXIT_FAILURE;
		}
		lock_id = -1;

		sleep(1);
	}

	return EXIT_SUCCESS;
}