/* dc/maple/purupuru.h -- Xbox shim: the Dreamcast jump pack, over the Xbox
 * gamepad's two rumble motors (Platform/xbox/kos_maple_xbox.c). */
#ifndef XBOX_DC_MAPLE_PURUPURU_H
#define XBOX_DC_MAPLE_PURUPURU_H

#include <kos/cdefs.h>
#include <dc/maple.h>

__BEGIN_DECLS

typedef union purupuru_effect {
    uint32_t raw;
    struct {
        bool     cont    : 1;
        uint32_t res     : 3;
        uint32_t motor   : 4;
        uint32_t bpow    : 3;
        bool     div     : 1;
        uint32_t fpow    : 3;
        bool     conv    : 1;
        uint8_t  freq;
        uint8_t  inc;
    };
} purupuru_effect_t;

_Static_assert(sizeof(purupuru_effect_t) == 4, "purupuru_effect_t layout");

int purupuru_rumble(maple_device_t *dev, const purupuru_effect_t *effect);
int purupuru_rumble_raw(maple_device_t *dev, uint32_t effect);

__END_DECLS

#endif /* XBOX_DC_MAPLE_PURUPURU_H */
