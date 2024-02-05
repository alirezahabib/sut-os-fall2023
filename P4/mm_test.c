/* A simple test harness for memory alloction. */

#include "mm_alloc.h"
#include <stdio.h>

void print_array(int* array, int size) {
    for (int i = 0; i < size; i++) {
        printf("%d ", array[i]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    int *data = (int*) mm_malloc(4);

    print_array(data, 4);
    data[0] = 1;
    data[1] = 2;
    data[2] = 3;
    data[3] = 4;
    print_array(data, 4);
    mm_free(data);
    printf("malloc sanity test successful!\n");
    return 0;
}
