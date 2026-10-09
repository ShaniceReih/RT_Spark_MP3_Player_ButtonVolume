#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

#include "stm32f4xx_hal.h"

class Song;

enum AudioPlaybackState
{
    AUDIO_STOPPED,
    AUDIO_PLAYING,
    AUDIO_PAUSED,
    AUDIO_FINISHED,
    AUDIO_ERROR
};

bool AudioCodec_Probe(void);
bool AudioCodec_Init(void);
void AudioCodec_SetVolume(uint8_t volume);
bool Audio_PlayTestTone(uint32_t durationMs);

void Audio_PlayNote(float frequencyHz, uint32_t durationMs);
void Audio_Silence(uint32_t durationMs);

// Nonblocking synthesized PCM playback of the existing song tables.
bool Audio_StartSong(const Song *song);
bool Audio_PauseSong(void);
bool Audio_ResumeSong(void);
void Audio_StopSong(void);
void Audio_Service(void);
AudioPlaybackState Audio_GetPlaybackState(void);

#endif
