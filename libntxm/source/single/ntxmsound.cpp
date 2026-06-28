/*
 * libNTXM - XM Player Library for the Nintendo DS
 *
 *    Copyright (C) 2005-2008 Tobias Weyand (0xtob)
 *                         me@nitrotracker.tobw.net
 *
 */

/***** BEGIN LICENSE BLOCK *****
 *
 * Version: Noncommercial zLib License / GPL 3.0
 *
 * The contents of this file are subject to the Noncommercial zLib License
 * (the "License"); you may not use this file except in compliance with
 * the License. You should have recieved a copy of the license with this package.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 3 or later (the "GPL"),
 * in which case the provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only under the terms of
 * either the GPL, and not to allow others to use your version of this file under
 * the terms of the Noncommercial zLib License, indicate your decision by
 * deleting the provisions above and replace them with the notice and other
 * provisions required by the GPL. If you do not delete the provisions above,
 * a recipient may use your version of this file under the terms of any one of
 * the GPL or the Noncommercial zLib License.
 *
 ***** END LICENSE BLOCK *****/

#ifndef NT_PLATFORM_NDS
#include <cstdlib>
#include <cstring>

extern "C" {
  #include "ntxm/demokit.h"
}

#include "ntxm/common.h"
#include "ntxm/ntxmsound.h"
#include "ntxm/ntxmtools.h"

template <class T>
class RingBuffer {
public:
    RingBuffer() {
        len = 16384;
        data = (T*) ntxm_cmalloc(sizeof(T) * len);
        wr = 0;
        rd = 0;
    }

    ~RingBuffer() {
        ntxm_free(data);
    }

    void push(const T *sample_data, size_t size) {
        while(free() < size) {
            resize(len * 2);
        }

        while (size) {
            size_t max_len = len - wr;
            size_t to_copy = max_len < size ? max_len : size;
            memcpy(data + wr, sample_data, sizeof(T) * to_copy);
            sample_data += to_copy;
            size -= to_copy;
            wr += to_copy;
            if (wr >= len) wr -= len;
        }
    }

    size_t pop(T *sample_data, size_t size) {
        size_t read = 0;
        while (size && wr != rd) {
            size_t max_len = wr >= rd ? (wr - rd) : (len - rd);
            size_t to_copy = max_len < size ? max_len : size;
            memcpy(sample_data, data + rd, sizeof(T) * to_copy);
            sample_data += to_copy;
            size -= to_copy;
            read += to_copy;
            rd += to_copy;
            if (rd >= len) rd -= len;
        }
        return read;
    }

    size_t used() const {
        if (wr >= rd) {
            return wr - rd;
        } else {
            return len + wr - rd;
        }
    }

    size_t free() const {
        return len - used();
    }

private:
    T *data;
    size_t len, wr, rd;

    void resize(size_t newlen) {
        data = (T*) ntxm_crealloc((void*) data, sizeof(T) * newlen);
        if (wr < rd) {
            memmove(data + rd + (newlen - len), data + rd, (len > rd ? (len - rd) : (rd - len)) * sizeof(T));
            rd += (newlen - len);
        }
        len = newlen;
    }
};

#define BUS_CLOCK (33513982)
#define TIMER_FREQ_SHIFT(n, divisor, shift) ((-((BUS_CLOCK >> (shift)) * (divisor)) - ((((n) + 1)) >> 1)) / (n))
#define SOUND_FREQ(n) TIMER_FREQ_SHIFT(n, 1, 1)
#define TICKS_COUNTER_SHIFT 7
#define DEST_TICKS_COUNTER_SHIFT 12

#define clamp(v, vmin, vmax) (((v) < (vmin)) ? (vmin) : ((v > (vmax)) ? (vmax) : (v)))

class SoundEmulator {
public:
    SoundEmulator();
    void update();
    size_t pop(int16_t* sample_data, size_t n);

    RingBuffer<int16_t> buffer;
    uint32_t ticks_per_ms = 0;
    uint32_t dest_ticks_per_ms = 0;
    const void *data[MAX_CHANNELS];
    int position[MAX_CHANNELS];
    uint32_t length[MAX_CHANNELS];
    int frequency[MAX_CHANNELS];
    int timer_tick[MAX_CHANNELS];
    uint32_t repeat_point[MAX_CHANNELS];
    uint8_t format[MAX_CHANNELS];
    uint8_t panning[MAX_CHANNELS];
    uint8_t volume[MAX_CHANNELS];
    uint8_t loop[MAX_CHANNELS];
    bool playing[MAX_CHANNELS];

private:
    bool can_pop = false;
    uint32_t last_ms = 0;
    uint32_t samples_cnt = 0;
    uint32_t ticks_cnt = 0;
    void tick();
    int nextPosition(int ch);
};

SoundEmulator::SoundEmulator() {
    last_ms = getTicks();
}

