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

#include "ntxm/ntxmsound.h"
#include "ntxm/common.h"
#include "ntxm/ntxmtools.h"

struct SoundChannelRegs {

	bool
	    stop; // needed so CR can be turned off/on in succession to restart a sample.
	bool
	    start; // needed so CR isn't fully rewritten if only the volume/panning changed.

	// control register
	union {
		u32 cr;
		struct {
			u8 vol;
			u8 vol_divider : 7;
			u8 hold : 1;
			u8 pan;
			u8 duty : 3;
			u8 repeat : 2;
			u8 format : 2;
			u8 status : 1;
		};
	};
	u32 source;
	u16 timer;
	u16 repeat_point;
	u32 length;
};

extern bool ntxm_stereo_output;
SoundChannelRegs buffered_regs[MAX_CHANNELS];

void ntxm_sound_flush_channels()
{
	for (int i = 0; i < MAX_CHANNELS; i++) {
		if (buffered_regs[i].stop) {
			buffered_regs[i].stop = false;
			SCHANNEL_CR(i) = 0;
		}
		SCHANNEL_SOURCE(i) = buffered_regs[i].source;
		SCHANNEL_TIMER(i) = buffered_regs[i].timer;
		SCHANNEL_REPEAT_POINT(i) = buffered_regs[i].repeat_point;
		SCHANNEL_LENGTH(i) = buffered_regs[i].length;
		if (buffered_regs[i].start) {
			buffered_regs[i].start = false;
			SCHANNEL_CR(i) = buffered_regs[i].cr;
		} else {
			SCHANNEL_VOL(i) = buffered_regs[i].vol;
			SCHANNEL_PAN(i) = buffered_regs[i].pan;
		}
	}
}

void ntxm_sound_channel_stop(int channel)
{
	buffered_regs[channel].stop = true;
	buffered_regs[channel].cr = 0;
}

bool ntxm_sound_channel_is_playing(int channel)
{
	return buffered_regs[channel].status;
}

void ntxm_sound_channel_set_volume(int channel, int volume)
{
	buffered_regs[channel].vol = volume;
}

void ntxm_sound_channel_set_frequency(int channel, int freq)
{
	buffered_regs[channel].timer = -freq;
}

void ntxm_sound_channel_set_panning(int channel, u32 panning)
{
	if (!ntxm_stereo_output)
		panning = 128;
	buffered_regs[channel].pan = (panning >> 1);
}

void ntxm_sound_channel_set_source(int channel, const void *src,
                                   u32 repeat_point, u32 length)
{
	buffered_regs[channel].source = (u32)src;
	buffered_regs[channel].repeat_point = repeat_point >> 2;
	buffered_regs[channel].length = length >> 2;
}

void ntxm_sound_channel_play(int channel, u32 loop, u32 format, u32 panning,
                             u32 volume)
{
	buffered_regs[channel].start = true;
	buffered_regs[channel].cr = SCHANNEL_ENABLE | loop | format |
	                            SOUND_PAN(panning >> 1) | SOUND_VOL(volume);
}
