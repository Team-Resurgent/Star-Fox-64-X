/* dc/maple.h -- Xbox shim: the Dreamcast's maple bus, over the Xbox's USB
 * game ports (Platform/xbox/kos_maple_xbox.c).
 *
 * The four Xbox controller ports are maple ports A-D. On each:
 *   unit 0  the controller itself (MAPLE_FUNC_CONTROLLER), when a gamepad is
 *           plugged in -- read through XInputGetState;
 *   unit 1  a jump pack (MAPLE_FUNC_PURUPURU) on the same pad, which every
 *           Xbox gamepad carries as its rumble motors (XInputSetState);
 * and on port A only:
 *   unit 2  a memory card (MAPLE_FUNC_MEMCARD) backed by the title's save
 *           game on the hard disk (Platform/xbox/kos_vmu_xbox.c).
 *
 * The structs carry KallistiOS's public fields with their names, so the
 * port's device code reads them unchanged; the private half is not kept. */
#ifndef XBOX_DC_MAPLE_H
#define XBOX_DC_MAPLE_H

#include <kos/cdefs.h>

__BEGIN_DECLS

#define MAPLE_FUNC_PURUPURU     0x00010000
#define MAPLE_FUNC_MOUSE        0x00020000
#define MAPLE_FUNC_CAMERA       0x00080000
#define MAPLE_FUNC_CONTROLLER   0x01000000
#define MAPLE_FUNC_MEMCARD      0x02000000
#define MAPLE_FUNC_LCD          0x04000000
#define MAPLE_FUNC_CLOCK        0x08000000
#define MAPLE_FUNC_MICROPHONE   0x10000000
#define MAPLE_FUNC_ARGUN        0x20000000
#define MAPLE_FUNC_KEYBOARD     0x40000000
#define MAPLE_FUNC_LIGHTGUN     0x80000000
#define MAPLE_FUNC_ANY          0xffffffff

#define MAPLE_PORT_COUNT    4
#define MAPLE_UNIT_COUNT    6

#define MAPLE_EOK       0
#define MAPLE_EFAIL     -1
#define MAPLE_EAGAIN    -2
#define MAPLE_EINVALID  -3
#define MAPLE_ENOTSUPP  -4
#define MAPLE_ETIMEOUT  -5

typedef struct maple_devinfo {
    uint32_t  functions;
    uint32_t  function_data[3];
    uint8_t   area_code;
    uint8_t   connector_direction;
    char      product_name[30];
    char      product_license[60];
    uint16_t  standby_power;
    uint16_t  max_power;
} maple_devinfo_t;

typedef struct maple_device {
    uint8_t         valid;
    int             port;
    int             unit;
    maple_devinfo_t info;
    void           *status;     /* the device's status buffer */
} maple_device_t;

maple_device_t *maple_enum_dev(int p, int u);
maple_device_t *maple_enum_type(int n, uint32_t func);
void *maple_dev_status(maple_device_t *dev);

/* KallistiOS v2.2.1's user attach/detach callbacks (the version the
 * Dreamcast port is built against): one of each per driver, set for every
 * driver serving any of `functions` (0 = all). An attach callback runs once
 * its device is enumerable, a detach callback once it no longer is. */
typedef void (*maple_attach_callback_t)(maple_device_t *dev);
typedef void (*maple_detach_callback_t)(maple_device_t *dev);
void maple_attach_callback(uint32_t functions, maple_attach_callback_t cb);
void maple_detach_callback(uint32_t functions, maple_detach_callback_t cb);

/* Platform/xbox: open the game ports and watch them for pads coming and
 * going. xbox_maple_init is called from main before the game; xbox_maple_poll
 * once a frame (pvr_scene_finish), as the Dreamcast's maple bus is scanned
 * on its vertical blank, and also by every maple_enum_dev
 * (Platform/xbox/kos_maple_xbox.c). */
void xbox_maple_init(void);
void xbox_maple_poll(void);

/* The port's menu reads the pads itself: xbox_pad_buttons and
 * xbox_pad_capture, declared with their XPAD_* bits in xbox_settings.h. */

__END_DECLS

#include <dc/maple/controller.h>

#endif /* XBOX_DC_MAPLE_H */
