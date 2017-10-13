#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct Palette {
    int size;
    uint8_t colors[0x100 * 3];
} Palette;

typedef struct GCE {
    uint16_t delay;
    uint8_t tindex;
    uint8_t disposal;
    int input;
    int transparency;
} GCE;

typedef struct GIF {
    int fd;
    uint16_t width, height;
    uint16_t loop_count;
    GCE gce;
    Palette *palette;
    Palette lct, gct;
    uint8_t *frame;
} GIF;

uint16_t
read_num(int fd)
{
    uint8_t bytes[2];

    read(fd, bytes, 2);
    return bytes[0] + (((uint16_t) bytes[1]) << 8);
}

GIF *
open_gif(const char *fname)
{
    int fd;
    uint8_t sigver[3];
    uint16_t width, height;
    uint8_t fdsz, bgidx, aspect;
    int gct_sz;
    GIF *gif;

    fd = open(fname, O_RDONLY);
    if (fd == -1) return NULL;
    /* Header */
    read(fd, sigver, 3);
    if (memcmp(sigver, "GIF", 3) != 0) {
        fprintf(stderr, "invalid signature\n");
        goto fail;
    }
    /* Version */
    read(fd, sigver, 3);
    if (memcmp(sigver, "89a", 3) != 0) {
        fprintf(stderr, "invalid version\n");
        goto fail;
    }
    /* Width x Height */
    width  = read_num(fd);
    height = read_num(fd);
    /* FDSZ */
    read(fd, &fdsz, 1);
    /* Presence of GCT */
    if (!(fdsz & 0x80)) {
        fprintf(stderr, "no global color table");
        goto fail;
    }
    /* Color Space's Depth */
    if (((fdsz >> 4) & 7) != 7) {
        fprintf(stderr, "depth of color space is not 8 bits");
        goto fail;
    }
    /* Ignore Sort Flag. */
    /* GCT Size */
    gct_sz = 1 << ((fdsz & 0x07) + 1);
    /* Background Color Index */
    read(fd, &bgidx, 1);
    /* Aspect Ratio */
    read(fd, &aspect, 1);
    /* Create GIF Structure. */
    gif = calloc(1, sizeof(*gif) + width * height);
    if (!gif) goto fail;
    gif->fd = fd;
    gif->width  = width;
    gif->height = height;
    /* Read GCT */
    gif->gct.size = gct_sz;
    read(fd, gif->gct.colors, 3 * gif->gct.size);
    gif->palette = &gif->gct;
    gif->frame = (uint8_t *) &gif[1];
    goto ok;
fail:
    close(fd);
ok:
    return gif;
}

void
discard_sub_blocks(GIF *gif)
{
    uint8_t size;

    do {
        read(gif->fd, &size, 1);
        lseek(gif->fd, size, SEEK_CUR);
    } while (size);
}

/* Ignored extension. */
void
read_plain_text_ext(GIF *gif)
{
    fprintf(stderr, "ignoring plain text extension\n");
    /* Discard plain text metadata. */
    lseek(gif->fd, 13, SEEK_CUR);
    /* Discard plain text sub-blocks. */
    discard_sub_blocks(gif);
}

void
read_graphic_control_ext(GIF *gif)
{
    uint8_t rdit;

    /* Discard block size (always 0x04). */
    lseek(gif->fd, 1, SEEK_CUR);
    read(gif->fd, &rdit, 1);
    gif->gce.disposal = (rdit >> 2) & 3;
    gif->gce.input = rdit & 2;
    gif->gce.transparency = rdit & 1;
    gif->gce.delay = read_num(gif->fd);
    read(gif->fd, &gif->gce.tindex, 1);
    /* Skip block terminator. */
    lseek(gif->fd, 1, SEEK_CUR);
}

/* Ignored extension. */
void
read_comment_ext(GIF *gif)
{
    fprintf(stderr, "ignoring comment extension\n");
    /* Discard comment sub-blocks. */
    discard_sub_blocks(gif);
}

void
read_application_ext(GIF *gif)
{
    char app_id[8];
    char app_auth_code[3];

    /* Discard block size (always 0x0B). */
    lseek(gif->fd, 1, SEEK_CUR);
    /* Application Identifier. */
    read(gif->fd, app_id, 8);
    /* Application Authentication Code. */
    read(gif->fd, app_auth_code, 3);
    if (!strncmp(app_id, "NETSCAPE", sizeof(app_id))) {
        /* Discard block size (0x03) and constant byte (0x01). */
        lseek(gif->fd, 2, SEEK_CUR);
        gif->loop_count = read_num(gif->fd);
        /* Skip block terminator. */
        lseek(gif->fd, 1, SEEK_CUR);
    } else {
        fprintf(stderr, "ignoring application extension: %.*s\n",
                (int) sizeof(app_id), app_id);
        discard_sub_blocks(gif);
    }
}

void
read_ext(GIF *gif)
{
    uint8_t label;

    read(gif->fd, &label, 1);
    switch (label) {
    case 0x01:
        read_plain_text_ext(gif);
        break;
    case 0xF9:
        read_graphic_control_ext(gif);
        break;
    case 0xFE:
        read_comment_ext(gif);
        break;
    case 0xFF:
        read_application_ext(gif);
        break;
    default:
        fprintf(stderr, "unknown extension: %02X\n", label);
    }
}

void
read_image_data(GIF *gif)
{
    uint8_t code_size;

    read(gif->fd, &code_size, 1);
    /* TODO: uncompress raster data. */
    discard_sub_blocks(gif);
}

void
read_image(GIF *gif)
{
    uint16_t x, y, w, h;
    uint8_t fisrz;
    int interlace;

    /* Image Descriptor. */
    x = read_num(gif->fd);
    y = read_num(gif->fd);
    w = read_num(gif->fd);
    h = read_num(gif->fd);
    read(gif->fd, &fisrz, 1);
    interlace = fisrz & 0x40;
    /* Ignore Sort Flag. */
    /* Local Color Table? */
    if (fisrz & 0x80) {
        /* Read LCT */
        gif->lct.size = 1 << ((fisrz & 0x07) + 1);
        read(gif->fd, gif->lct.colors, 3 * gif->lct.size);
        gif->palette = &gif->lct;
    } else
        gif->palette = &gif->gct;
    /* Image Data. */
    read_image_data(gif);
}

/* Return 1 if got a frame; 0 if got GIF trailer; -1 if error. */
int
get_frame(GIF *gif)
{
    char sep;

    read(gif->fd, &sep, 1);
    while (sep != ',') {
        if (sep == ';')
            return 0;
        if (sep == '!')
            read_ext(gif);
        else return -1;
        read(gif->fd, &sep, 1);
    }
    read_image(gif);
    return 1;
}

int
main()
{
    int nframes = 0;
    GIF *gif = open_gif("example.gif");
    if (gif) {
        printf("%ux%u\n", gif->width, gif->height);
        printf("%d colors\n", gif->palette->size);
        while (get_frame(gif)) nframes++;
        close(gif->fd);
        free(gif);
        printf("%d frames\n", nframes);
    }
    return 0;
}
