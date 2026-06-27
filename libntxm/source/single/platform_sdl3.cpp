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

#include <cstdio>
#include <SDL3/SDL.h>
#include "ntxm/ntxmsound.h"
#include "ntxm/player.h"

extern Player *player;
static SDL_TimerID playerTimer;
static SDL_Mutex *playerMutex;
static SDL_AudioStream *audioStream;

bool NtxmPlayerLock(void) {
    if (player == NULL)
        return false;
    SDL_LockMutex(playerMutex);
    return true;
}

void NtxmPlayerUnlock(void) {
    SDL_UnlockMutex(playerMutex);
}

static uint32_t NtxmTimerHandler(void *userdata, SDL_TimerID timerID, uint32_t interval) {
    if (NtxmPlayerLock()) {
        player->playTimerHandler();
        NtxmPlayerUnlock();
        return 1;
    } else {
        return 0;
    }
}

void SDLCALL NtxmFetchAudio(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    if (additional_amount > 0) {
        int16_t *data = SDL_stack_alloc(int16_t, additional_amount >> 1);
        if (data) {
            if (NtxmPlayerLock()) {
                int16_t samples_fetched = ntxm_sound_fetch_samples(data, additional_amount >> 1);
                NtxmPlayerUnlock();
                SDL_PutAudioStreamData(stream, data, samples_fetched << 1);
            }
            SDL_stack_free(data);
        }
    }
}

bool CommandInit() {
    playerMutex = SDL_CreateMutex();
    player = new Player(NULL);
    playerTimer = SDL_AddTimer(1, NtxmTimerHandler, NULL);

    ntxm_sound_set_playback_frequency(48000);
    const SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 48000 };
    audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NtxmFetchAudio, NULL);
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audioStream));

    return true;
}

void CommandExit() {
    SDL_DestroyAudioStream(audioStream);

    Player *player_local = player;
    player = NULL;
    delete player;
    SDL_DestroyMutex(playerMutex);
}
#endif
