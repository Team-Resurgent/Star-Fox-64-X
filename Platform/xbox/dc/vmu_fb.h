/* dc/vmu_fb.h -- Xbox shim: KallistiOS's VMU LCD framebuffer.
 *
 * The drawing calls are KOS's own (util/vmu_fb.c, pure software into a
 * 48x32 bitmap) and draw exactly as there. A VMU's screen is a maple LCD
 * device; the Xbox maple bus (kos_maple_xbox.c) has none, so
 * maple_enum_type(n, MAPLE_FUNC_LCD) finds nothing and nothing is presented
 * -- as on a Dreamcast with no VMU in the pad. (Platform/xbox/kos_extra_xbox.c) */
#ifndef XBOX_DC_VMU_FB_H
#define XBOX_DC_VMU_FB_H

#include <kos/cdefs.h>
#include <dc/maple.h>

__BEGIN_DECLS

#define VMU_SCREEN_WIDTH  48
#define VMU_SCREEN_HEIGHT 32

typedef struct vmufb {
    uint32_t data[VMU_SCREEN_WIDTH];
} vmufb_t;

void vmufb_paint_area(vmufb_t *fb, unsigned int x, unsigned int y,
                      unsigned int w, unsigned int h, const uint8_t *data);
void vmufb_clear_area(vmufb_t *fb, unsigned int x, unsigned int y,
                      unsigned int w, unsigned int h);
void vmufb_clear(vmufb_t *fb);
void vmufb_paint_xbm(vmufb_t *fb, unsigned int x, unsigned int y,
                     unsigned int w, unsigned int h, const uint8_t *xbm_data);
/* draws on the device's LCD; the Xbox bus never hands out an LCD device,
 * so a call with one that is not an LCD does nothing */
void vmufb_present(const vmufb_t *fb, maple_device_t *dev);

__END_DECLS

#endif /* XBOX_DC_VMU_FB_H */
