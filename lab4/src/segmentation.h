#ifndef SEGMENTATION_H
#define SEGMENTATION_H
#include <stddef.h>
#include <stdint.h>

#define LEVELS 256
#define MAX_WORKERS 4096

/* Nonempty 8-bit image; buffers must be distinct or identical, not partly overlap.
   Return 0 on success, otherwise an errno/pthread error number. */
unsigned otsu_threshold(const uint64_t histogram[LEVELS], size_t n);
int segment_sequential(const uint8_t *input, uint8_t *output,
                       size_t n, unsigned *threshold);
int segment_parallel(const uint8_t *input, uint8_t *output,
                     size_t n, unsigned q, unsigned *threshold);
void generate_image(uint8_t *image, size_t width, size_t height, uint32_t seed);
double monotonic_seconds(void);
unsigned logical_processors(void);
uint64_t image_checksum(const uint8_t *image, size_t n);

typedef struct { size_t width, height; uint8_t *pixels; } Image;
int pgm_read(const char *path, Image *image);
int pgm_write(const char *path, const Image *image);
#endif
