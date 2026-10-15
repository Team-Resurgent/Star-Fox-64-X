/* dc/sound/sfxmgr.h -- Xbox shim: the channel reservation half of
 * KallistiOS's sound-effect manager (snd_sfxmgr.c), which the stream code
 * takes its channels from. (Platform/xbox/kos_snd_stream_xbox.c) */
#ifndef XBOX_DC_SOUND_SFXMGR_H
#define XBOX_DC_SOUND_SFXMGR_H

#include <kos/cdefs.h>

__BEGIN_DECLS

/* the lowest channel not reserved, now reserved; -1 when all 64 are */
int snd_sfx_chn_alloc(void);
void snd_sfx_chn_free(int chn);

__END_DECLS

#endif /* XBOX_DC_SOUND_SFXMGR_H */
