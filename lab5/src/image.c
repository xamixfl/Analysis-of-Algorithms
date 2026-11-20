#include "lab5.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The separator after the last number belongs to the header once only. */
static int read_number(FILE *file, size_t *value)
{
    int c;
    do {
        c = fgetc(file);
        if (c == '#') {
            do {
                c = fgetc(file);
            } while (c != '\n' && c != '\r' && c != EOF);
        }
    } while (isspace(c));

    if (!isdigit(c))
        return 0;
    size_t number = 0;
    do {
        unsigned digit = (unsigned)(c - '0');
        if (number > (SIZE_MAX - digit) / 10)
            return 0;
        number = number * 10 + digit;
        c = fgetc(file);
    } while (isdigit(c));
    if (!isspace(c))
        return 0;
    *value = number;
    return 1;
}

const char *read_pgm(const char *path, Image *image)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return "open_input";

    size_t width, height, maxval;
    unsigned char *pixels = NULL;
    const char *error = NULL;
    if (fgetc(file) != 'P' || fgetc(file) != '5' || !isspace(fgetc(file)) ||
        !read_number(file, &width) || !read_number(file, &height) ||
        !read_number(file, &maxval) || maxval != 255 ||
        width == 0 || height == 0 || width > SIZE_MAX / height) {
        error = "bad_header";
    } else {
        size_t count = width * height;
        pixels = malloc(count);
        if (!pixels)
            error = "allocation";
        else if (fread(pixels, 1, count, file) != count ||
                 fgetc(file) != EOF || ferror(file))
            error = "bad_raster";
    }
    if (fclose(file) != 0 && !error)
        error = "read_input";
    if (error)
        free(pixels);
    else
        *image = (Image){width, height, pixels};
    return error;
}

unsigned segment_otsu(Image *image)
{
    enum { LEVELS = 256 };
    size_t histogram[LEVELS] = {0};
    size_t count = image->width * image->height;
    for (size_t i = 0; i < count; ++i)
        ++histogram[image->pixels[i]];

    long double total = 0;
    unsigned threshold = 0;
    for (unsigned v = 0; v < LEVELS; ++v) {
        total += (long double)v * histogram[v];
        if (histogram[v])
            threshold = v;
    }

    size_t left = 0;
    long double sum = 0, best = -1;
    for (unsigned v = 0; v < LEVELS - 1; ++v) {
        left += histogram[v];
        sum += (long double)v * histogram[v];
        if (left == 0 || left == count)
            continue;
        long double difference = (long double)count * sum - left * total;
        long double score = difference * difference /
                            ((long double)left * (count - left));
        if (score > best) {
            best = score;
            threshold = v;
        }
    }

    for (size_t i = 0; i < count; ++i)
        image->pixels[i] = image->pixels[i] > threshold ? 255 : 0;
    return threshold;
}

const char *write_mask(const char *directory, const Request *request)
{
    const Image *image = &request->image;
    /* Room for a separator, a decimal size_t, extensions, and terminators. */
    size_t capacity = strlen(directory) + 3 * sizeof(size_t) + 16;
    char *path = malloc(capacity);
    char *temporary = malloc(capacity);
    const char *error = "allocation";
    if (path && temporary) {
        snprintf(path, capacity, "%s/%06zu.pgm", directory, request->id);
        snprintf(temporary, capacity, "%s.part", path);
        FILE *file = fopen(temporary, "wb");
        error = "open_output";
        if (file) {
            size_t count = image->width * image->height;
            int written = fprintf(file, "P5\n%zu %zu\n255\n", image->width,
                                  image->height) >= 0 &&
                          fwrite(image->pixels, 1, count, file) == count;
            int closed = fclose(file) == 0;
            if (written && closed && rename(temporary, path) == 0)
                error = NULL;
            else {
                error = "write_output";
                remove(temporary);
            }
        }
    }
    free(temporary);
    free(path);
    return error;
}
