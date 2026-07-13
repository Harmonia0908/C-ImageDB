#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include "common.h"
#include "ppm.h"

#define PPM_MAX_VAL 255

/* Read next non-whitespace, non-comment token from the PPM header.
 * Returns 0 on success, -1 on EOF or format error.
 * Token is written to buf (at most bufsize bytes, always null-terminated). */
static int ppm_read_token(FILE *fp, char *buf, size_t bufsize) {
    int c;
    int truncated = 0;

    if (bufsize == 0) return -1;

    /* Skip whitespace and comments */
    for (;;) {
        c = getc(fp);
        if (c == EOF) return -1;

        if (c == '#') {
            /* Skip to end of line */
            while ((c = getc(fp)) != EOF && c != '\n')
                ;
            if (c == EOF) return -1;
            continue;
        }

        if (!isspace((unsigned char)c))
            break;
    }

    /* Read token */
    {
        size_t i = 0;
        do {
            if (i + 1 < bufsize)
                buf[i++] = (char)c;
            else
                truncated = 1;
            c = getc(fp);
        } while (c != EOF && !isspace((unsigned char)c) && c != '#');

        buf[i] = '\0';

        /* Put back the delimiter for next call (unless it's a comment char) */
        if (c != EOF)
            ungetc(c, fp);
    }

    return truncated ? -1 : 0;
}

/* Consume the single separator between maxval and the binary raster. CRLF is
 * treated as one line ending; other whitespace is consumed exactly once so a
 * legitimate whitespace-valued first pixel byte is never skipped. */
static int ppm_consume_raster_separator(FILE *fp) {
    int c = getc(fp);

    if (c == EOF)
        return -1;
    if (c == '#') {
        while ((c = getc(fp)) != EOF && c != '\n')
            ;
        return c == EOF ? -1 : 0;
    }
    if (!isspace((unsigned char)c))
        return -1;
    if (c == '\r') {
        int next = getc(fp);
        if (next != '\n' && next != EOF)
            ungetc(next, fp);
    }
    return 0;
}

image_t *ppm_read(const char *path) {
    FILE *fp;
    image_t *img;
    char token[32];
    int width, height;
    char *end;
    size_t pixel_bytes;

    if (!path)
        return NULL;

    fp = fopen(path, "rb");
    if (!fp)
        return NULL;

    /* Read magic number */
    if (ppm_read_token(fp, token, sizeof(token)) != 0 || strcmp(token, "P6") != 0) {
        fclose(fp);
        return NULL;
    }

    /* Read width */
    {
        long lw;
        if (ppm_read_token(fp, token, sizeof(token)) != 0) {
            fclose(fp); return NULL;
        }
        errno = 0;
        lw = strtol(token, &end, 10);
        if (errno == ERANGE || end == token || *end != '\0')
            { fclose(fp); return NULL; }
        if (lw <= 0 || lw > (long)INT_MAX || lw > (long)MAX_IMAGE_WIDTH)
            { fclose(fp); return NULL; }
        width = (int)lw;
    }

    /* Read height */
    {
        long lh;
        if (ppm_read_token(fp, token, sizeof(token)) != 0) {
            fclose(fp); return NULL;
        }
        errno = 0;
        lh = strtol(token, &end, 10);
        if (errno == ERANGE || end == token || *end != '\0')
            { fclose(fp); return NULL; }
        if (lh <= 0 || lh > (long)INT_MAX || lh > (long)MAX_IMAGE_HEIGHT)
            { fclose(fp); return NULL; }
        height = (int)lh;
    }

    /* Check pixel count */
    {
        size_t w = (size_t)width, h = (size_t)height;
        if (w > SIZE_MAX / h || w * h > MAX_IMAGE_PIXELS)
            { fclose(fp); return NULL; }
    }

    /* Read max value */
    {
        long lm;
        if (ppm_read_token(fp, token, sizeof(token)) != 0) {
            fclose(fp); return NULL;
        }
        errno = 0;
        lm = strtol(token, &end, 10);
        if (errno == ERANGE || end == token || *end != '\0')
            { fclose(fp); return NULL; }
        if (lm != PPM_MAX_VAL)
            { fclose(fp); return NULL; }
    }

    if (ppm_consume_raster_separator(fp) != 0) {
        fclose(fp);
        return NULL;
    }

    pixel_bytes = (size_t)width * (size_t)height * 3;

    img = image_create(width, height, 3);
    if (!img) {
        fclose(fp);
        return NULL;
    }

    if (fread(img->data, 1, pixel_bytes, fp) != pixel_bytes) {
        image_destroy(img);
        fclose(fp);
        return NULL;
    }

    if (fclose(fp) != 0) {
        image_destroy(img);
        return NULL;
    }
    return img;
}

int ppm_write(const char *path, const image_t *img) {
    FILE *fp;
    size_t data_size;

    if (!path || !image_valid(img) || img->channels != IMAGE_CHANNELS)
        return -1;

    fp = fopen(path, "wb");
    if (!fp)
        return -1;

    if (fprintf(fp, "P6\n%d %d\n%d\n", img->width, img->height,
                PPM_MAX_VAL) < 0) {
        fclose(fp);
        return -1;
    }

    data_size = (size_t)img->width * img->height * 3;
    if (fwrite(img->data, 1, data_size, fp) != data_size) {
        fclose(fp);
        return -1;
    }

    return fclose(fp) == 0 ? 0 : -1;
}
