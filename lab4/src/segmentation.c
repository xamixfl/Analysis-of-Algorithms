#include "segmentation.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

unsigned otsu_threshold(const uint64_t h[LEVELS], size_t n)
{
    uint64_t total = 0, left_sum = 0, left_count = 0;
    unsigned best = 0, only = 0;
    long double maximum = -1.0L;
    for (unsigned v = 0; v < LEVELS; ++v) {
        total += (uint64_t)v * h[v];
        if (h[v]) only = v;
    }
    for (unsigned t = 0; t < LEVELS - 1; ++t) {
        left_count += h[t];
        left_sum += (uint64_t)t * h[t];
        if (left_count == 0 || left_count == n) continue;
        long double delta = (long double)n * left_sum
                          - (long double)left_count * total;
        long double score = delta * delta /
            ((long double)left_count * (n - left_count));
        if (score > maximum) { maximum = score; best = t; }
    }
    /* Empty class at every threshold: choose the only occupied level. */
    return maximum < 0.0L ? only : best;
}

static int valid(const uint8_t *in, uint8_t *out, size_t n, unsigned *t)
{
    return in && out && t && n > 0 && n <= UINT64_MAX / 255;
}

int segment_sequential(const uint8_t *in, uint8_t *out,
                       size_t n, unsigned *threshold)
{
    if (!valid(in, out, n, threshold)) return EINVAL;
    uint64_t histogram[LEVELS] = {0};
    for (size_t i = 0; i < n; ++i) ++histogram[in[i]];
    *threshold = otsu_threshold(histogram, n);
    for (size_t i = 0; i < n; ++i)
        out[i] = in[i] > *threshold ? 255 : 0;
    return 0;
}

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t ready, proceed;
    unsigned completed, phase, threshold;
    int abort;
} Shared;

typedef struct {
    const uint8_t *in;
    uint8_t *out;
    size_t begin, end;
    uint64_t histogram[LEVELS];
    Shared *shared;
} Worker;

static void *worker_main(void *argument)
{
    Worker *w = argument;
    Shared *s = w->shared;
    for (size_t i = w->begin; i < w->end; ++i)
        ++w->histogram[w->in[i]];
    pthread_mutex_lock(&s->mutex);
    ++s->completed;
    pthread_cond_signal(&s->ready);
    while (s->phase == 0 && !s->abort)
        pthread_cond_wait(&s->proceed, &s->mutex);
    unsigned threshold = s->threshold;
    int abort = s->abort;
    pthread_mutex_unlock(&s->mutex);
    if (!abort)
        for (size_t i = w->begin; i < w->end; ++i)
            w->out[i] = w->in[i] > threshold ? 255 : 0;
    return NULL;
}

int segment_parallel(const uint8_t *in, uint8_t *out,
                     size_t n, unsigned q, unsigned *threshold)
{
    if (!valid(in, out, n, threshold) || q == 0 || q > MAX_WORKERS)
        return EINVAL;
    Worker *workers = calloc(q, sizeof(*workers));
    pthread_t *threads = calloc(q, sizeof(*threads));
    if (!workers || !threads) { free(workers); free(threads); return ENOMEM; }
    Shared shared = {0};
    int rc = pthread_mutex_init(&shared.mutex, NULL);
    if (rc) goto free_buffers;
    rc = pthread_cond_init(&shared.ready, NULL);
    if (rc) goto destroy_mutex;
    rc = pthread_cond_init(&shared.proceed, NULL);
    if (rc) goto destroy_ready;
    unsigned created = 0;
    for (unsigned j = 0; j < q; ++j) {
        /* This equivalent form avoids overflow of n*j. */
        size_t base = n / q, extra = n % q;
        workers[j].begin = j * base + (j < extra ? j : extra);
        workers[j].end = workers[j].begin + base + (j < extra);
        workers[j].in = in;
        workers[j].out = out;
        workers[j].shared = &shared;
        rc = pthread_create(&threads[j], NULL, worker_main, &workers[j]);
        if (rc) break;
        ++created;
    }
    pthread_mutex_lock(&shared.mutex);
    if (rc) {
        shared.abort = 1;
    } else {
        while (shared.completed != q)
            pthread_cond_wait(&shared.ready, &shared.mutex);
        uint64_t histogram[LEVELS] = {0};
        for (unsigned j = 0; j < q; ++j)
            for (unsigned v = 0; v < LEVELS; ++v)
                histogram[v] += workers[j].histogram[v];
        *threshold = otsu_threshold(histogram, n);
        shared.threshold = *threshold;
        shared.phase = 1;
    }
    pthread_cond_broadcast(&shared.proceed);
    pthread_mutex_unlock(&shared.mutex);
    for (unsigned j = 0; j < created; ++j) pthread_join(threads[j], NULL);
    pthread_cond_destroy(&shared.proceed);
destroy_ready:
    pthread_cond_destroy(&shared.ready);
destroy_mutex:
    pthread_mutex_destroy(&shared.mutex);
free_buffers:
    free(threads);
    free(workers);
    return rc;
}
