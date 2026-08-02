#ifdef NT_PLATFORM_3DS
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
#include "ntxm/player.h"
#include <3ds.h>
#include <cstring>

extern Player *player;
static LightLock playerMutex;

#define AUDIO_BUFFER_SAMPLES 1024
#define AUDIO_BUFFER_SIZE (AUDIO_BUFFER_SAMPLES * 4)

static uint32_t *audioBuffer;
static ndspWaveBuf ndspAudioBuffer[2];
static uint32_t ndspNextBlock;

bool NtxmPlayerLock(void)
{
	LightLock_Lock(&playerMutex);
	return true;
}

void NtxmPlayerUnlock(void)
{
	LightLock_Unlock(&playerMutex);
}

static void NtxmNdspCallback(void *userdata)
{
	if (player != NULL) {
		if (ndspAudioBuffer[ndspNextBlock].status == NDSP_WBUF_DONE) {
			if (NtxmPlayerLock()) {
				ntxm_sound_fetch_samples(
				    player, ndspAudioBuffer[ndspNextBlock].data_pcm16,
				    AUDIO_BUFFER_SAMPLES);
				NtxmPlayerUnlock();
				DSP_FlushDataCache(ndspAudioBuffer[ndspNextBlock].data_pcm16,
				                   AUDIO_BUFFER_SIZE);
				ndspChnWaveBufAdd(0, &ndspAudioBuffer[ndspNextBlock]);
				ndspNextBlock = 1 - ndspNextBlock;
			}
		}
	}
}

void CommandSetPlaybackFrequency(u32 freq)
{
	// FIXME: No-op.
}

bool CommandInit()
{
	audioBuffer = (uint32_t *)linearAlloc(AUDIO_BUFFER_SIZE * 2);
	if (!audioBuffer)
		return false;

	ndspInit();
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnSetInterp(0, NDSP_INTERP_NONE);
	ndspChnSetRate(0, NTXMSOUND_SAMPLE_RATE_32K);
	ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
	ndspSetCallback(NtxmNdspCallback, nullptr);

	float mix[12];
	memset(mix, 0, sizeof(mix));
	mix[0] = 1.0;
	mix[1] = 1.0;
	ndspChnSetMix(0, mix);

	memset(ndspAudioBuffer, 0, sizeof(ndspAudioBuffer));
	DSP_FlushDataCache(ndspAudioBuffer, sizeof(ndspAudioBuffer));
	ndspAudioBuffer[0].data_vaddr = &audioBuffer[0];
	ndspAudioBuffer[0].nsamples = AUDIO_BUFFER_SAMPLES;
	ndspAudioBuffer[1].data_vaddr = &audioBuffer[AUDIO_BUFFER_SAMPLES];
	ndspAudioBuffer[1].nsamples = AUDIO_BUFFER_SAMPLES;

	memset(audioBuffer, 0, AUDIO_BUFFER_SIZE * 2);
	ndspChnWaveBufAdd(0, &ndspAudioBuffer[0]);
	ndspChnWaveBufAdd(0, &ndspAudioBuffer[1]);

	ntxm_sound_set_playback_frequency(NTXMSOUND_SAMPLE_RATE_32K);
	ndspNextBlock = 0;

	LightLock_Init(&playerMutex);
	player = new Player(NULL);
	return true;
}

void CommandExit()
{
	Player *player_local = player;
	player = NULL;
	delete player_local;

	ndspExit();
}
#endif
