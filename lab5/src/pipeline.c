#include "lab5.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct {
    Request *items[QUEUE_CAPACITY];
    size_t head;
    size_t count;
    int closed;
    pthread_mutex_t mutex;
    pthread_cond_t readable;
    pthread_cond_t writable;
} Queue;

typedef struct {
    Queue *input;
    Queue *output;
    size_t stage;
    const char *directory;
} Worker;

static int queue_init(Queue *queue)
{
    int error = pthread_mutex_init(&queue->mutex, NULL);
    if (error)
        return error;
    error = pthread_cond_init(&queue->readable, NULL);
    if (!error) {
        error = pthread_cond_init(&queue->writable, NULL);
        if (!error)
            return 0;
        pthread_cond_destroy(&queue->readable);
    }
    pthread_mutex_destroy(&queue->mutex);
    return error;
}

static void queue_destroy(Queue *queue)
{
    pthread_cond_destroy(&queue->writable);
    pthread_cond_destroy(&queue->readable);
    pthread_mutex_destroy(&queue->mutex);
}

/* Each queue has one producer. It closes the queue after its last push. */
static void queue_push(Queue *queue, Request *request)
{
    pthread_mutex_lock(&queue->mutex);
    while (queue->count == QUEUE_CAPACITY)
        pthread_cond_wait(&queue->writable, &queue->mutex);
    size_t tail = (queue->head + queue->count) % QUEUE_CAPACITY;
    queue->items[tail] = request;
    ++queue->count;
    pthread_cond_signal(&queue->readable);
    pthread_mutex_unlock(&queue->mutex);
}

static Request *queue_pop(Queue *queue)
{
    Request *request = NULL;
    pthread_mutex_lock(&queue->mutex);
    while (queue->count == 0 && !queue->closed)
        pthread_cond_wait(&queue->readable, &queue->mutex);
    if (queue->count) {
        request = queue->items[queue->head];
        queue->head = (queue->head + 1) % QUEUE_CAPACITY;
        --queue->count;
        pthread_cond_signal(&queue->writable);
    }
    pthread_mutex_unlock(&queue->mutex);
    return request;
}

static void queue_close(Queue *queue)
{
    pthread_mutex_lock(&queue->mutex);
    queue->closed = 1;
    pthread_cond_broadcast(&queue->readable);
    pthread_mutex_unlock(&queue->mutex);
}

static void process_stage(Request *request, size_t stage, const char *directory)
{
    assert(stage < STAGES);
    if (!request->error) {
        Interval *interval = &request->time[stage];
        interval->ran = 1;
        interval->start = clock_ticks();
        switch (stage) {
        case 0:
            request->error = read_pgm(request->input, &request->image);
            break;
        case 1:
            request->threshold = segment_otsu(&request->image);
            break;
        case 2:
            request->error = write_mask(directory, request);
            break;
        }
        interval->finish = clock_ticks();
    }
    if (stage == STAGES - 1) {
        free(request->image.pixels);
        request->image.pixels = NULL;
    }
}

static void *work(void *argument)
{
    Worker *worker = argument;
    Request *request;
    while ((request = queue_pop(worker->input)) != NULL) {
        process_stage(request, worker->stage, worker->directory);
        if (worker->output)
            queue_push(worker->output, request);
    }
    if (worker->output)
        queue_close(worker->output);
    return NULL;
}

void run_sequential(Request *requests, size_t count, const char *directory)
{
    for (size_t i = 0; i < count; ++i) {
        requests[i].submitted = clock_ticks();
        for (size_t stage = 0; stage < STAGES; ++stage)
            process_stage(&requests[i], stage, directory);
    }
}

int run_pipeline(Request *requests, size_t count, const char *directory)
{
    Queue queues[STAGES] = {0};
    Worker workers[STAGES];
    pthread_t threads[STAGES];
    size_t initialized = 0, started = 0;
    int error = 0;

    for (; initialized < STAGES; ++initialized) {
        error = queue_init(&queues[initialized]);
        if (error)
            break;
    }
    if (!error) {
        for (; started < STAGES; ++started) {
            Queue *next = started + 1 < STAGES ? &queues[started + 1] : NULL;
            workers[started] = (Worker){&queues[started], next, started, directory};
            error = pthread_create(&threads[started], NULL, work, &workers[started]);
            if (error)
                break;
        }
    }
    if (error) {
        for (size_t stage = 0; stage < initialized; ++stage)
            queue_close(&queues[stage]);
    } else {
        for (size_t i = 0; i < count; ++i) {
            requests[i].submitted = clock_ticks();
            queue_push(&queues[0], &requests[i]);
        }
        queue_close(&queues[0]);
    }
    for (size_t stage = 0; stage < started; ++stage)
        pthread_join(threads[stage], NULL);
    for (size_t stage = 0; stage < initialized; ++stage)
        queue_destroy(&queues[stage]);
    return error;
}
