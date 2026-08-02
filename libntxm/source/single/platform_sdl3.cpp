#ifdef NT_PLATFORM_SDL3
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
#include <SDL3/SDL.h>
#include <cstdio>

extern Player *player;
static SDL_Mutex *playerMutex;
static SDL_AudioStream *audioStream;

bool NtxmPlayerLock(void)
{
	if (player == NULL)
		return false;
	SDL_LockMutex(playerMutex);
	return true;
}

void NtxmPlayerUnlock(void)
{
	SDL_UnlockMutex(playerMutex);
}

void SDLCALL NtxmFetchAudio(void *userdata, SDL_AudioStream *stream,
                            int additional_amount, int total_amount)
{
	if (additional_amount > 0) {
		int16_t *data = SDL_stack_alloc(int16_t, additional_amount >> 1);
		if (data) {
			if (NtxmPlayerLock()) {
				int16_t samples_fetched = ntxm_sound_fetch_samples(
				    player, data, additional_amount >> 2);
				NtxmPlayerUnlock();
				SDL_PutAudioStreamData(stream, data, samples_fetched << 2);
			}
			SDL_stack_free(data);
		}
	}
}

void CommandSetPlaybackFrequency(u32 freq)
{
	NtxmPlayerLock();

	ntxm_sound_set_playback_frequency(freq);
	SDL_DestroyAudioStream(audioStream);
	const SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, (int)freq};
	audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
	                                        &spec, NtxmFetchAudio, NULL);
	SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audioStream));

	NtxmPlayerUnlock();
}

bool CommandInit()
{
	playerMutex = SDL_CreateMutex();
	player = new Player(NULL);

	ntxm_sound_set_playback_frequency(NTXMSOUND_SAMPLE_RATE_32K);
	const SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, NTXMSOUND_SAMPLE_RATE_32K};
	audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
	                                        &spec, NtxmFetchAudio, NULL);
	SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audioStream));

	return true;
}

void CommandExit()
{
	SDL_DestroyAudioStream(audioStream);

	Player *player_local = player;
	player = NULL;
	delete player_local;
	SDL_DestroyMutex(playerMutex);
}
#endif
