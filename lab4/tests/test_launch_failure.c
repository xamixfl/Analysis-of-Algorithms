/* Inject pthread_create failures without changing the measured source file. */
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned calls, fail_at;
static int failing_create(pthread_t *t, const pthread_attr_t *a,
                          void *(*function)(void *), void *argument)
{
    if (calls++ == fail_at) return EAGAIN;
    return pthread_create(t, a, function, argument);
}
#define pthread_create failing_create
#include "../src/segmentation.c"
#undef pthread_create
int main(void)
{
    uint8_t input[1024], output[1024]; unsigned threshold;
    for (unsigned i = 0; i < sizeof(input); ++i) input[i] = (uint8_t)i;
    const unsigned failures[] = {0, 1, 3, 7};
    for (unsigned i = 0; i < sizeof(failures)/sizeof(failures[0]); ++i) {
        calls = 0; fail_at = failures[i]; memset(output, 90, sizeof(output));
        assert(segment_parallel(input, output, sizeof(input), 8, &threshold) == EAGAIN);
        for (unsigned j = 0; j < sizeof(output); ++j) assert(output[j] == 90);
    }
    puts("Launch failure: 4 injected failures, existing workers joined, mask untouched: PASS");
    return 0;
}
