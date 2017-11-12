#ifndef GIFDEC_H
#define GIFDEC_H

#include <stdint.h>

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

GIF *open_gif(const char *fname);
int get_frame(GIF *gif);
void close_gif(GIF *gif);

#endif /* GIFDEC_H */
