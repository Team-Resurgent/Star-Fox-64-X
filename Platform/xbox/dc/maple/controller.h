/* dc/maple/controller.h -- Xbox shim: the Dreamcast controller's state, as
 * KallistiOS reports it. Platform/xbox/kos_maple_xbox.c fills it from an
 * Xbox gamepad (see the mapping there). */
#ifndef XBOX_DC_MAPLE_CONTROLLER_H
#define XBOX_DC_MAPLE_CONTROLLER_H

#include <kos/cdefs.h>

__BEGIN_DECLS

#define CONT_C              BIT(0)
#define CONT_B              BIT(1)
#define CONT_A              BIT(2)
#define CONT_START          BIT(3)
#define CONT_DPAD_UP        BIT(4)
#define CONT_DPAD_DOWN      BIT(5)
#define CONT_DPAD_LEFT      BIT(6)
#define CONT_DPAD_RIGHT     BIT(7)
#define CONT_Z              BIT(8)
#define CONT_Y              BIT(9)
#define CONT_X              BIT(10)
#define CONT_D              BIT(11)
#define CONT_DPAD2_UP       BIT(12)
#define CONT_DPAD2_DOWN     BIT(13)
#define CONT_DPAD2_LEFT     BIT(14)
#define CONT_DPAD2_RIGHT    BIT(15)

#define CONT_RESET_BUTTONS  (CONT_A | CONT_B | CONT_X | CONT_Y | CONT_START)

typedef struct cont_state {
    union {
        uint32_t buttons;
        struct {
            uint32_t c: 1;
            uint32_t b: 1;
            uint32_t a: 1;
            uint32_t start: 1;
            uint32_t dpad_up: 1;
            uint32_t dpad_down: 1;
            uint32_t dpad_left: 1;
            uint32_t dpad_right: 1;
            uint32_t z: 1;
            uint32_t y: 1;
            uint32_t x: 1;
            uint32_t d: 1;
            uint32_t dpad2_up: 1;
            uint32_t dpad2_down: 1;
            uint32_t dpad2_left: 1;
            uint32_t dpad2_right: 1;
            uint32_t: 16;
        };
    };
    int ltrig;
    int rtrig;
    int joyx;
    int joyy;
    int joy2x;
    int joy2y;
} cont_state_t;

__END_DECLS

#endif /* XBOX_DC_MAPLE_CONTROLLER_H */
