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

#include <cstring>
#include <3ds.h>
#include "ntxm/ntxmsound.h"
#include "ntxm/player.h"

extern Player *player;
static Handle playerTimer;
static Thread playerThread;
static LightLock playerMutex;

#define AUDIO_SAMPLE_RATE 32728
#define AUDIO_BUFFER_SAMPLES 2048
#define AUDIO_BUFFER_SIZE (AUDIO_BUFFER_SAMPLES * 4)

static uint32_t *audioBuffer;
static ndspWaveBuf ndspAudioBuffer[2];
static uint32_t ndspNextBlock;

bool NtxmPlayerLock(void) {
    LightLock_Lock(&playerMutex);
    return true;
}

void NtxmPlayerUnlock(void) {
    LightLock_Unlock(&playerMutex);
}

static void NtxmTimerThread(void *userdata) {
    while (player != NULL) {
        if (NtxmPlayerLock()) {
            if (ndspAudioBuffer[ndspNextBlock].status == NDSP_WBUF_DONE) {
                ntxm_sound_fetch_samples(ndspAudioBuffer[ndspNextBlock].data_pcm16, AUDIO_BUFFER_SAMPLES * 2);
                DSP_FlushDataCache(ndspAudioBuffer[ndspNextBlock].data_pcm16, AUDIO_BUFFER_SIZE);
                ndspChnWaveBufAdd(0, &ndspAudioBuffer[ndspNextBlock]);
                ndspNextBlock = 1 - ndspNextBlock;
            }

            player->playTimerHandler();
            NtxmPlayerUnlock();
        }
        svcWaitSynchronization(playerTimer, 10000000LL);
    }
}

bool CommandInit() {
    audioBuffer = (uint32_t*) linearAlloc(AUDIO_BUFFER_SIZE * 2);
    if (!audioBuffer)
        return false;

    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_NONE);
    ndspChnSetRate(0, AUDIO_SAMPLE_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0;
    mix[1] = 1.0;
    ndspChnSetMix(0, mix);

    memset(ndspAudioBuffer, 0, sizeof(ndspAudioBuffer));
    ndspAudioBuffer[0].data_vaddr = &audioBuffer[0];
    ndspAudioBuffer[0].nsamples = AUDIO_BUFFER_SAMPLES;
    ndspAudioBuffer[1].data_vaddr = &audioBuffer[AUDIO_BUFFER_SAMPLES];
    ndspAudioBuffer[1].nsamples = AUDIO_BUFFER_SAMPLES;

    memset(audioBuffer, 0, AUDIO_BUFFER_SIZE * 2);
    ndspChnWaveBufAdd(0, &ndspAudioBuffer[0]);
    ndspChnWaveBufAdd(0, &ndspAudioBuffer[1]);

    ntxm_sound_set_playback_frequency(AUDIO_SAMPLE_RATE);
    ndspNextBlock = 0;

    LightLock_Init(&playerMutex);
    player = new Player(NULL);
    svcCreateTimer(&playerTimer, RESET_PULSE);
    svcSetTimer(playerTimer, 1000000LL, 1000000LL);
    playerThread = threadCreate(NtxmTimerThread, 0, (24 * 1024), 0x20, -2, true);
    return true;
}

void CommandExit() {
    Player *player_local = player;
    player = NULL;
    delete player_local;
    threadJoin(playerThread, U64_MAX);
    svcCloseHandle(playerTimer);

    ndspExit();
}
#endif
