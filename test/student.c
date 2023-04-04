#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include "common.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("%s\n", "Usage: ./student LOW HIGH");
        return EXIT_FAILURE;
    }

    int low = 0;
    int high = 0;

    sscanf(argv[1], "%d", &low);
    sscanf(argv[2], "%d", &high);

    if (low < 0 || high < 0) {
        printf("%s\n", "Low and high must be positive");
        return EXIT_FAILURE;
    }

    if (low > high) {
        printf("%s\n", "Low must be less than or equal to high");
        return EXIT_FAILURE;
    }

    return 0;
}