int SoundEmulator::nextPosition(int ch) {
    uint32_t samplen = format[ch] == NTXMSOUND_FORMAT_16BIT ? 2 : 1;
    int result = position[ch] + samplen;
    if (result >= length[ch]) {
        if (loop[ch] == NTXMSOUND_REPEAT) {
            return repeat_point[ch];
        } else {
            return -1;
        }
    } else {
        return result;
    }
}

void SoundEmulator::tick() {
    int32_t samples[2] = {0};
#ifdef NT_PLATFORM_3DS
    bool resample_linear = false;
#else
    bool resample_linear = true;
#endif

    ticks_cnt += ticks_per_ms;
    uint32_t ticks_elapsed = (ticks_cnt >> TICKS_COUNTER_SHIFT);
    ticks_cnt &= (1 << TICKS_COUNTER_SHIFT) - 1;

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!playing[i] || !volume[i]) continue;

        int16_t sample = 0;
        if (resample_linear) {
            int16_t sample1, sample2;

            int sample1pos = position[i];
            int sample2pos = nextPosition(i);
            if (sample2pos < 0) sample2pos = sample1pos;

            if (format[i] == NTXMSOUND_FORMAT_8BIT) {
                sample1 = ((const int8_t*) data[i])[sample1pos] << 8;
                sample2 = ((const int8_t*) data[i])[sample2pos] << 8;
            } else {
                sample1 = ((const int16_t*) data[i])[sample1pos >> 1];
                sample2 = ((const int16_t*) data[i])[sample2pos >> 1];
            }

            sample = ((sample1 * timer_tick[i]) + (sample2 * (frequency[i] - timer_tick[i]))) / frequency[i];
        } else {
            if (format[i] == NTXMSOUND_FORMAT_8BIT) {
                sample = ((const int8_t*) data[i])[position[i]] << 8;
            } else {
                sample = ((const int16_t*) data[i])[position[i] >> 1];
            }
        }
        sample = ((int)sample * volume[i]) / 128;
        samples[0] += ((int)sample * (256 - panning[i])) / 256;
        samples[1] += ((int)sample * panning[i]) / 256;

        timer_tick[i] -= ticks_elapsed;
        while (timer_tick[i] <= 0) {
            position[i] = nextPosition(i);
            timer_tick[i] += frequency[i];
            if (position[i] < 0) {
                playing[i] = false;
                break;
            }
        }
    }

    int16_t clamped_samples[2] = {
        (int16_t) clamp(samples[0], -32768, 32767),
        (int16_t) clamp(samples[1], -32768, 32767)
    };
    buffer.push(clamped_samples, 2);
}

void SoundEmulator::update() {
    uint32_t ticks = getTicks();
    uint32_t sub_samples = ((ticks - last_ms) * dest_ticks_per_ms);
    samples_cnt += sub_samples;
    uint32_t samples = sub_samples >> DEST_TICKS_COUNTER_SHIFT;
    samples_cnt &= (1 << DEST_TICKS_COUNTER_SHIFT) - 1;
    while(samples--) tick();
    last_ms = ticks;
}

size_t SoundEmulator::pop(int16_t* sample_data, size_t n) {
    if (!can_pop && buffer.free() < 2*n) return 0;

    can_pop = true;
    return buffer.pop(sample_data, n);
}

SoundEmulator emu;

void ntxm_sound_set_playback_frequency(int freq) {
    emu.ticks_per_ms = (((BUS_CLOCK >> 1) << TICKS_COUNTER_SHIFT) / freq);
    emu.dest_ticks_per_ms = (freq << DEST_TICKS_COUNTER_SHIFT) / 1000;
}

size_t ntxm_sound_fetch_samples(int16_t* sample_data, size_t n) {
    emu.update();
    return emu.pop(sample_data, n);
}

void ntxm_sound_channel_stop(int channel) {
    emu.update();
    emu.playing[channel] = false;
}

bool ntxm_sound_channel_is_playing(int channel) {
    emu.update();
    return emu.playing[channel];
}

void ntxm_sound_channel_set_volume(int channel, int volume) {
    emu.update();
    emu.volume[channel] = volume;
}

void ntxm_sound_channel_set_frequency(int channel, int freq) {
    emu.update();
    emu.frequency[channel] = -SOUND_FREQ(freq);
}

void ntxm_sound_channel_set_panning(int channel, u32 panning) {
    emu.update();
    emu.panning[channel] = panning;
}

void ntxm_sound_channel_set_source(int channel, const void *src, uint32_t repeat_point, uint32_t length) {
    emu.update();
    emu.data[channel] = src;
    emu.repeat_point[channel] = repeat_point;
    emu.length[channel] = length;
}

void ntxm_sound_channel_play(int channel, u32 loop, u32 format, u32 panning, u32 volume) {
    emu.update();
    emu.loop[channel] = loop;
    emu.format[channel] = format;
    emu.panning[channel] = panning;
    emu.volume[channel] = volume;
    emu.position[channel] = 0;
    emu.timer_tick[channel] = emu.frequency[channel];
    emu.playing[channel] = true;
}
#endif
