#include <stdlib.h>
#include <stdio.h>

float ***create_3d_float_array(size_t num_i, size_t num_j, size_t num_k) {
    // Allocate memory for pointers and data in one block
    size_t ptrs_size = num_i * sizeof(float **) + num_i * num_j * sizeof(float *);
    size_t data_size = num_i * num_j * num_k * sizeof(float);
    char *block = malloc(ptrs_size + data_size);
    if (!block) return NULL;

    float ***f = (float ***) block;
    float **f_j = (float **)(block + num_i * sizeof(float **));
    float *f_k = (float *)(block + ptrs_size);

    // Set up pointers
    for (size_t i = 0; i < num_i; ++i) {
        f[i] = f_j + i * num_j;
        for (size_t j = 0; j < num_j; ++j) {
            f[i][j] = f_k + (i * num_j * num_k) + (j * num_k);
        }
    }

    return f;
}
