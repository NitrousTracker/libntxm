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

#ifndef _NTXMSOUND_H_
#define _NTXMSOUND_H_

#include "common.h"

#define NTXMSOUND_FORMAT_ADPCM	    (2<<29)
#define NTXMSOUND_FORMAT_16BIT 		(1<<29)
#define NTXMSOUND_FORMAT_8BIT 		(0)

#if defined(__NDS__)
#define NTXMSOUND_REPEAT SOUND_REPEAT
#define NTXMSOUND_ONE_SHOT SOUND_ONE_SHOT

#if defined(ARM7)
static inline void ntxm_sound_channel_stop(int channel) {
	SCHANNEL_CR(channel) = 0;
}

static inline bool ntxm_sound_channel_is_playing(int channel) {
	return (SCHANNEL_CR(channel) & BIT(31));
}

static inline void ntxm_sound_channel_set_volume(int channel, int volume) {
	SCHANNEL_VOL(channel) = SOUND_VOL(volume);
}

static inline void ntxm_sound_channel_set_frequency(int channel, int freq) {
	SCHANNEL_TIMER(channel) = SOUND_FREQ(freq);
}

static inline void ntxm_sound_channel_set_panning(int channel, u32 panning) {
	u32 control_reg_val = SCHANNEL_CR(channel) & 0xff80ffff;
	SCHANNEL_CR(channel) = control_reg_val | SOUND_PAN(panning >> 1);
}

static inline void ntxm_sound_channel_set_source(int channel, const void *src, uint32_t repeat_point, uint32_t length) {
    SCHANNEL_SOURCE(channel) = (uint32_t) src;
    SCHANNEL_REPEAT_POINT(channel) = repeat_point >> 2;
    SCHANNEL_LENGTH(channel) = length >> 2;
}

static inline void ntxm_sound_channel_play(int channel, u32 loop, u32 format, u32 panning, u32 volume) {
	SCHANNEL_CR(channel) =
		SCHANNEL_ENABLE |
		loop |
		format |
		SOUND_PAN(panning >> 1) |
		SOUND_VOL(volume);
}
#endif
#else
// TODO
#define NTXMSOUND_REPEAT 1
#define NTXMSOUND_ONE_SHOT 0

static inline void ntxm_sound_channel_stop(int channel) {

}

static inline bool ntxm_sound_channel_is_playing(int channel) {
	return false;
}

static inline void ntxm_sound_channel_set_volume(int channel, int volume) {

}

static inline void ntxm_sound_channel_set_frequency(int channel, int freq) {

}

static inline void ntxm_sound_channel_set_panning(int channel, u32 panning) {

}

static inline void ntxm_sound_channel_set_source(int channel, const void *src, uint32_t repeat_point, uint32_t length) {

}

static inline void ntxm_sound_channel_play(int channel, u32 loop, u32 format, u32 panning, u32 volume) {

}
#endif

#endif
