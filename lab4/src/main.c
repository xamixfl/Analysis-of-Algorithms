#include "segmentation.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse(const char *text, size_t *value)
{
    if (!*text) return 0;
    size_t result = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9' || result > (SIZE_MAX - (*p - '0')) / 10)
            return 0;
        result = result * 10 + (*p - '0');
    }
    if (!result) return 0;
    *value = result; return 1;
}

static int error(int rc)
{
    fprintf(stderr, "Error %d: %s\n", rc, strerror(rc));
    return 1;
}

static int benchmark(const char *path, unsigned repetitions)
{
    unsigned k = logical_processors(), configs[32] = {0}, count = 1;
    if (!k || k > MAX_WORKERS / 8) return error(EINVAL);
    unsigned limit = 8 * k;
    for (unsigned q = 1; q <= limit; q *= 2) configs[count++] = q;
    if (configs[count - 1] != limit) configs[count++] = limit;
    FILE *f = fopen(path, "w");
    if (!f) return error(errno);
    fprintf(f, "side,n,repeat,mode,q,seconds,threshold,checksum\n");
    const size_t sizes[] = {256, 512, 1024, 2048, 4096};
    for (unsigned z = 0; z < sizeof(sizes)/sizeof(sizes[0]); ++z) {
        size_t side = sizes[z], n = side * side;
        uint8_t *in = malloc(n), *out = malloc(n), *reference = malloc(n);
        if (!in || !out || !reference) {
            free(in); free(out); free(reference); fclose(f); return error(ENOMEM);
        }
        generate_image(in, side, side, 20261112);
        unsigned expected = 0;
        segment_sequential(in, reference, n, &expected);
        uint64_t checksum = image_checksum(reference, n);
        /* One unrecorded warmup for every configuration. */
        for (unsigned c = 0; c < count; ++c) {
            unsigned t = 0;
            int rc = configs[c] ? segment_parallel(in, out, n, configs[c], &t)
                                : segment_sequential(in, out, n, &t);
            if (rc || t != expected || memcmp(out, reference, n)) {
                free(in); free(out); free(reference); fclose(f); return error(rc ? rc : EDOM);
            }
        }
        /* Rotate configuration order, so each receives different positions. */
        for (unsigned r = 0; r < repetitions; ++r)
            for (unsigned position = 0; position < count; ++position) {
                unsigned q = configs[(position + r) % count], t = 0;
                double start = monotonic_seconds();
                int rc = q ? segment_parallel(in, out, n, q, &t)
                           : segment_sequential(in, out, n, &t);
                double elapsed = monotonic_seconds() - start;
                if (rc || t != expected || memcmp(out, reference, n)) {
                    free(in); free(out); free(reference); fclose(f); return error(rc ? rc : EDOM);
                }
                fprintf(f, "%zu,%zu,%u,%s,%u,%.9f,%u,%llu\n", side, n, r,
                    q ? "parallel" : "sequential", q, elapsed, t,
                    (unsigned long long)checksum);
            }
        fflush(f);
        fprintf(stderr, "Measured %zux%zu: %u configurations x %u repeats\n",
                side, side, count, repetitions);
        free(in); free(out); free(reference);
    }
    return fclose(f) ? error(EIO) : 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "info") == 0) {
        printf("logical_processors=%u\n", logical_processors()); return 0;
    }
    if (argc == 4 && strcmp(argv[1], "bench") == 0) {
        size_t repetitions;
        if (!parse(argv[3], &repetitions) || repetitions > 10000) return error(EINVAL);
        return benchmark(argv[2], (unsigned)repetitions);
    }
    if (argc == 6 && strcmp(argv[1], "generate") == 0) {
        size_t w, h, seed;
        if (!parse(argv[2], &w) || !parse(argv[3], &h) ||
            !parse(argv[4], &seed) || seed > UINT32_MAX || w > SIZE_MAX / h)
            return error(EINVAL);
        Image image = {w, h, malloc(w * h)};
        if (!image.pixels) return error(ENOMEM);
        generate_image(image.pixels, w, h, (uint32_t)seed);
        int rc = pgm_write(argv[5], &image); free(image.pixels);
        return rc ? error(rc) : 0;
    }
    int seq = argc == 5 && strcmp(argv[1], "segment") == 0 && strcmp(argv[2], "seq") == 0;
    int par = argc == 6 && strcmp(argv[1], "segment") == 0 && strcmp(argv[2], "par") == 0;
    if (seq || par) {
        size_t q = 0;
        if (par && (!parse(argv[3], &q) || q > MAX_WORKERS)) return error(EINVAL);
        Image image;
        int rc = pgm_read(argv[par ? 4 : 3], &image);
        if (rc) return error(rc);
        size_t n = image.width * image.height;
        uint8_t *out = malloc(n);
        if (!out) { free(image.pixels); return error(ENOMEM); }
        unsigned threshold = 0;
        rc = par ? segment_parallel(image.pixels, out, n, (unsigned)q, &threshold)
                 : segment_sequential(image.pixels, out, n, &threshold);
        free(image.pixels); image.pixels = out;
        if (!rc) rc = pgm_write(argv[par ? 5 : 4], &image);
        if (!rc) printf("threshold=%u checksum=%llu\n", threshold,
                       (unsigned long long)image_checksum(out, n));
        free(out); return rc ? error(rc) : 0;
    }
    fprintf(stderr, "Usage:\n  segment.exe info\n"
        "  segment.exe generate WIDTH HEIGHT SEED OUTPUT.pgm\n"
        "  segment.exe segment seq INPUT.pgm OUTPUT.pgm\n"
        "  segment.exe segment par Q INPUT.pgm OUTPUT.pgm\n"
        "  segment.exe bench OUTPUT.csv REPEATS\n");
    return 1;
}
