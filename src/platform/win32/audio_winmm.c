#include "platform/win32/audio_winmm.h"

#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>

#define YAAT_AUDIO_LOOP_COUNT 0xffffffffUL

static unsigned short yaat_audio_le16(const unsigned char *p)
{
    return (unsigned short)(p[0] | (p[1] << 8));
}

static unsigned long yaat_audio_le32(const unsigned char *p)
{
    return ((unsigned long)p[0]) | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static void yaat_audio_copy(char *dst, int dst_size, const char *src)
{
    int i;
    if (dst == 0 || dst_size <= 0) return;
    if (src == 0) src = "";
    for (i = 0; i < dst_size - 1 && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static int yaat_audio_parse_wav(const unsigned char *data, unsigned long size,
                                WAVEFORMATEX *format,
                                unsigned long *sample_offset,
                                unsigned long *sample_size)
{
    unsigned long cursor;
    int has_fmt;
    int has_data;

    if (data == 0 || size < 12 || format == 0 || sample_offset == 0 || sample_size == 0) return 0;
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) return 0;
    memset(format, 0, sizeof(*format));
    cursor = 12;
    has_fmt = 0;
    has_data = 0;
    while (cursor + 8 <= size) {
        const unsigned char *chunk;
        unsigned long chunk_size;
        chunk = data + cursor;
        chunk_size = yaat_audio_le32(chunk + 4);
        cursor += 8;
        if (cursor + chunk_size > size) return 0;
        if (memcmp(chunk, "fmt ", 4) == 0 && chunk_size >= 16) {
            format->wFormatTag = yaat_audio_le16(data + cursor);
            format->nChannels = yaat_audio_le16(data + cursor + 2);
            format->nSamplesPerSec = yaat_audio_le32(data + cursor + 4);
            format->nAvgBytesPerSec = yaat_audio_le32(data + cursor + 8);
            format->nBlockAlign = yaat_audio_le16(data + cursor + 12);
            format->wBitsPerSample = yaat_audio_le16(data + cursor + 14);
            format->cbSize = 0;
            has_fmt = 1;
        } else if (memcmp(chunk, "data", 4) == 0) {
            *sample_offset = cursor;
            *sample_size = chunk_size;
            has_data = 1;
        }
        cursor += chunk_size + (chunk_size & 1UL);
    }
    if (!has_fmt || !has_data) return 0;
    if (format->wFormatTag != WAVE_FORMAT_PCM) return 0;
    if (format->nChannels != 1 && format->nChannels != 2) return 0;
    if (format->wBitsPerSample != 8 && format->wBitsPerSample != 16) return 0;
    if (*sample_size == 0) return 0;
    return 1;
}

static void yaat_winmm_channel_stop(YaatWinmmChannel *channel)
{
    if (channel == 0 || !channel->active) return;
    waveOutReset(channel->wave_out);
    waveOutUnprepareHeader(channel->wave_out, &channel->header, sizeof(channel->header));
    waveOutClose(channel->wave_out);
    if (channel->buffer != 0) free(channel->buffer);
    memset(channel, 0, sizeof(*channel));
}

static void yaat_winmm_apply_volume(YaatWinmmAudio *audio, YaatWinmmChannel *channel)
{
    unsigned long scalar;
    unsigned long packed;
    if (audio == 0 || channel == 0 || !channel->active) return;
    scalar = audio->muted ? 0UL : (unsigned long)((audio->volume_percent * 0xffffL) / 100L);
    if (scalar > 0xffffUL) scalar = 0xffffUL;
    packed = scalar | (scalar << 16);
    waveOutSetVolume(channel->wave_out, packed);
}

static int yaat_winmm_channel_play(YaatWinmmAudio *audio, YaatWinmmChannel *channel,
                                   const char *logical_path, int loop)
{
    YaatAssetBuffer asset;
    WAVEFORMATEX format;
    unsigned long sample_offset;
    unsigned long sample_size;
    MMRESULT result;

    if (audio == 0 || channel == 0 || audio->store == 0 || logical_path == 0 || logical_path[0] == '\0') return 0;
    memset(&asset, 0, sizeof(asset));
    if (!yaat_asset_store_load(audio->store, logical_path, &asset)) return 0;
    if (!yaat_audio_parse_wav(asset.data, asset.size, &format, &sample_offset, &sample_size)) {
        yaat_asset_buffer_free(&asset);
        return 0;
    }
    yaat_winmm_channel_stop(channel);
    channel->buffer = (unsigned char *)malloc(sample_size);
    if (channel->buffer == 0) {
        yaat_asset_buffer_free(&asset);
        return 0;
    }
    memcpy(channel->buffer, asset.data + sample_offset, sample_size);
    channel->buffer_size = sample_size;
    yaat_asset_buffer_free(&asset);

    result = waveOutOpen(&channel->wave_out, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        free(channel->buffer);
        memset(channel, 0, sizeof(*channel));
        return 0;
    }
    memset(&channel->header, 0, sizeof(channel->header));
    channel->header.lpData = (LPSTR)channel->buffer;
    channel->header.dwBufferLength = channel->buffer_size;
    if (loop) {
        channel->header.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
        channel->header.dwLoops = YAAT_AUDIO_LOOP_COUNT;
    }
    result = waveOutPrepareHeader(channel->wave_out, &channel->header, sizeof(channel->header));
    if (result == MMSYSERR_NOERROR) result = waveOutWrite(channel->wave_out, &channel->header, sizeof(channel->header));
    if (result != MMSYSERR_NOERROR) {
        waveOutClose(channel->wave_out);
        free(channel->buffer);
        memset(channel, 0, sizeof(*channel));
        return 0;
    }
    channel->active = 1;
    yaat_audio_copy(channel->logical_path, sizeof(channel->logical_path), logical_path);
    yaat_winmm_apply_volume(audio, channel);
    return 1;
}

void yaat_winmm_audio_init(YaatWinmmAudio *audio, YaatAssetStore *store)
{
    if (audio == 0) return;
    memset(audio, 0, sizeof(*audio));
    audio->store = store;
    audio->volume_percent = 100;
}

void yaat_winmm_audio_shutdown(YaatWinmmAudio *audio)
{
    if (audio == 0) return;
    yaat_winmm_channel_stop(&audio->sound);
    yaat_winmm_channel_stop(&audio->music);
}

int yaat_winmm_audio_play_sound(YaatWinmmAudio *audio, const char *logical_path)
{
    return yaat_winmm_channel_play(audio, &audio->sound, logical_path, 0);
}

int yaat_winmm_audio_play_music(YaatWinmmAudio *audio, const char *logical_path)
{
    if (audio == 0 || logical_path == 0 || logical_path[0] == '\0') return 0;
    if (audio->music.active && strcmp(audio->music.logical_path, logical_path) == 0) return 1;
    return yaat_winmm_channel_play(audio, &audio->music, logical_path, 1);
}

void yaat_winmm_audio_stop_music(YaatWinmmAudio *audio)
{
    if (audio == 0) return;
    yaat_winmm_channel_stop(&audio->music);
}

void yaat_winmm_audio_set_muted(YaatWinmmAudio *audio, int muted)
{
    if (audio == 0) return;
    audio->muted = muted != 0;
    yaat_winmm_apply_volume(audio, &audio->sound);
    yaat_winmm_apply_volume(audio, &audio->music);
}

void yaat_winmm_audio_toggle_muted(YaatWinmmAudio *audio)
{
    if (audio == 0) return;
    yaat_winmm_audio_set_muted(audio, !audio->muted);
}

void yaat_winmm_audio_set_volume(YaatWinmmAudio *audio, int volume_percent)
{
    if (audio == 0) return;
    if (volume_percent < 0) volume_percent = 0;
    if (volume_percent > 100) volume_percent = 100;
    audio->volume_percent = volume_percent;
    yaat_winmm_apply_volume(audio, &audio->sound);
    yaat_winmm_apply_volume(audio, &audio->music);
}

int yaat_winmm_audio_muted(const YaatWinmmAudio *audio)
{
    return audio != 0 ? audio->muted : 1;
}

int yaat_winmm_audio_volume(const YaatWinmmAudio *audio)
{
    return audio != 0 ? audio->volume_percent : 0;
}
