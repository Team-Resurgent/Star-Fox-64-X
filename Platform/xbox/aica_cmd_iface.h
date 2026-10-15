/* aica_cmd_iface.h -- Xbox shim for KallistiOS's ARM-side aica_cmd_iface.h
 * (kernel/arch/dreamcast/sound/arm/aica_cmd_iface.h, (c)2000-2002 Megan
 * Potter, KOS licence): where the SH-4/ARM command queues and the channel
 * mirror sit in sound RAM. Same values; on the Xbox they are offsets into
 * the emulated sound RAM (Platform/xbox/aica/aica_xbox.h). */
#ifndef __ARM_AICA_CMD_IFACE_H
#define __ARM_AICA_CMD_IFACE_H

#include <arch/types.h>
#include <dc/sound/aica_comm.h>

#define AICA_MEM_CMD_QUEUE  0x010000    /* 32K */
#define AICA_MEM_RESP_QUEUE 0x018000    /* 32K */
#define AICA_MEM_CHANNELS   0x020000    /* 64 * 16*4 = 4K */
#define AICA_MEM_CLOCK      0x021000    /* 4 bytes */
#define AICA_RAM_START      0x030000
#define AICA_RAM_END        0x200000

#define AICA_CHANNEL(x)     (AICA_MEM_CHANNELS + (x) * sizeof(aica_channel_t))

#define AICA_CHANNEL_KEYONEX   0x8000
#define AICA_CHANNEL_KEYONB    0x4000

#endif /* __ARM_AICA_CMD_IFACE_H */
