#ifndef LAB5_H
#define LAB5_H

#include <stddef.h>
#include <stdint.h>

enum { STAGES = 3, QUEUE_CAPACITY = 2 };

typedef struct {
    size_t width;
    size_t height;
    unsigned char *pixels;
} Image;

typedef struct {
    uint64_t start;
    uint64_t finish;
    int ran;
} Interval;

typedef struct {
    const char *input;
    size_t id;
    Image image;
    uint64_t submitted;
    Interval time[STAGES];
    unsigned threshold;
    const char *error;
} Request;

const char *read_pgm(const char *path, Image *image);
unsigned segment_otsu(Image *image);
const char *write_mask(const char *directory, const Request *request);

uint64_t clock_ticks(void);
double ticks_ms(uint64_t ticks);
void run_sequential(Request *requests, size_t count, const char *directory);
int run_pipeline(Request *requests, size_t count, const char *directory);

#endif
