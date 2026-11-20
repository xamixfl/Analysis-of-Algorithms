#define _POSIX_C_SOURCE 200809L
#include "lab5.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <time.h>
#endif

static uint64_t frequency;

static int clock_init(void)
{
#ifdef _WIN32
    LARGE_INTEGER value;
    if (!QueryPerformanceFrequency(&value))
        return 0;
    frequency = (uint64_t)value.QuadPart;
#else
    struct timespec stamp;
    if (clock_gettime(CLOCK_MONOTONIC, &stamp) != 0)
        return 0;
    frequency = 1000000000;
#endif
    return 1;
}

uint64_t clock_ticks(void)
{
#ifdef _WIN32
    LARGE_INTEGER stamp;
    QueryPerformanceCounter(&stamp);
    return (uint64_t)stamp.QuadPart;
#else
    struct timespec stamp;
    clock_gettime(CLOCK_MONOTONIC, &stamp);
    return (uint64_t)stamp.tv_sec * frequency + (uint64_t)stamp.tv_nsec;
#endif
}

double ticks_ms(uint64_t ticks)
{
    return (double)ticks * 1000 / (double)frequency;
}

static int make_directory(const char *path)
{
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0777);
#endif
}

static int write_trace(const char *directory, const Request *requests,
                       size_t count, uint64_t origin)
{
    size_t capacity = strlen(directory) + sizeof("/trace.csv");
    char *path = malloc(capacity);
    if (!path)
        return 0;
    snprintf(path, capacity, "%s/trace.csv", directory);
    FILE *file = fopen(path, "w");
    if (!file) {
        free(path);
        return 0;
    }
    fputs("id,status,threshold,submitted_ms,read_start_ms,read_end_ms,"
          "segment_start_ms,segment_end_ms,write_start_ms,write_end_ms\n", file);
    for (size_t i = 0; i < count; ++i) {
        const Request *request = &requests[i];
        fprintf(file, "%zu,%s,", request->id, request->error ? request->error : "ok");
        if (request->time[1].ran)
            fprintf(file, "%u", request->threshold);
        fprintf(file, ",%.6f", ticks_ms(request->submitted - origin));
        for (size_t stage = 0; stage < STAGES; ++stage) {
            const Interval *interval = &request->time[stage];
            if (interval->ran)
                fprintf(file, ",%.6f,%.6f", ticks_ms(interval->start - origin),
                        ticks_ms(interval->finish - origin));
            else
                fputs(",,", file);
        }
        fputc('\n', file);
    }
    int good = !ferror(file);
    if (fclose(file) != 0)
        good = 0;
    if (!good)
        remove(path);
    free(path);
    return good;
}

int main(int argc, char **argv)
{
    if (argc < 4 || (strcmp(argv[1], "seq") && strcmp(argv[1], "pipe"))) {
        fprintf(stderr, "Usage: %s seq|pipe NEW_OUTPUT_DIRECTORY INPUT.pgm [...]\n",
                argv[0]);
        return 2;
    }
    if (!clock_init()) {
        fputs("Could not initialize the monotonic clock.\n", stderr);
        return 1;
    }
    const char *directory = argv[2];
    if (make_directory(directory) != 0) {
        perror(directory);
        return 1;
    }
    int pipeline = strcmp(argv[1], "pipe") == 0;
    size_t count = (size_t)argc - 3;
    uint64_t origin = clock_ticks();
    Request *requests = calloc(count, sizeof(*requests));
    if (!requests) {
        fputs("Could not allocate the requests.\n", stderr);
        return 1;
    }
    for (size_t i = 0; i < count; ++i) {
        requests[i].id = i + 1;
        requests[i].input = argv[i + 3];
    }

    int error = 0;
    if (pipeline)
        error = run_pipeline(requests, count, directory);
    else
        run_sequential(requests, count, directory);
    double elapsed = ticks_ms(clock_ticks() - origin);

    if (error) {
        fprintf(stderr, "Could not start the pipeline: %s\n", strerror(error));
        free(requests);
        return 1;
    }
    size_t failed = 0;
    for (size_t i = 0; i < count; ++i) {
        if (requests[i].error) {
            fprintf(stderr, "Request %zu (%s): %s\n", requests[i].id,
                    requests[i].input, requests[i].error);
            ++failed;
        }
    }
    int trace_ok = write_trace(directory, requests, count, origin);
    free(requests);
    if (!trace_ok)
        fputs("Could not write trace.csv.\n", stderr);
    printf("mode=%s requests=%zu successful=%zu elapsed_ms=%.6f\n",
           argv[1], count, count - failed, elapsed);
    return failed || !trace_ok ? 1 : 0;
}
