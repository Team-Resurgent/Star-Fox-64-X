/* dc/sound/stream.h -- Xbox shim: KallistiOS's sound streams (dc/sound/
 * stream.h, KOS licence), over the emulated AICA. The implementation is
 * KOS's snd_stream.c (Platform/xbox/kos_snd_stream_xbox.c): ring buffers in
 * sound RAM, looped by the stock firmware's channels, refilled by
 * snd_stream_poll from the user's callback. */
#ifndef XBOX_DC_SOUND_STREAM_H
#define XBOX_DC_SOUND_STREAM_H

#include <kos/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

#define SND_STREAM_MAX 4
#define SND_STREAM_BUFFER_MAX_PCM16 (128 << 10)
#define SND_STREAM_BUFFER_MAX_PCM8  (64 << 10)
#define SND_STREAM_BUFFER_MAX_ADPCM (32 << 10)            /* v2.2.1 */
#define SND_STREAM_BUFFER_MAX       (64 << 10)

typedef int snd_stream_hnd_t;
#define SND_STREAM_INVALID -1

typedef void *(*snd_stream_callback_t)(snd_stream_hnd_t hnd, int smp_req, int *smp_recv);
typedef size_t (*snd_stream_callback_direct_t)(snd_stream_hnd_t hnd, uintptr_t left,
                                               uintptr_t right, size_t size_req);
typedef void (*snd_stream_filter_t)(snd_stream_hnd_t hnd, void *obj, int hz,
                                    int channels, void **buffer, int *samplecnt);

void snd_stream_set_callback(snd_stream_hnd_t hnd, snd_stream_callback_t cb);
void snd_stream_set_callback_direct(snd_stream_hnd_t hnd, snd_stream_callback_direct_t cb);
void snd_stream_set_userdata(snd_stream_hnd_t hnd, void *d);
void *snd_stream_get_userdata(snd_stream_hnd_t hnd);
void snd_stream_filter_add(snd_stream_hnd_t hnd, snd_stream_filter_t filtfunc, void *obj);
void snd_stream_filter_remove(snd_stream_hnd_t hnd, snd_stream_filter_t filtfunc, void *obj);

int snd_stream_init(void);
int snd_stream_init_ex(int channels, size_t buffer_size);
void snd_stream_shutdown(void);
snd_stream_hnd_t snd_stream_alloc(snd_stream_callback_t cb, int bufsize);
snd_stream_hnd_t snd_stream_reinit(snd_stream_hnd_t hnd, snd_stream_callback_t cb);
void snd_stream_destroy(snd_stream_hnd_t hnd);
void snd_stream_queue_enable(snd_stream_hnd_t hnd);
void snd_stream_queue_disable(snd_stream_hnd_t hnd);
void snd_stream_start(snd_stream_hnd_t hnd, uint32_t freq, int st);
void snd_stream_start_pcm8(snd_stream_hnd_t hnd, uint32_t freq, int st);
void snd_stream_start_adpcm(snd_stream_hnd_t hnd, uint32_t freq, int st);
void snd_stream_queue_go(snd_stream_hnd_t hnd);
void snd_stream_stop(snd_stream_hnd_t hnd);
int snd_stream_poll(snd_stream_hnd_t hnd);
void snd_stream_volume(snd_stream_hnd_t hnd, int vol);
void snd_stream_pan(snd_stream_hnd_t hnd, int left_pan, int right_pan);

/* KOS's channel splitters (sound/snd_pcm_split.s), in C */
void snd_pcm16_split(uint32_t *data, uint32_t *left, uint32_t *right, size_t size);
void snd_pcm8_split(uint32_t *data, uint32_t *left, uint32_t *right, size_t size);
void snd_adpcm_split(uint32_t *data, uint32_t *left, uint32_t *right, size_t size);

__END_DECLS

#endif /* XBOX_DC_SOUND_STREAM_H */
