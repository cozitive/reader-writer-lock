#include <stdio.h>
#include "common.h"

int main(int argc, char *argv[]) {
    int starting_integer = 0;

    if (argc != 2) {
        printf("%s\n", "Usage: ./professor STARTING_INTEGER");
        return 1;
    }

    sscanf(argv[1], "%d", &starting_integer);

    if (starting_integer < 0) {
        printf("%s\n", "Starting integer must be positive");
        return 1;
    }

    return 0;
}