#ifndef GIFDEC_H
#define GIFDEC_H

#include <stdint.h>

typedef struct gd_Palette {
    int size;
    uint8_t colors[0x100 * 3];
} gd_Palette;

typedef struct gd_GCE {
    uint16_t delay;
    uint8_t tindex;
    uint8_t disposal;
    int input;
    int transparency;
} gd_GCE;

typedef struct gd_GIF {
    int fd;
    uint16_t width, height;
    uint16_t loop_count;
    gd_GCE gce;
    gd_Palette *palette;
    gd_Palette lct, gct;
    uint8_t *frame;
} gd_GIF;

gd_GIF *gd_open_gif(const char *fname);
int gd_get_frame(gd_GIF *gif);
void gd_close_gif(gd_GIF *gif);

#endif /* GIFDEC_H */
