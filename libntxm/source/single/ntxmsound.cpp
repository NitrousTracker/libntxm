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

#include <cstdlib>
#include <cstring>

extern "C" {
#include "ntxm/demokit.h"
}

#include "ntxm/common.h"
#include "ntxm/ntxmsound.h"
#include "ntxm/ntxmtools.h"
#include "ntxm/player.h"

// #define DEBUG_SOUND

#define TICKS_COUNTER_SHIFT 7
#define SAMPLES_PER_MS_SHIFT 16

#define clamp(v, vmin, vmax)                                                   \
	(((v) < (vmin)) ? (vmin) : ((v > (vmax)) ? (vmax) : (v)))

class SoundEmulator
{
public:
	SoundEmulator();
	void setRenderFrequency(uint32_t frequency);
	void generate(Player *player, int16_t *sample_data, size_t n);

	uint32_t ticks_per_ms = 0;
	uint32_t samples_per_ms = 0;
	uint32_t render_frequency = 0;
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
	uint32_t sub_samples = 0;
	uint32_t ticks_cnt = 0;

	void nextSample(int16_t *buffer);
	int nextPosition(int ch);
};

SoundEmulator::SoundEmulator()
{
	setRenderFrequency(32728);
}

void SoundEmulator::setRenderFrequency(uint32_t frequency)
{
	render_frequency = frequency;
	samples_per_ms = (frequency << 16) / 1000;
	ticks_per_ms =
	    (((NTXM_NDS_BUS_CLOCK >> 1) << TICKS_COUNTER_SHIFT) / frequency);
}

int SoundEmulator::nextPosition(int ch)
{
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

void SoundEmulator::nextSample(int16_t *buffer)
{
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
		if (!playing[i] || !volume[i])
			continue;

		int16_t sample = 0;
		if (resample_linear) {
			int16_t sample1, sample2;

			int sample1pos = position[i];
			int sample2pos = nextPosition(i);
			if (sample2pos < 0)
				sample2pos = sample1pos;

			if (format[i] == NTXMSOUND_FORMAT_8BIT) {
				sample1 = ((const int8_t *)data[i])[sample1pos] << 8;
				sample2 = ((const int8_t *)data[i])[sample2pos] << 8;
			} else {
				sample1 = ((const int16_t *)data[i])[sample1pos >> 1];
				sample2 = ((const int16_t *)data[i])[sample2pos >> 1];
			}

			sample = ((sample1 * timer_tick[i]) +
			          (sample2 * (frequency[i] - timer_tick[i]))) /
			         frequency[i];
		} else {
			if (format[i] == NTXMSOUND_FORMAT_8BIT) {
				sample = ((const int8_t *)data[i])[position[i]] << 8;
			} else {
				sample = ((const int16_t *)data[i])[position[i] >> 1];
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

	buffer[0] = (int16_t)clamp(samples[0], -32768, 32767);
	buffer[1] = (int16_t)clamp(samples[1], -32768, 32767);
}

void SoundEmulator::generate(Player *player, int16_t *sample_data, size_t n)
{
	for (size_t i = 0; i < n; i++, sample_data += 2) {
		sub_samples += (1 << SAMPLES_PER_MS_SHIFT);
		while (sub_samples >= samples_per_ms) {
			player->update(1 * MS_UNIT);
			sub_samples -= samples_per_ms;
		}
		nextSample(sample_data);
	}
}

SoundEmulator emu;

void ntxm_sound_flush_channels()
{
	// does nothing on non-DS platforms
}

void ntxm_sound_set_playback_frequency(int freq)
{
	emu.setRenderFrequency(freq);
}

size_t ntxm_sound_fetch_samples(Player *player, int16_t *sample_data, size_t n)
{
	emu.generate(player, sample_data, n);
	return n;
}

void ntxm_sound_channel_stop(int channel)
{
	emu.playing[channel] = false;
}

bool ntxm_sound_channel_is_playing(int channel)
{
	return emu.playing[channel];
}

void ntxm_sound_channel_set_volume(int channel, int volume)
{
#ifdef DEBUG_SOUND
	printf("ntxmsound: volume  ch %d = %d\n", channel, volume);
#endif
	emu.volume[channel] = volume;
}

void ntxm_sound_channel_set_frequency(int channel, int freq)
{
#ifdef DEBUG_SOUND
	printf("ntxmsound: freq    ch %d = %d\n", channel, freq);
#endif
	emu.frequency[channel] = freq;
}

void ntxm_sound_channel_set_panning(int channel, u32 panning)
{
#ifdef DEBUG_SOUND
	printf("ntxmsound: panning ch %d = %d\n", channel, panning);
#endif
	if (!ntxm_stereo_output)
		panning = 128;
	emu.panning[channel] = panning;
}

void ntxm_sound_channel_set_source(int channel, const void *src,
                                   uint32_t repeat_point, uint32_t length)
{
#ifdef DEBUG_SOUND
	printf("ntxmsound: source  ch %d, len %d\n", channel, length);
#endif
	emu.data[channel] = src;
	emu.repeat_point[channel] = repeat_point;
	emu.length[channel] = repeat_point + length;
}

void ntxm_sound_channel_play(int channel, u32 loop, u32 format, u32 panning,
                             u32 volume)
{
#ifdef DEBUG_SOUND
	printf("ntxmsound: playing ch %d\n", channel);
#endif
	emu.loop[channel] = loop;
	emu.format[channel] = format;
	emu.panning[channel] = panning;
	emu.volume[channel] = volume;
	emu.position[channel] = 0;
	emu.timer_tick[channel] = emu.frequency[channel];
	emu.playing[channel] = emu.length[channel] > 0;
}
