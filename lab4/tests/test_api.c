#include "segmentation.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    uint8_t input[257] = {0}, output[257], expected[257]; unsigned t, reference;
    assert(segment_sequential(NULL, output, 1, &t) == EINVAL);
    assert(segment_sequential(input, NULL, 1, &t) == EINVAL);
    assert(segment_sequential(input, output, 0, &t) == EINVAL);
    assert(segment_sequential(input, output, 1, NULL) == EINVAL);
    assert(segment_parallel(input, output, 1, 0, &t) == EINVAL);
    assert(segment_parallel(input, output, 1, MAX_WORKERS + 1, &t) == EINVAL);
    assert(segment_parallel(NULL, output, 1, 1, &t) == EINVAL);
    for (unsigned i = 0; i < 257; ++i) input[i] = (uint8_t)(i * 73);
    assert(!segment_sequential(input, expected, 257, &reference));
    unsigned qs[] = {1, 2, 3, 4, 8, 16, 32};
    for (unsigned j = 0; j < sizeof(qs)/sizeof(qs[0]); ++j) {
        assert(!segment_parallel(input, output, 257, qs[j], &t));
        assert(t == reference && !memcmp(output, expected, 257));
        memcpy(output, input, 257);
        assert(!segment_parallel(output, output, 257, qs[j], &t));
        assert(t == reference && !memcmp(output, expected, 257));
    }
    input[0] = 255;
    assert(!segment_parallel(input, output, 1, 32, &t));
    assert(t == 255 && output[0] == 0);
    printf("API: 7 invalid checks, 14 equivalence/in-place cases, q>n: PASS\n");
    return 0;
}
