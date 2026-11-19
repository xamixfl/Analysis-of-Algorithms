#include "segmentation.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static uint32_t random32(uint32_t *state)
{
    *state ^= *state << 13; *state ^= *state >> 17; *state ^= *state << 5;
    return *state;
}

void generate_image(uint8_t *a, size_t width, size_t height, uint32_t seed)
{
    uint32_t state = seed ? seed : 1;
    for (size_t y = 0; y < height; ++y)
        for (size_t x = 0; x < width; ++x) {
            double dx = (2.0 * x / width - 1.0);
            double dy = (2.0 * y / height - 1.0);
            int body = dx * dx / 0.72 + dy * dy / 0.92 < 1.0;
            double lx = dx + 0.33, rx = dx - 0.33;
            int lungs = lx * lx / 0.075 + dy * dy / 0.52 < 1.0 ||
                        rx * rx / 0.075 + dy * dy / 0.52 < 1.0;
            unsigned noise = random32(&state) % 40;
            a[y * width + x] = (uint8_t)((body && !lungs ? 165 : 25) + noise);
        }
}

double monotonic_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER count, frequency;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&frequency);
    return (double)count.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (double)time.tv_sec + time.tv_nsec * 1e-9;
#endif
}

unsigned logical_processors(void)
{
#ifdef _WIN32
    return GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (unsigned)n : 1;
#endif
}

uint64_t image_checksum(const uint8_t *a, size_t n)
{
    uint64_t result = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < n; ++i) {
        result ^= a[i]; result *= UINT64_C(1099511628211);
    }
    return result;
}

static int token(FILE *f, char *text, size_t capacity, int *delimiter)
{
    int c;
    do {
        c = fgetc(f);
        if (c == '#') while (c != '\n' && c != EOF) c = fgetc(f);
    } while (c != EOF && isspace((unsigned char)c));
    if (c == EOF) return 0;
    size_t n = 0;
    while (c != EOF && !isspace((unsigned char)c) && c != '#') {
        if (n + 1 == capacity) return 0;
        text[n++] = (char)c;
        c = fgetc(f);
    }
    if (c == '#') while (c != '\n' && c != EOF) c = fgetc(f);
    text[n] = 0;
    if (delimiter) *delimiter = c;
    return 1;
}

static int natural(const char *text, size_t *value)
{
    if (!*text) return 0;
    size_t result = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (!isdigit(*p) || result > (SIZE_MAX - (*p - '0')) / 10) return 0;
        result = result * 10 + (*p - '0');
    }
    if (!result) return 0;
    *value = result; return 1;
}

int pgm_read(const char *path, Image *image)
{
    memset(image, 0, sizeof(*image));
    FILE *f = fopen(path, "rb");
    if (!f) return errno;
    char text[64]; size_t width, height, maxval; int delimiter = 0;
    int ok = token(f, text, sizeof(text), NULL) && strcmp(text, "P5") == 0 &&
        token(f, text, sizeof(text), NULL) && natural(text, &width) &&
        token(f, text, sizeof(text), NULL) && natural(text, &height) &&
        token(f, text, sizeof(text), &delimiter) && natural(text, &maxval);
    if (!ok || maxval != 255 || delimiter == EOF || width > SIZE_MAX / height ||
        width * height > UINT64_MAX / 255) { fclose(f); return EINVAL; }
    /* Treat CRLF as the single header line ending. Do not skip raster bytes. */
    if (delimiter == '\r') {
        int c = fgetc(f); if (c != '\n' && c != EOF) ungetc(c, f);
    }
    size_t n = width * height;
    uint8_t *pixels = malloc(n);
    if (!pixels) { fclose(f); return ENOMEM; }
    if (fread(pixels, 1, n, f) != n || fgetc(f) != EOF || ferror(f)) {
        free(pixels); fclose(f); return EINVAL;
    }
    fclose(f);
    image->width = width; image->height = height; image->pixels = pixels;
    return 0;
}

int pgm_write(const char *path, const Image *image)
{
    FILE *f = fopen(path, "wb");
    if (!f) return errno;
    size_t n = image->width * image->height;
    int ok = fprintf(f, "P5\n%zu %zu\n255\n", image->width, image->height) > 0 &&
             fwrite(image->pixels, 1, n, f) == n;
    if (fclose(f)) ok = 0;
    return ok ? 0 : EIO;
}
