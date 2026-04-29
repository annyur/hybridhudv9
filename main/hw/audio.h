/* audio.h — ES8311/ES7210 音频 */
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void audio_init(void);
void audio_play(const int16_t *data, int len);
int  audio_record(int16_t *buf, int len);

#ifdef __cplusplus
}
#endif

#endif