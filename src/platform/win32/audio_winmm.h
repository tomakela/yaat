#ifndef YAAT_PLATFORM_WIN32_AUDIO_WINMM_H
#define YAAT_PLATFORM_WIN32_AUDIO_WINMM_H

#include "runtime/asset_loader.h"

#include <windows.h>
#include <mmsystem.h>

typedef struct YaatWinmmChannel {
    HWAVEOUT wave_out;
    WAVEHDR header;
    unsigned char *buffer;
    unsigned long buffer_size;
    char logical_path[YAAT_ASSET_MAX_PATH];
    int active;
} YaatWinmmChannel;

typedef struct YaatWinmmAudio {
    YaatAssetStore *store;
    YaatWinmmChannel sound;
    YaatWinmmChannel music;
    int muted;
    int volume_percent;
} YaatWinmmAudio;

void yaat_winmm_audio_init(YaatWinmmAudio *audio, YaatAssetStore *store);
void yaat_winmm_audio_shutdown(YaatWinmmAudio *audio);
int yaat_winmm_audio_play_sound(YaatWinmmAudio *audio, const char *logical_path);
int yaat_winmm_audio_play_music(YaatWinmmAudio *audio, const char *logical_path);
void yaat_winmm_audio_stop_music(YaatWinmmAudio *audio);
void yaat_winmm_audio_set_muted(YaatWinmmAudio *audio, int muted);
void yaat_winmm_audio_toggle_muted(YaatWinmmAudio *audio);
void yaat_winmm_audio_set_volume(YaatWinmmAudio *audio, int volume_percent);
int yaat_winmm_audio_muted(const YaatWinmmAudio *audio);
int yaat_winmm_audio_volume(const YaatWinmmAudio *audio);

#endif
