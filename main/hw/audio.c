/* audio.c — ES8311/ES7210 音频 */
#include "audio.h"

void audio_init(void) {}
void audio_play(const int16_t *data, int len) { (void)data; (void)len; }
int audio_record(int16_t *buf, int len) { (void)buf; (void)len; return 0; }
