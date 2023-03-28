#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("%s\n", "Usage: ./student LOW HIGH");
        return 1;
    }

    int low = 0;
    int high = 0;

    sscanf(argv[1], "%d", &low);
    sscanf(argv[2], "%d", &high);

    if (low < 0 || high < 0) {
        printf("%s\n", "Low and high must be positive");
        return 1;
    }

    if (low > high) {
        printf("%s\n", "Low must be less than or equal to high");
        return 1;
    }

    return 0;
}