/* dc/video.h -- Xbox shim: the Dreamcast's video output.
 *
 * The port asks for 640x480 RGB565, interlaced NTSC or VGA by cable
 * (src/gfx/gfx_pvr.c). The Xbox renderer (Platform/xbox/pvr_xbox.c) opens
 * the display itself in pvr_init, from the dashboard's settings: 720p, 480p
 * or 480i, always 60 Hz, which is the Dreamcast's NTSC/VGA rate. So the
 * mode calls are accepted and the cable reads as the one the console is
 * set up for: VGA (progressive) when the dashboard allows 480p, composite
 * otherwise.
 *
 * vram_s / vram_l are KOS's pointers to the framebuffer. The port reads
 * vram_s for its motion-blur effect (gfx_pvr.c capture_framebuffer); here
 * it is a 640x480 RGB565 picture of the last frame shown, refreshed the
 * first time it is read each frame (pvr_xbox.c). */
#ifndef XBOX_DC_VIDEO_H
#define XBOX_DC_VIDEO_H

#include <kos/cdefs.h>

__BEGIN_DECLS

typedef enum vid_display_mode_generic {
    DM_GENERIC_FIRST = 0x1000,
    DM_320x240 = 0x1000,
    DM_640x480,
    DM_800x608,
    DM_256x256,
    DM_768x480,
    DM_768x576,
    DM_GENERIC_LAST = DM_768x576
} vid_display_mode_generic_t;

/* KOS's specific modes (vid_display_mode_t), the ones the port names */
typedef enum vid_display_mode {
    DM_320x240_VGA = 1,
    DM_320x240_NTSC,
    DM_640x480_VGA,
    DM_640x480_NTSC_IL,
    DM_640x480_PAL_IL,
    DM_256x256_PAL_IL,
    DM_768x480_NTSC_IL,
    DM_768x576_PAL_IL,
    DM_768x480_PAL_IL,
    DM_320x240_PAL,
    DM_320x240_VGA_MB,
    DM_320x240_NTSC_MB,
    DM_640x480_VGA_MB,
    DM_640x480_NTSC_IL_MB,
    DM_640x480_PAL_IL_MB,
    DM_256x256_PAL_IL_MB,
    DM_768x480_NTSC_IL_MB,
    DM_768x576_PAL_IL_MB,
    DM_768x480_PAL_IL_MB,
    DM_SENTINEL,
    DM_MODE_COUNT
} vid_display_mode_t;

typedef enum vid_pixel_mode {
    PM_RGB555 = 0,
    PM_RGB565 = 1,
    PM_RGB888P = 2,
    PM_RGB0888 = 3,
    PM_RGB888 = PM_RGB0888
} vid_pixel_mode_t;

/* KOS's cable types */
#define CT_VGA          0
#define CT_NONE         1
#define CT_RGB          2
#define CT_COMPOSITE    3

int vid_check_cable(void);
void vid_set_mode(int dm, vid_pixel_mode_t pm);
void vid_waitvbl(void);
void vid_border_color(int r, int g, int b);
/* 0 blanks the display (KOS turns the video output off), 1 shows it */
void vid_set_enabled(int val);

uint16_t *pvr_xbox_vram_s(void);
#define vram_s (pvr_xbox_vram_s())

__END_DECLS

#endif /* XBOX_DC_VIDEO_H */
