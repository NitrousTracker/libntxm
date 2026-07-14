/*
 * Copyright (c) 2020-2024, Olav Sørensen
 * Copyright (c) 2026, Adrian "asie" Siekierka
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>

extern "C" {
#include "../common/tables.h"
#include "ntxm/demokit.h"
}

#include "ntxm/fifocommand.h"
#include "ntxm/ntxmsound.h"
#include "ntxm/ntxmtools.h"
#include "ntxm/player.h"
#include "ntxm/song.h"

enum // voice flags
{
	IS_Vol = 1,
	IS_Period = 2,
	IS_NyTon = 4,
	IS_Pan = 8,
	IS_QuickVol = 16,
	IS_NtxmVolFade = 32
};

#define TAG_SONG 253
#define TAG_SAMPLE 254
#define TAG_NONE 255

#define USE_VOLUME_RAMPING
#define QUICK_VOL_FADE_MS 10

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

Player::Player(void (*_playTimerListener)(void))
    : playing(false), patternLoop(false), playTimerListener(_playTimerListener)
{
#ifdef NT_PLATFORM_NDS
	// FIXME: Move out of Player
	demoInit();
#endif
	currMs = nextPlayerMs = 0;
	PMPIgnoreMute = false;
	PMPSampleOverride = nullptr;
	setSong(nullptr);
}

static inline uint8_t soundGetVolume(uint16_t vol)
{
	if (vol > 0)
		vol--; // 8bb: 0..256 -> 0..255 ( FT2 does this to prevent mul overflow in updateVolume() )
	return vol >> 1;
}

void Player::startSongChannel(int c, stmTyp *ch, Sample *s, int smpOffset)
{
	if (!s || (!PMPIgnoreMute && song && song->channelMuted(c))) {
		ntxm_sound_channel_stop(c);
		ch->ntxmTag = TAG_NONE;
		return;
	}

	ntxm_sound_channel_set_frequency(
	    c, ntxmGetFrequencyValue(ch->outPeriod, !song || song->linear));
	s->play(c, ch->finalPan, 0, smpOffset);
	ch->ntxmTag = TAG_SONG;
}

void Player::updateMsPerTick(void)
{
	u8 bpm = state.bpm;
	if (!bpm && song)
		bpm = song->bpm;
	if (!bpm)
		bpm = 125;
	msPerTick = (2500 * MS_UNIT) / bpm;
}

#ifdef NT_PLATFORM_NDS
void Player::playTimerHandler()
{
	update(getSubMsDelta());
}
#endif

void Player::tryEarlyVolumeRamps(void)
{
#ifdef USE_VOLUME_RAMPING
	// Is this the last tick before the next row?
	if (state.timer == 1 && state.pattDelTime2 == 0) {

		// Check if, for any of the active channels, a new note starts in the next row.
		for (int c = 0; c < song->n_channels; c++) {
			stmTyp *ch = &stm[c];
			const Cell *p = &song->getPattern(state.pattNr)[c][state.pattPos];

			bool anyNote = p->note != EMPTY_NOTE && p->note != STOP_NOTE &&
			               p->instrument != NO_INSTRUMENT;
			bool anyPortaFx =
			    (p->volume & 0xF0) == 0xF0 || p->effect == 3 || p->effect == 5;
			bool anyRetrigFx = p->effect == 27;
			u32 delayTicks = 0;
			if (p->effect == 0x0E) {
				if (p->effect_param >= 0xD1 && p->effect_param <= 0xDF) {
					delayTicks = p->effect_param - 0xD0;
				} else if (p->effect_param == 0x90) {
					anyRetrigFx = true;
				}
			}
			if (anyNote && !anyPortaFx && !anyRetrigFx) {
				// If so, fade out to avoid a click.
				ch->ntxmStartVol = ch->ntxmCurVol;
				ch->ntxmEndVol = 0;
				ch->ntxmRampTimer = (msPerTick * (1 + delayTicks)) / MS_UNIT;
				ch->ntxmRampDuration = QUICK_VOL_FADE_MS;
				ch->ntxmEarlyRamp = true;
			}
		}
	}
#endif
}

void Player::update(s32 msDelta)
{
	if (msDelta <= 0)
		return;
	currMs += msDelta;

	// Run FT2 player routine
	if (playing) {
		while ((currMs - nextPlayerMs) <= INT32_MAX) {
			mainPlayer();
			tryEarlyVolumeRamps();
			nextPlayerMs += msPerTick;
		}
	}

	// Synchronize channels
	for (int c = 0; c < MAX_CHANNELS; c++) {
		stmTyp *ch = &stm[c];

		if (ch->ntxmTag == TAG_SONG && song->channelMuted(c)) {
			ntxm_sound_channel_stop(c);
			ch->ntxmTag = TAG_NONE;
			ch->status = 0;
			continue;
		}

		const uint8_t status = ch->status;
		if (!status)
			continue;
#ifdef USE_VOLUME_RAMPING
		ch->status = status & IS_NtxmVolFade;
#else
		ch->status = 0;
#endif

		if ((status & IS_Vol) && !ch->ntxmEarlyRamp) {
#ifdef USE_VOLUME_RAMPING
			ch->ntxmStartVol = ch->ntxmCurVol;
			ch->ntxmEndVol = ch->finalVol;

			if (!(status & IS_QuickVol)) {
				ch->ntxmRampDuration =
				    msPerTick / MS_UNIT; // integer part only.
				ch->ntxmRampTimer = ch->ntxmRampDuration;
			} else if (ch->finalVol == 0 && !ch->envSustainActive) {
				// allow volume ramping
				ch->ntxmRampTimer = QUICK_VOL_FADE_MS;
				ch->ntxmRampDuration = QUICK_VOL_FADE_MS;
			} else {
				// no ramping, snap straight to target volume
				ch->ntxmRampTimer = 0;
				ch->ntxmRampDuration = 10;
			}

			ch->status = IS_NtxmVolFade;
#else
			ntxm_sound_channel_set_volume(c, soundGetVolume(ch->finalVol));
#endif
		}

		if (status & IS_Period) {
			ntxm_sound_channel_set_frequency(
			    c, ntxmGetFrequencyValue(ch->finalPeriod,
			                             !song || song->getLinear()));
		}

		if (status & IS_Pan) {
			ntxm_sound_channel_set_panning(c, ch->finalPan);
		}

#ifdef USE_VOLUME_RAMPING
		// Calculate fades
		if (status & IS_NtxmVolFade) {
			int curVol;
			int timer = ch->ntxmRampTimer;
			int duration = ch->ntxmRampDuration;
			int start = ch->ntxmStartVol;
			int end = ch->ntxmEndVol;

			if (timer > duration) {
				curVol = start;
				timer -= 1;
			} else if (timer > 0) {
				int mix = duration - timer;
				curVol = start + ((end - start) * mix) / duration;
				timer -= 1;
			} else {
				curVol = end;
				ch->status = 0;
			}

			ch->ntxmRampTimer = timer;
			ch->ntxmCurVol = curVol;
			ntxm_sound_channel_set_volume(c, soundGetVolume(curVol));
		}
#endif
	}

	ntxm_sound_flush_channels();

	/* if(playTimerListener) {
        playTimerListener();
    } */
}

void Player::play(int potpos, int row, bool repeat)
{
	stopVoices();

	state.globVol = 64;
	state.pattDelTime = state.pattDelTime2 = 0; // 8bb: added these

	setPos(potpos, row);
	playing = true;
	songLoop = repeat;
	nextPlayerMs = currMs;
	onSongSpeedChanged();
}

void Player::onSongSpeedChanged(void)
{
	if (!playing) {
		return;
	}

	updateMsPerTick();
	state.speed = song->speed;
}

void Player::stop(void)
{
	if (!playing) {
		return;
	}

	stopVoices();
	playing = false;
}

void Player::playNote(int note, int volume, int channel, int instidx)
{
	if (channel >= MAX_CHANNELS) {
		return;
	}

	Cell cell;
	cell.note = note;
	cell.instrument = instidx;
	cell.volume = volume;
	cell.effect = NO_EFFECT;
	cell.effect_param = 0;

	PMPTmpActiveChannel = channel;
	PMPIgnoreMute = true;
	getNewNote(&stm[channel], &cell);
	fixaEnvelopeVibrato(&stm[channel]);
	PMPIgnoreMute = false;
}

void Player::stopAllNotes(int note, int instidx)
{
	stmTyp *ch = stm;
	for (uint8_t i = 0; i < MAX_CHANNELS; i++, ch++)
		if (ch->tonNr == note && ch->instrNr == instidx)
			stopChannel(i);
}

void Player::playSample(Sample *sample, int note, int volume, int channel)
{
	if (channel >= MAX_CHANNELS) {
		return;
	}

	stmTyp *ch = &stm[channel];
	PMPSampleOverride = sample;
	PMPIgnoreMute = true;
	PMPTmpActiveChannel = channel;
	ch->instrNr = NO_INSTRUMENT;
	startTone(note, 0, 0, ch);
	retrigVolume(ch);
	ch->finalVol = ch->outVol;
	ch->finalPeriod = ch->outPeriod;
	PMPSampleOverride = nullptr;
	PMPIgnoreMute = false;
	stm[channel].ntxmTag = TAG_SAMPLE;
}

void Player::stopChannel(int channel)
{
	if (channel >= MAX_CHANNELS) {
		return;
	}

	resetVoice(&stm[channel]);
	ntxm_sound_channel_stop(channel);
}

int Player::getChannelForTag(u16 tag)
{
	int i;
	// First look for existing channels using this tag, just in case.
	for (i = 0; i < MAX_CHANNELS; i++) {
		if (stm[i].ntxmTag == tag) {
			return i;
		}
	}
	// Look for inactive channels.
	for (i = MAX_CHANNELS - 1; i >= 0; i--) {
		if (stm[i].tonTyp == EMPTY_NOTE) {
			return i;
		}
	}
	// TODO: Look for deprioritized channels.
	/* for (i = MAX_CHANNELS-1; i > 0; i--) {
		if (channel_active[i] == 2) {
			return i;
		}
	} */
	// Fail
	return -1;
}

void Player::playNoteAuto(int instidx, int note, int volume, int tag)
{
	int channel = getChannelForTag(tag);
	if (channel != -1) {
		playNote(note, volume, channel, instidx);
		stm[channel].ntxmTag = tag;
	}
}

void Player::stopNoteAuto(int tag)
{
	for (uint8_t i = 0; i < MAX_CHANNELS; i++)
		if (stm[i].ntxmTag == tag)
			stopChannel(i);
}

void Player::setPatternLoop(bool repeat)
{
	patternLoop = repeat;
}

void Player::setSong(Song *_song)
{
	playing = false;
	song = _song;

	memset(stm, 0, sizeof(stm));
	memset(&state, 0, sizeof(state));

	stopVoices();
	updateMsPerTick();

	state.globVol = 64;
	state.pattDelTime = state.pattDelTime2 = 0; // 8bb: added these
}

// Based on ft2play's pmplay.c

#define MAX_NOTES (10 * 12 * 16 + 16)
#define MAX_FRQ 32000

void Player::setPos(int32_t pos, int32_t row) // -1 = don't change
{
	if (pos != -1) {
		state.songPos = (int16_t)pos;
		if (song->getPotLength() > 0 && state.songPos >= song->getPotLength())
			state.songPos = song->getPotLength() - 1;

		state.pattNr = song->getPotEntry((uint8_t)state.songPos);
		state.pattLen = song->getPatternLength((uint8_t)state.pattNr);
	}

	if (row != -1) {
		state.pattPos = (int16_t)row;
		if (state.pattPos >= state.pattLen)
			state.pattPos = state.pattLen - 1;
	}

	state.timer = 1;
}

void Player::resetVoice(stmTyp *ch)
{
	if (ch->ntxmTag == TAG_SAMPLE) {
		CommandSampleFinish();
	}

	ch->tonTyp = EMPTY_NOTE;
	ch->relTonNr = 0;
	ch->instrNr = NO_INSTRUMENT;
	ch->instrSeg = nullptr;
	ch->status = IS_Vol;

	ch->realVol = 0;
	ch->outVol = 0;
	ch->oldVol = 0;
	ch->finalVol = 0;
	ch->oldPan = 128;
	ch->outPan = 128;
	ch->finalPan = 128;
	ch->vibDepth = 0;

	ch->ntxmTag = TAG_NONE;
}

void Player::stopVoices(void)
{
	stmTyp *ch = stm;

	for (uint8_t i = 0; i < MAX_CHANNELS; i++, ch++) {
		resetVoice(ch);
	}
}

// Based on ft2play's pmp_main.c

typedef void (*volKolEfxRoutine)(stmTyp *ch);
typedef void (*volKolEfxRoutine2)(stmTyp *ch, uint8_t *volKol);
typedef void (*efxRoutine)(stmTyp *ch, uint8_t param);

uint16_t Player::note2Period(uint16_t note)
{
	return (!song || song->getLinear()) ? linearPeriods[note]
	                                    : amigaPeriods[note];
}

void Player::retrigVolume(stmTyp *ch)
{
	ch->realVol = ch->oldVol;
	ch->outVol = ch->oldVol;
	ch->outPan = ch->oldPan;
	ch->status |= IS_Vol + IS_Pan + IS_QuickVol;
}

void Player::retrigEnvelopeVibrato(stmTyp *ch)
{
	// 8bb: reset vibrato position
	if (!(ch->waveCtrl & 0x04))
		ch->vibPos = 0;

	/*
	** 8bb:
	** In FT2.00 .. FT2.09, if the sixth bit of "ch->waveCtrl" is set
	** (from effect E7x where x is $4..$7 or $C..$F) and you trigger a note,
	** the replayer interrupt will freeze / lock up. This is because of a
	** label bug in the original code, causing it to jump back to itself
	** indefinitely.
	*/

	// 8bb: safely reset tremolo position
	if (!(ch->waveCtrl & 0x40))
		ch->tremPos = 0;

	ch->retrigCnt = 0;
	ch->tremorPos = 0;

	ch->envSustainActive = true;

	Instrument *ins = ch->instrSeg;

	// asie: handle null instrument
	if (!ins) {
		ch->fadeOutSpeed = 0;
		ch->fadeOutAmp = 32768;
		return;
	}

	if (ins->vol_env_on) {
		ch->envVCnt = 65535; // 8bb: will be increased to 0 on envelope handling
		ch->envVPos = 0;
	}

	if (ins->pan_env_on) {
		ch->envPCnt = 65535; // 8bb: will be increased to 0 on envelope handling
		ch->envPPos = 0;
	}

	ch->fadeOutSpeed = ins->fadeout_vol;

	// 8bb: final fadeout range is in fact 0..32768, and not 0..65536 like the XM format doc says
	ch->fadeOutAmp = 32768;

	if (ins->vibrato_depth > 0) {
		ch->eVibPos = 0;

		if (ins->vibrato_sweep > 0) {
			ch->eVibAmp = 0;
			ch->eVibSweep = (ins->vibrato_depth << 8) / ins->vibrato_sweep;
		} else {
			ch->eVibAmp = ins->vibrato_depth << 8;
			ch->eVibSweep = 0;
		}
	}
}

void Player::keyOff(stmTyp *ch)
{
	Instrument *ins = ch->instrSeg;

	// asie: handle null instrument
	if (ins && !ins->pan_env_on) // 8bb: FT2 logic bug!
	{
		if (ch->envPCnt >= (uint16_t)ins->pan_envelope_x[ch->envPPos])
			ch->envPCnt = ins->pan_envelope_x[ch->envPPos] - 1;
	}

	if (ins && ins->vol_env_on) {
		if (ch->envVCnt >= (uint16_t)ins->vol_envelope_x[ch->envVPos])
			ch->envVCnt = ins->vol_envelope_x[ch->envVPos] - 1;
	} else {
		ch->realVol = 0;
		ch->outVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}

	ch->envSustainActive = false;
}

void Player::startTone(uint8_t ton, uint8_t effTyp, uint8_t eff, stmTyp *ch)
{
	if (ton == STOP_NOTE) {
		keyOff(ch);
		return;
	}

	// 8bb: if we came from Rxy (retrig), we didn't check note (Ton) yet
	if (ton == EMPTY_NOTE) {
		ton = ch->tonNr;
		if (ton == EMPTY_NOTE)
			return; // 8bb: if still no note, return
	}

	ch->tonNr = ton;

	Instrument *ins = song->getInstrument(ch->instrNr);
	ch->instrSeg = ins;
	ch->mute = ins && ins->mute;

	uint8_t smp =
	    ins ? (ins->getNoteSample(ton) & 0xF) : 0; // 8bb: added for safety
	ch->sampleNr = smp;

	Sample *s = PMPSampleOverride ? PMPSampleOverride
	                              : (ins ? ins->getSample(smp) : nullptr);
	ch->relTonNr = !s ? 0 : s->rel_note;

	ton += ch->relTonNr;
	if (ton >= 10 * 12) // 8bb: unsigned check (also handles note < 0)
		return;

	ch->oldVol = !s ? 64 : s->volume;
	ch->oldPan = !s ? 128 : s->panning;

	if (effTyp == 0x0E && (eff & 0xF0) == 0x50) // 8bb: EFx - Set Finetune
		ch->fineTune = ((eff & 0x0F) << 4) - 128;
	else
		ch->fineTune = !s ? 0 : s->finetune;

	if (ton != EMPTY_NOTE) {
		const uint16_t tmpTon =
		    ((ton) << 4) + (((int8_t)ch->fineTune >> 3) + 16); // 8bb: 0..1935
		ch->outPeriod = ch->realPeriod = note2Period(tmpTon);
	}

	ch->status |= IS_Period + IS_Vol + IS_Pan + IS_NyTon + IS_QuickVol;

	u8 smpOffset;
	if (effTyp == 9) // 8bb: 9xx - Set Sample Offset
	{
		if (eff)
			ch->smpOffset = ch->eff;

		smpOffset = ch->smpOffset;
	} else {
		smpOffset = 0;
	}

	startSongChannel(PMPTmpActiveChannel, ch, s, smpOffset);
}

void Player::finePortaUp(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fPortaUpSpeed;

	ch->fPortaUpSpeed = param;

	ch->realPeriod -= param << 2;
	if ((int16_t)ch->realPeriod < 1)
		ch->realPeriod = 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

void Player::finePortaDown(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fPortaDownSpeed;

	ch->fPortaDownSpeed = param;

	ch->realPeriod += param << 2;
	if ((int16_t)ch->realPeriod >
	    MAX_FRQ - 1) // 8bb: FT2 bug, should've been unsigned comparison!
		ch->realPeriod = MAX_FRQ - 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

void Player::setGlissCtrl(stmTyp *ch, uint8_t param)
{
	ch->glissFunk = param;
}

void Player::setVibratoCtrl(stmTyp *ch, uint8_t param)
{
	ch->waveCtrl = (ch->waveCtrl & 0xF0) | param;
}

void Player::jumpLoop(stmTyp *ch, uint8_t param)
{
	if (param == 0) {
		ch->pattPos = state.pattPos & 0xFF;
	} else if (ch->loopCnt == 0) {
		ch->loopCnt = param;

		state.pBreakPos = ch->pattPos;
		state.pBreakFlag = true;
	} else if (--ch->loopCnt > 0) {
		state.pBreakPos = ch->pattPos;
		state.pBreakFlag = true;
	}
}

void Player::setTremoloCtrl(stmTyp *ch, uint8_t param)
{
	ch->waveCtrl = (param << 4) | (ch->waveCtrl & 0x0F);
}

void Player::volFineUp(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fVolSlideUpSpeed;

	ch->fVolSlideUpSpeed = param;

	ch->realVol += param;
	if (ch->realVol > 64)
		ch->realVol = 64;

	ch->outVol = ch->realVol;
	ch->status |= IS_Vol;
}

void Player::volFineDown(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fVolSlideDownSpeed;

	ch->fVolSlideDownSpeed = param;

	ch->realVol -= param;
	if ((int8_t)ch->realVol < 0)
		ch->realVol = 0;

	ch->outVol = ch->realVol;
	ch->status |= IS_Vol;
}

void Player::noteCut0(stmTyp *ch, uint8_t param)
{
	if (param == 0) // 8bb: only a parameter of zero is handled here
	{
		ch->realVol = 0;
		ch->outVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}
}

void Player::pattDelay(stmTyp *ch, uint8_t param)
{
	if (state.pattDelTime2 == 0)
		state.pattDelTime = param + 1;

	(void)ch;
}

void Player::E_Effects_TickZero(stmTyp *ch, uint8_t param)
{
	switch (param >> 4) {
	case 0x1: finePortaUp(ch, param & 0x0F); break;
	case 0x2: finePortaDown(ch, param & 0x0F); break;
	case 0x3: setGlissCtrl(ch, param & 0x0F); break;
	case 0x4: setVibratoCtrl(ch, param & 0x0F); break;
	case 0x6: jumpLoop(ch, param & 0x0F); break;
	case 0x7: setTremoloCtrl(ch, param & 0x0F); break;
	case 0xA: volFineUp(ch, param & 0x0F); break;
	case 0xB: volFineDown(ch, param & 0x0F); break;
	case 0xC: noteCut0(ch, param & 0x0F); break;
	case 0xE: pattDelay(ch, param & 0x0F); break;
	}
}

void Player::posJump(stmTyp *ch, uint8_t param)
{
	state.songPos = (int16_t)param - 1;
	state.pBreakPos = 0;
	state.posJumpFlag = true;

	(void)ch;
}

void Player::pattBreak(stmTyp *ch, uint8_t param)
{
	state.posJumpFlag = true;

	param = ((param >> 4) * 10) + (param & 0x0F);
	if (param <= 63)
		state.pBreakPos = param;
	else
		state.pBreakPos = 0;

	(void)ch;
}

void Player::setSpeed(stmTyp *ch, uint8_t param)
{
	if (param >= 32) {
		state.bpm = param;
	} else {
		state.timer = state.speed = param;
	}

	updateMsPerTick();

	(void)ch;
}

void Player::setGlobaVol(stmTyp *ch, uint8_t param)
{
	if (param > 64)
		param = 64;

	state.globVol = param;

	stmTyp *c = stm;
	for (int32_t i = 0; i < MAX_CHANNELS;
	     i++, c++) // 8bb: this updates the volume for all voices
		c->status |= IS_Vol;

	(void)ch;
}

void Player::setEnvelopePos(stmTyp *ch, uint8_t param)
{
	bool envUpdate;
	int8_t point;
	int16_t tick;

	Instrument *ins = ch->instrSeg;

	// asie: handle null instrument
	// *** VOLUME ENVELOPE ***
	if (ins && ins->vol_env_on) {
		ch->envVCnt = param - 1;

		point = 0;
		envUpdate = true;
		tick = param;

		if (ins->n_vol_points > 1) {
			point++;
			for (int32_t i = 0; i < ins->n_vol_points - 1; i++) {
				if (tick < ins->vol_envelope_x[point]) {
					point--;

					tick -= ins->vol_envelope_x[point];
					if (tick == 0) // 8bb: FT2 doesn't test for <= 0 here
					{
						envUpdate = false;
						break;
					}

					const int16_t x0 = ins->vol_envelope_x[point + 0];
					const int16_t x1 = ins->vol_envelope_x[point + 1];

					const int16_t xDiff = x1 - x0;
					if (xDiff <= 0) {
						envUpdate = true;
						break;
					}

					const int16_t y0 = ins->vol_envelope_y[point + 0];
					const int16_t y1 = ins->vol_envelope_y[point + 1];

					const int8_t yDiff = (int8_t)(y1 - y0);
					ch->envVIPValue = (yDiff << 8) / xDiff;

					ch->envVAmp = ((int8_t)y0 << 8) +
					              (int16_t)(ch->envVIPValue * (tick - 1));

					point++;

					envUpdate = false;
					break;
				}

				point++;
			}

			if (envUpdate)
				point--;
		}

		if (envUpdate) {
			ch->envVIPValue = 0;
			ch->envVAmp = (int8_t)ins->vol_envelope_y[point] << 8;
		}

		if (point >= ins->n_vol_points) {
			point = ins->n_vol_points - 1;
			if (point < 0)
				point = 0;
		}

		ch->envVPos = point;
	}

	// *** PANNING ENVELOPE ***
	if (ins &&
	    ins->vol_env_sustain) // 8bb: FT2 logic bug, should've been ins->envPTyp
	{
		ch->envPCnt = param - 1;

		point = 0;
		envUpdate = true;
		tick = param;

		if (ins->n_pan_points > 1) {
			point++;
			for (int32_t i = 0; i < ins->n_pan_points - 1; i++) {
				if (tick < ins->pan_envelope_x[point]) {
					point--;

					tick -= ins->pan_envelope_x[point];
					if (tick == 0) // 8bb: FT2 doesn't test for <= 0 here
					{
						envUpdate = false;
						break;
					}

					const int16_t x0 = ins->pan_envelope_x[point + 0];
					const int16_t x1 = ins->pan_envelope_x[point + 1];

					const int16_t xDiff = x1 - x0;
					if (xDiff <= 0) {
						envUpdate = true;
						break;
					}

					const int16_t y0 = ins->pan_envelope_y[point + 0];
					const int16_t y1 = ins->pan_envelope_y[point + 1];

					const int8_t yDiff = (int8_t)(y1 - y0);
					ch->envPIPValue = (yDiff << 8) / xDiff;

					ch->envPAmp = ((int8_t)y0 << 8) +
					              (int16_t)(ch->envPIPValue * (tick - 1));

					point++;

					envUpdate = false;
					break;
				}

				point++;
			}

			if (envUpdate)
				point--;
		}

		if (envUpdate) {
			ch->envPIPValue = 0;
			ch->envPAmp = (int8_t)ins->pan_envelope_y[point] << 8;
		}

		if (point >= ins->n_pan_points) {
			point = ins->n_pan_points - 1;
			if (point < 0)
				point = 0;
		}

		ch->envPPos = point;
	}
}

/* 8bb:
** -- tick-zero volume column effects --
** 2nd parameter is used for a volume column quirk with the Rxy command (multiretrig)
*/

void Player::v_SetVibSpeed(stmTyp *ch, uint8_t *volKol)
{
	*volKol = (ch->volKolVol & 0x0F) << 2;
	if (*volKol != 0)
		ch->vibSpeed = *volKol;
}

void Player::v_Volume(stmTyp *ch, uint8_t *volKol)
{
	*volKol -= 16;
	if (*volKol >
	    64) // 8bb: no idea why FT2 has this check, this can't happen...
		*volKol = 64;

	ch->outVol = ch->realVol = *volKol;
	ch->status |= IS_Vol + IS_QuickVol;
}

void Player::v_FineSlideDown(stmTyp *ch, uint8_t *volKol)
{
	*volKol = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->realVol;
	if ((int8_t)*volKol < 0)
		*volKol = 0;

	ch->outVol = ch->realVol = *volKol;
	ch->status |= IS_Vol;
}

void Player::v_FineSlideUp(stmTyp *ch, uint8_t *volKol)
{
	*volKol = (ch->volKolVol & 0x0F) + ch->realVol;
	if (*volKol > 64)
		*volKol = 64;

	ch->outVol = ch->realVol = *volKol;
	ch->status |= IS_Vol;
}

void Player::v_SetPan(stmTyp *ch, uint8_t *volKol)
{
	*volKol <<= 4;

	ch->outPan = *volKol;
	ch->status |= IS_Pan;
}

// -- non-tick-zero volume column effects --

void Player::v_SlideDown(stmTyp *ch)
{
	uint8_t newVol = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->realVol;
	if ((int8_t)newVol < 0)
		newVol = 0;

	ch->outVol = ch->realVol = newVol;
	ch->status |= IS_Vol;
}

void Player::v_SlideUp(stmTyp *ch)
{
	uint8_t newVol = (ch->volKolVol & 0x0F) + ch->realVol;
	if (newVol > 64)
		newVol = 64;

	ch->outVol = ch->realVol = newVol;
	ch->status |= IS_Vol;
}

void Player::v_Vibrato(stmTyp *ch)
{
	const uint8_t param = ch->volKolVol & 0xF;
	if (param > 0)
		ch->vibDepth = param;

	vibrato2(ch);
}

void Player::v_PanSlideLeft(stmTyp *ch)
{
	uint16_t tmp16 = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->outPan;
	if (tmp16 <
	    256) // 8bb: includes an FT2 bug: pan-slide-left of 0 = set pan to 0
		tmp16 = 0;

	ch->outPan = (uint8_t)tmp16;
	ch->status |= IS_Pan;
}

void Player::v_PanSlideRight(stmTyp *ch)
{
	uint16_t tmp16 = (ch->volKolVol & 0x0F) + ch->outPan;
	if (tmp16 > 255)
		tmp16 = 255;

	ch->outPan = (uint8_t)tmp16;
	ch->status |= IS_Pan;
}

void Player::v_TonePorta(stmTyp *ch)
{
	tonePorta(ch,
	          0); // 8bb: the last parameter is actually not used in tonePorta()
}

void Player::VJumpTab_TickNonZero(uint8_t efx, stmTyp *ch)
{
	switch (efx) {
	case 0x6: v_SlideDown(ch); break;
	case 0x7: v_SlideUp(ch); break;
	case 0xB: v_Vibrato(ch); break;
	case 0xD: v_PanSlideLeft(ch); break;
	case 0xE: v_PanSlideRight(ch); break;
	case 0xF: v_TonePorta(ch); break;
	}
}

void Player::VJumpTab_TickZero(uint8_t efx, stmTyp *ch, uint8_t *volKol)
{
	switch (efx) {
	case 0x1:
	case 0x2:
	case 0x3:
	case 0x4:
	case 0x5: v_Volume(ch, volKol); break;
	case 0x8: v_FineSlideDown(ch, volKol); break;
	case 0x9: v_FineSlideUp(ch, volKol); break;
	case 0xA: v_SetVibSpeed(ch, volKol); break;
	case 0xC: v_SetPan(ch, volKol); break;
	}
}

void Player::setPan(stmTyp *ch, uint8_t param)
{
	ch->outPan = param;
	ch->status |= IS_Pan;
}

void Player::setVol(stmTyp *ch, uint8_t param)
{
	if (param > 64)
		param = 64;

	ch->outVol = ch->realVol = param;
	ch->status |= IS_Vol + IS_QuickVol;
}

void Player::xFinePorta(stmTyp *ch, uint8_t param)
{
	const uint8_t type = param >> 4;
	param &= 0x0F;

	if (type == 0x1) // extra fine porta up
	{
		if (param == 0)
			param = ch->ePortaUpSpeed;

		ch->ePortaUpSpeed = param;

		uint16_t newPeriod = ch->realPeriod;

		newPeriod -= param;
		if ((int16_t)newPeriod < 1)
			newPeriod = 1;

		ch->outPeriod = ch->realPeriod = newPeriod;
		ch->status |= IS_Period;
	} else if (type == 0x2) // extra fine porta down
	{
		if (param == 0)
			param = ch->ePortaDownSpeed;

		ch->ePortaDownSpeed = param;

		uint16_t newPeriod = ch->realPeriod;

		newPeriod += param;
		if ((int16_t)newPeriod >
		    MAX_FRQ - 1) // 8bb: FT2 bug, should've been unsigned comparison!
			newPeriod = MAX_FRQ - 1;

		ch->outPeriod = ch->realPeriod = newPeriod;
		ch->status |= IS_Period;
	}
}

void Player::doMultiRetrig(
    stmTyp *ch,
    uint8_t
        param) // 8bb: "param" is never used (needed for efx jumptable structure)
{
	uint8_t cnt = ch->retrigCnt + 1;
	if (cnt < ch->retrigSpeed) {
		ch->retrigCnt = cnt;
		return;
	}

	ch->retrigCnt = 0;

	int16_t vol = ch->realVol;
	switch (ch->retrigVol) {
	case 0x1: vol -= 1; break;
	case 0x2: vol -= 2; break;
	case 0x3: vol -= 4; break;
	case 0x4: vol -= 8; break;
	case 0x5: vol -= 16; break;
	case 0x6: vol = (vol >> 1) + (vol >> 3) + (vol >> 4); break;
	case 0x7: vol >>= 1; break;
	case 0x8: break; // 8bb: does not change the volume
	case 0x9: vol += 1; break;
	case 0xA: vol += 2; break;
	case 0xB: vol += 4; break;
	case 0xC: vol += 8; break;
	case 0xD: vol += 16; break;
	case 0xE: vol = (vol >> 1) + vol; break;
	case 0xF: vol += vol; break;
	default: break;
	}
	vol = ntxm_clamp(vol, 0, 64);

	ch->realVol = (uint8_t)vol;
	ch->outVol = ch->realVol;

	if (ch->volKolVol >= 0x10 &&
	    ch->volKolVol <= 0x50) // 8bb: Set Volume (volume column)
	{
		ch->outVol = ch->volKolVol - 0x10;
		ch->realVol = ch->outVol;
	} else if (ch->volKolVol >= 0xC0 &&
	           ch->volKolVol <= 0xCF) // 8bb: Set Panning (volume column)
	{
		ch->outPan = (ch->volKolVol & 0x0F) << 4;
	}

	startTone(EMPTY_NOTE, 0, 0, ch);

	(void)param;
}

void Player::multiRetrig(stmTyp *ch, uint8_t param, uint8_t volumeColumnData)
{
	uint8_t tmpParam;

	tmpParam = param & 0x0F;
	if (tmpParam == 0)
		tmpParam = ch->retrigSpeed;

	ch->retrigSpeed = tmpParam;

	tmpParam = param >> 4;
	if (tmpParam == 0)
		tmpParam = ch->retrigVol;

	ch->retrigVol = tmpParam;

	if (volumeColumnData == 0)
		doMultiRetrig(
		    ch,
		    0); // 8bb: the second parameter is never used (needed for efx jumptable structure)
}

void Player::JumpTab_TickZero(stmTyp *ch, uint8_t effTyp, uint8_t eff)
{
	switch (effTyp) {
	case 8: setPan(ch, eff); break;
	case 11: posJump(ch, eff); break;
	case 12: setVol(ch, eff); break;
	case 13: pattBreak(ch, eff); break;
	case 14: E_Effects_TickZero(ch, eff); break;
	case 15: setSpeed(ch, eff); break;
	case 16: setGlobaVol(ch, eff); break;
	case 21: setEnvelopePos(ch, eff); break;
	case 34: xFinePorta(ch, eff); break;
	}
}

void Player::checkEffects(stmTyp *ch) // tick0 effect handling
{
	// volume column effects
	uint8_t newVolKol =
	    ch->volKolVol; // 8bb: manipulated by vol. column effects, then used for multiretrig check (FT2 quirk)
	VJumpTab_TickZero(ch->volKolVol >> 4, ch, &newVolKol);

	// normal effects
	const uint8_t param = ch->eff;

	// asie: .xm loader filters out arpeggio 00 vs non-arpeggio 00
	if ((ch->effTyp == NO_EFFECT) || ch->effTyp > 35)
		return;

	// 8bb: this one has to be done here instead of in the jumptable, as it needs the "newVolKol" parameter (FT2 quirk)
	if (ch->effTyp == 27) // 8bb: Rxy - Multi Retrig
	{
		multiRetrig(ch, param, newVolKol);
		return;
	}

	JumpTab_TickZero(ch, ch->effTyp, ch->eff);
}

void Player::fixTonePorta(stmTyp *ch, const Cell *p, uint8_t inst)
{
	if (p->note != EMPTY_NOTE) {
		if (p->note == STOP_NOTE) {
			keyOff(ch);
		} else {
			const uint16_t portaTmp = ((p->note + ch->relTonNr) << 4) +
			                          (((int8_t)ch->fineTune >> 3) + 16);
			if (portaTmp < MAX_NOTES) {
				ch->wantPeriod = note2Period(portaTmp);

				if (ch->wantPeriod == ch->realPeriod)
					ch->portaDir = 0;
				else if (ch->wantPeriod > ch->realPeriod)
					ch->portaDir = 1;
				else
					ch->portaDir = 2;
			}
		}
	}

	if (inst != NO_INSTRUMENT) {
		retrigVolume(ch);

		if (p->note != STOP_NOTE)
			retrigEnvelopeVibrato(ch);
	}
}

void Player::getNewNote(stmTyp *ch, const Cell *p)
{
	ch->ntxmEarlyRamp = false;
	ch->volKolVol = p->volume;

	if (ch->effTyp == 0) {
		// asie: .xm loader filters out arpeggio 00 vs non-arpeggio 00
		ch->outPeriod = ch->realPeriod;
		ch->status |= IS_Period;
	} else {
		// 8bb: if we have a vibrato (4xy/6xy) on previous row (ch) that ends at current row (p), set period back
		if ((ch->effTyp == 4 || ch->effTyp == 6) &&
		    (p->effect != 4 && p->effect != 6)) {
			ch->outPeriod = ch->realPeriod;
			ch->status |= IS_Period;
		}
	}

	ch->effTyp = p->effect;
	ch->eff = p->effect_param;
	ch->tonTyp = (p->instrument << 8) | p->note;

	// 8bb: 'inst' var is used for later if-checks
	uint8_t inst = p->instrument;
	if (inst < MAX_INSTRUMENTS)
		ch->instrNr = inst;
	else
		inst = NO_INSTRUMENT;

	bool checkEfx = true;
	if (p->effect ==
	    0x0E) // 8bb: check for EDx (Note Delay) and E90 (Retrigger Note)
	{
		if (p->effect_param >= 0xD1 &&
		    p->effect_param <= 0xDF) // 8bb: ED1..EDF (Note Delay)
			return;
		else if (p->effect_param == 0x90) // 8bb: E90 (Retrigger Note)
			checkEfx = false;
	}

	if (checkEfx) {
		if ((ch->volKolVol & 0xF0) == 0xF0) // 8bb: Portamento (volume column)
		{
			const uint8_t volKolParam = ch->volKolVol & 0x0F;
			if (volKolParam > 0)
				ch->portaSpeed = volKolParam << 6;

			fixTonePorta(ch, p, inst);
			checkEffects(ch);
			return;
		}

		if (p->effect == 3 || p->effect == 5) // 8bb: Portamento (3xx/5xx)
		{
			if (p->effect != 5 && p->effect_param != 0)
				ch->portaSpeed = p->effect_param << 2;

			fixTonePorta(ch, p, inst);
			checkEffects(ch);
			return;
		}

		if (p->effect == 0x14 &&
		    p->effect_param ==
		        0) // 8bb: K00 (Key Off - only handle tick 0 here)
		{
			keyOff(ch);

			if (inst)
				retrigVolume(ch);

			checkEffects(ch);
			return;
		}

		if (p->note == EMPTY_NOTE) {
			if (inst != NO_INSTRUMENT) {
				retrigVolume(ch);
				retrigEnvelopeVibrato(ch);
			}

			checkEffects(ch);
			return;
		}
	}

	if (p->note == STOP_NOTE)
		keyOff(ch);
	else
		startTone(p->note, p->effect, p->effect_param, ch);

	if (inst != NO_INSTRUMENT) {
		retrigVolume(ch);
		if (p->note != STOP_NOTE)
			retrigEnvelopeVibrato(ch);
	}

	checkEffects(ch);
}

void Player::fixaEnvelopeVibrato(stmTyp *ch)
{
	bool envInterpolateFlag, envDidInterpolate;
	uint8_t envPos;
	int16_t autoVibVal, envVal;
	uint16_t autoVibAmp;
	uint32_t vol;

	// asie: handle null instrument
	Instrument *ins = ch->instrSeg;

	// *** FADEOUT ***
	if (!ch->envSustainActive) {
		ch->status |= IS_Vol;

		if (ch->fadeOutSpeed >
		    ch->fadeOutAmp) // 8bb: ch->fadeOutAmp-ch->fadeOutSpeed < 0?
		{
			ch->fadeOutAmp = 0;
			ch->fadeOutSpeed = 0;
		} else {
			ch->fadeOutAmp -= ch->fadeOutSpeed;
		}
	}

	if (!ch->mute) {
		// *** VOLUME ENVELOPE ***
		envVal = 0;
		if (ins && ins->vol_env_on) {
			envDidInterpolate = false;
			envPos = ch->envVPos;

			if (++ch->envVCnt == ins->vol_envelope_x[envPos]) {
				ch->envVAmp = (int8_t)ins->vol_envelope_y[envPos] << 8;

				envPos++;
				if (ins->vol_env_loop) {
					envPos--;

					if (envPos == ins->vol_loop_end_point) {
						if (!(ins->vol_env_sustain) ||
						    envPos != ins->vol_sustain_point ||
						    ch->envSustainActive) {
							envPos = ins->vol_loop_start_point;

							ch->envVCnt = ins->vol_envelope_x[envPos];
							ch->envVAmp = (int8_t)ins->vol_envelope_y[envPos]
							              << 8;
						}
					}

					envPos++;
				}

				if (envPos < ins->n_vol_points) {
					envInterpolateFlag = true;
					if ((ins->vol_env_sustain) && ch->envSustainActive) {
						if (envPos - 1 == ins->vol_sustain_point) {
							envPos--;
							ch->envVIPValue = 0;
							envInterpolateFlag = false;
						}
					}

					if (envInterpolateFlag) {
						ch->envVPos = envPos;

						const int16_t x0 = ins->vol_envelope_x[envPos - 1];
						const int16_t x1 = ins->vol_envelope_x[envPos - 0];

						const int16_t xDiff = x1 - x0;
						if (xDiff > 0) {
							const int16_t y0 = ins->vol_envelope_y[envPos - 1];
							const int16_t y1 = ins->vol_envelope_y[envPos - 0];

							const int8_t yDiff = (int8_t)(y1 - y0);
							ch->envVIPValue = (yDiff << 8) / xDiff;

							envVal = ch->envVAmp;
							envDidInterpolate = true;
						} else {
							ch->envVIPValue = 0;
						}
					}
				} else {
					ch->envVIPValue = 0;
				}
			}

			if (!envDidInterpolate) {
				ch->envVAmp += ch->envVIPValue;
				envVal = ch->envVAmp;

				// 8bb: FT2 tests the upper byte here (unsigned test!)
				uint8_t envHiByte = (uint8_t)(envVal >> 8);
				if (envHiByte > 64) {
					if (envHiByte <= 160) // 8bb: 160 unsigned is -64 signed
						envVal = 64 * 256;
					else
						envVal = 0;

					ch->envVIPValue = 0;
				}
			}

			envVal >>= 8;

			vol = (envVal * ch->outVol * ch->fadeOutAmp) >> (16 + 2);
			vol = (vol * state.globVol) >> 7;

			ch->status |=
			    IS_Vol; // 8bb: this updates vol on every tick (because vol envelope is enabled)
		} else {
			vol = ((ch->outVol << 4) * ch->fadeOutAmp) >> 16;
			vol = (vol * state.globVol) >> 7;
		}

		ch->finalVol = (uint16_t)vol; // 8bb: 0..256
	} else {
		ch->finalVol = 0;
	}

	// *** PANNING ENVELOPE ***

	envVal = 0;
	if (ins && ins->pan_env_on) {
		envDidInterpolate = false;
		envPos = ch->envPPos;

		if (++ch->envPCnt == ins->pan_envelope_x[envPos]) {
			ch->envPAmp = (int8_t)ins->pan_envelope_y[envPos] << 8;

			envPos++;
			if (ins->pan_env_loop) {
				envPos--;

				if (envPos == ins->pan_loop_end_point) {
					if (!(ins->pan_env_sustain) ||
					    envPos != ins->pan_sustain_point ||
					    ch->envSustainActive) {
						envPos = ins->pan_loop_start_point;

						ch->envPCnt = ins->pan_envelope_x[envPos];
						ch->envPAmp = (int8_t)ins->pan_envelope_y[envPos] << 8;
					}
				}

				envPos++;
			}

			if (envPos < ins->n_pan_points) {
				envInterpolateFlag = true;
				if ((ins->pan_env_sustain) && ch->envSustainActive) {
					if (envPos - 1 == ins->pan_sustain_point) {
						envPos--;
						ch->envPIPValue = 0;
						envInterpolateFlag = false;
					}
				}

				if (envInterpolateFlag) {
					ch->envPPos = envPos;

					const int16_t x0 = ins->pan_envelope_x[envPos - 1];
					const int16_t x1 = ins->pan_envelope_x[envPos - 0];

					const int16_t xDiff = x1 - x0;
					if (xDiff > 0) {
						const int16_t y0 = ins->pan_envelope_y[envPos - 1];
						const int16_t y1 = ins->pan_envelope_y[envPos - 0];

						const int8_t yDiff = (int8_t)(y1 - y0);
						ch->envPIPValue = (yDiff << 8) / xDiff;

						envVal = ch->envPAmp;
						envDidInterpolate = true;
					} else {
						ch->envPIPValue = 0;
					}
				}
			} else {
				ch->envPIPValue = 0;
			}
		}

		if (!envDidInterpolate) {
			ch->envPAmp += ch->envPIPValue;
			envVal = ch->envPAmp;

			// 8bb: FT2 tests the upper byte here (unsigned test!)
			uint8_t envHiByte = (uint8_t)(envVal >> 8);
			if (envHiByte > 64) {
				if (envHiByte <= 160) // 8bb: 160 unsigned is -64 signed
					envVal = 64 * 256;
				else
					envVal = 0;

				ch->envPIPValue = 0;
			}
		}

		int16_t panTmp = ch->outPan - 128;
		if (panTmp > 0)
			panTmp = 0 - panTmp;
		panTmp += 128;
		panTmp <<= 3;

		envVal -= 32 * 256;
		const int8_t panAdd = (int8_t)((envVal * panTmp) >> 16);

		ch->finalPan = (uint8_t)(ch->outPan + panAdd);
		ch->status |= IS_Pan;
	} else {
		ch->finalPan = ch->outPan;
	}

	// *** AUTO VIBRATO ***
	if (ins && ins->vibrato_depth > 0) {
		if (ch->eVibSweep > 0) {
			autoVibAmp = ch->eVibSweep;
			if (ch->envSustainActive) {
				autoVibAmp += ch->eVibAmp;
				if ((autoVibAmp >> 8) > ins->vibrato_depth) {
					autoVibAmp = ins->vibrato_depth << 8;
					ch->eVibSweep = 0;
				}

				ch->eVibAmp = autoVibAmp;
			}
		} else {
			autoVibAmp = ch->eVibAmp;
		}

		ch->eVibPos += ins->vibrato_rate;

		if (ins->vibrato_type == 1)
			autoVibVal = (ch->eVibPos > 127) ? 64 : -64; // square
		else if (ins->vibrato_type == 2)
			autoVibVal = (((ch->eVibPos >> 1) + 64) & 127) - 64; // ramp up
		else if (ins->vibrato_type == 3)
			autoVibVal = ((-(ch->eVibPos >> 1) + 64) & 127) - 64; // ramp down
		else
			autoVibVal = vibSineTab[ch->eVibPos]; // sine

		autoVibVal <<= 2;
		uint16_t tmpPeriod = (autoVibVal * (int16_t)autoVibAmp) >> 16;

		tmpPeriod += ch->outPeriod;
		if (tmpPeriod >= MAX_FRQ)
			tmpPeriod = 0; // 8bb: yes, FT2 does this (!)

		ch->finalPeriod = tmpPeriod;
		ch->status |= IS_Period;
	} else {
		ch->finalPeriod = ch->outPeriod;
	}
}

// 8bb: converts period to note number, for arpeggio and portamento (in semitone-slide mode)
uint16_t Player::relocateTon(uint16_t period, uint8_t arpNote, stmTyp *ch)
{
	int32_t tmpPeriod;

	const int32_t fineTune = ((int8_t)ch->fineTune >> 3) + 16;

	// 8bb: FT2 bug, should've been 10*12*16. Notes above B-7 (95) will have issues.
	// You can only achieve such high notes by having a high relative note value
	// in the sample.
	int32_t hiPeriod = 8 * 12 * 16;

	int32_t loPeriod = 0;

	for (int32_t i = 0; i < 8; i++) {
		tmpPeriod = (((loPeriod + hiPeriod) >> 1) & ~15) + fineTune;

		int32_t lookUp = tmpPeriod - 8;
		if (lookUp < 0)
			lookUp =
			    0; // 8bb: safety fix (C-0 w/ ftune <= -65). This buggy read seems to return 0 in FT2 (TODO: verify)

		if (period >= note2Period(lookUp))
			hiPeriod = (tmpPeriod - fineTune) & ~15;
		else
			loPeriod = (tmpPeriod - fineTune) & ~15;
	}

	tmpPeriod = loPeriod + fineTune + (arpNote << 4);
	if (tmpPeriod >=
	    (8 * 12 * 16 + 15) -
	        1) // 8bb: FT2 bug, should've been 10*12*16+16 (also notice the +2 difference)
		tmpPeriod = (8 * 12 * 16 + 16) - 1;

	return note2Period(tmpPeriod);
}

void Player::vibrato2(stmTyp *ch)
{
	uint8_t tmpVib = (ch->vibPos >> 2) & 0x1F;

	switch (ch->waveCtrl & 3) {
	// 0: sine
	case 0: tmpVib = vibTab[tmpVib]; break;

	// 1: ramp
	case 1: {
		tmpVib <<= 3;
		if ((int8_t)ch->vibPos < 0)
			tmpVib = ~tmpVib;
	} break;

	// 2/3: square
	default: tmpVib = 255; break;
	}

	tmpVib = (tmpVib * ch->vibDepth) >> 5;

	if ((int8_t)ch->vibPos < 0)
		ch->outPeriod = ch->realPeriod - tmpVib;
	else
		ch->outPeriod = ch->realPeriod + tmpVib;

	ch->status |= IS_Period;
	ch->vibPos += ch->vibSpeed;
}

void Player::arp(stmTyp *ch, uint8_t param)
{
	// 8bb: The original arpTab table only supports 16 ticks, so it can and will overflow.
	// I have added overflown values to the table so that we can handle up to 256 ticks.
	// The added overflow entries are accurate to the overflow-read in FT2.08/FT2.09.
	const uint8_t tick = arpTab[state.timer & 0xFF];

	if (tick == 0) {
		ch->outPeriod = ch->realPeriod;
	} else {
		const uint8_t note = (tick == 1) ? (param >> 4) : (param & 0x0F);
		ch->outPeriod = relocateTon(ch->realPeriod, note, ch);
	}

	ch->status |= IS_Period;
}

void Player::portaUp(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->portaUpSpeed;

	ch->portaUpSpeed = param;

	ch->realPeriod -= param << 2;
	if ((int16_t)ch->realPeriod < 1)
		ch->realPeriod = 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

void Player::portaDown(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->portaDownSpeed;

	ch->portaDownSpeed = param;

	ch->realPeriod += param << 2;
	if ((int16_t)ch->realPeriod >
	    MAX_FRQ - 1) // 8bb: FT2 bug, should've been unsigned comparison!
		ch->realPeriod = MAX_FRQ - 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

void Player::tonePorta(stmTyp *ch,
                       uint8_t param) // 8bb: param is a placeholder (not used)
{
	if (ch->portaDir == 0)
		return;

	if (ch->portaDir > 1) {
		ch->realPeriod -= ch->portaSpeed;
		if ((int16_t)ch->realPeriod <= (int16_t)ch->wantPeriod) {
			ch->portaDir = 1;
			ch->realPeriod = ch->wantPeriod;
		}
	} else {
		ch->realPeriod += ch->portaSpeed;
		if (ch->realPeriod >= ch->wantPeriod) {
			ch->portaDir = 1;
			ch->realPeriod = ch->wantPeriod;
		}
	}

	if (ch->glissFunk) // 8bb: semitone-slide flag
		ch->outPeriod = relocateTon(ch->realPeriod, 0, ch);
	else
		ch->outPeriod = ch->realPeriod;

	ch->status |= IS_Period;

	(void)param;
}

void Player::vibrato(stmTyp *ch, uint8_t param)
{
	uint8_t tmp8;

	if (ch->eff > 0) {
		tmp8 = param & 0x0F;
		if (tmp8 > 0)
			ch->vibDepth = tmp8;

		tmp8 = (param & 0xF0) >> 2;
		if (tmp8 > 0)
			ch->vibSpeed = tmp8;
	}

	vibrato2(ch);
}

void Player::tonePlusVol(stmTyp *ch, uint8_t param)
{
	tonePorta(ch, 0); // 8bb: the last parameter is not used in tonePorta()
	volume(ch, param);

	(void)param;
}

void Player::vibratoPlusVol(stmTyp *ch, uint8_t param)
{
	vibrato2(ch);
	volume(ch, param);

	(void)param;
}

void Player::tremolo(stmTyp *ch, uint8_t param)
{
	uint8_t tmp8;
	int16_t tremVol;

	const uint8_t tmpEff = param;
	if (tmpEff > 0) {
		tmp8 = tmpEff & 0x0F;
		if (tmp8 > 0)
			ch->tremDepth = tmp8;

		tmp8 = (tmpEff & 0xF0) >> 2;
		if (tmp8 > 0)
			ch->tremSpeed = tmp8;
	}

	uint8_t tmpTrem = (ch->tremPos >> 2) & 0x1F;
	switch ((ch->waveCtrl >> 4) & 3) {
	// 0: sine
	case 0: tmpTrem = vibTab[tmpTrem]; break;

	// 1: ramp
	case 1: {
		tmpTrem <<= 3;
		if ((int8_t)ch->vibPos < 0) // 8bb: FT2 bug, should've been ch->tremPos
			tmpTrem = ~tmpTrem;
	} break;

	// 2/3: square
	default: tmpTrem = 255; break;
	}
	tmpTrem = (tmpTrem * ch->tremDepth) >> 6;

	if ((int8_t)ch->tremPos < 0) {
		tremVol = ch->realVol - tmpTrem;
		if (tremVol < 0)
			tremVol = 0;
	} else {
		tremVol = ch->realVol + tmpTrem;
		if (tremVol > 64)
			tremVol = 64;
	}

	ch->outVol = (uint8_t)tremVol;
	ch->status |= IS_Vol;
	ch->tremPos += ch->tremSpeed;
}

void Player::volume(stmTyp *ch, uint8_t param) // 8bb: volume slide
{
	if (param == 0)
		param = ch->volSlideSpeed;

	ch->volSlideSpeed = param;

	uint8_t newVol = ch->realVol;
	if ((param & 0xF0) == 0) {
		newVol -= param;
		if ((int8_t)newVol < 0)
			newVol = 0;
	} else {
		param >>= 4;

		newVol += param;
		if (newVol > 64)
			newVol = 64;
	}

	ch->outVol = ch->realVol = newVol;
	ch->status |= IS_Vol;
}

void Player::globalVolSlide(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->globVolSlideSpeed;

	ch->globVolSlideSpeed = param;

	uint8_t newVol = (uint8_t)state.globVol;
	if ((param & 0xF0) == 0) {
		newVol -= param;
		if ((int8_t)newVol < 0)
			newVol = 0;
	} else {
		param >>= 4;

		newVol += param;
		if (newVol > 64)
			newVol = 64;
	}

	state.globVol = newVol;

	stmTyp *c = stm;
	for (int32_t i = 0; i < MAX_CHANNELS;
	     i++, c++) // 8bb: this updates the volume for all voices
		c->status |= IS_Vol;
}

void Player::keyOffCmd(stmTyp *ch, uint8_t param)
{
	if ((uint8_t)(state.speed - state.timer) == (param & 31))
		keyOff(ch);
}

void Player::panningSlide(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->panningSlideSpeed;

	ch->panningSlideSpeed = param;

	int16_t newPan = (int16_t)ch->outPan;
	if ((param & 0xF0) == 0) {
		newPan -= param;
		if (newPan < 0)
			newPan = 0;
	} else {
		param >>= 4;

		newPan += param;
		if (newPan > 255)
			newPan = 255;
	}

	ch->outPan = (uint8_t)newPan;
	ch->status |= IS_Pan;
}

void Player::tremor(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->tremorSave;

	ch->tremorSave = param;

	uint8_t tremorSign = ch->tremorPos & 0x80;
	uint8_t tremorData = ch->tremorPos & 0x7F;

	tremorData--;
	if ((int8_t)tremorData < 0) {
		if (tremorSign == 0x80) {
			tremorSign = 0x00;
			tremorData = param & 0x0F;
		} else {
			tremorSign = 0x80;
			tremorData = param >> 4;
		}
	}

	ch->tremorPos = tremorSign | tremorData;
	ch->outVol = (tremorSign == 0x80) ? ch->realVol : 0;
	ch->status |= IS_Vol + IS_QuickVol;
}

void Player::retrigNote(stmTyp *ch, uint8_t param)
{
	if (param == 0) // 8bb: E9x with a param of zero is handled in getNewNote()
		return;

#if 0
	if ((state.speed-state.timer) % param == 0)
#else
	if (retrigTickTable[param][state.speed - state.timer] == 0)
#endif
	{
		startTone(EMPTY_NOTE, 0, 0, ch);
		retrigEnvelopeVibrato(ch);
	}
}

void Player::noteCut(stmTyp *ch, uint8_t param)
{
	if ((uint8_t)(state.speed - state.timer) == param) {
		ch->outVol = ch->realVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}
}

void Player::noteDelay(stmTyp *ch, uint8_t param)
{
	if ((uint8_t)(state.speed - state.timer) == param) {
		startTone(ch->tonTyp & 0xFF, 0, 0, ch);

		if ((ch->tonTyp >> 8) !=
		    NO_INSTRUMENT) // 8bb: do we have an instrument number?
			retrigVolume(ch);

		retrigEnvelopeVibrato(ch);

		if (ch->volKolVol >= 0x10 &&
		    ch->volKolVol <= 0x50) // 8bb: Set Volume (volume column)
		{
			ch->outVol = ch->volKolVol - 16;
			ch->realVol = ch->outVol;
		} else if (ch->volKolVol >= 0xC0 &&
		           ch->volKolVol <= 0xCF) // 8bb: Set Panning (volume column)
		{
			ch->outPan = (ch->volKolVol & 0x0F) << 4;
		}
	}
}

void Player::E_Effects_TickNonZero(stmTyp *ch, uint8_t param)
{
	switch (param >> 4) {
	case 0x9: retrigNote(ch, param & 0xF); break;
	case 0xC: noteCut(ch, param & 0xF); break;
	case 0xD: noteDelay(ch, param & 0xF); break;
	}
}

void Player::JumpTab_TickNonZero(stmTyp *ch, uint8_t effTyp, uint8_t eff)
{
	switch (effTyp) {
	case 0: arp(ch, eff); break;
	case 1: portaUp(ch, eff); break;
	case 2: portaDown(ch, eff); break;
	case 3: tonePorta(ch, eff); break;
	case 4: vibrato(ch, eff); break;
	case 5: tonePlusVol(ch, eff); break;
	case 6: vibratoPlusVol(ch, eff); break;
	case 7: tremolo(ch, eff); break;
	case 10: volume(ch, eff); break;
	case 14: E_Effects_TickNonZero(ch, eff); break;
	case 17: globalVolSlide(ch, eff); break;
	case 20: keyOffCmd(ch, eff); break;
	case 25: panningSlide(ch, eff); break;
	case 27: doMultiRetrig(ch, eff); break;
	case 29: tremor(ch, eff); break;
	}
}

void Player::doEffects(stmTyp *ch) // tick>0 effect handling
{
	const uint8_t volKolEfx = ch->volKolVol >> 4;
	if (volKolEfx > 0)
		VJumpTab_TickNonZero(volKolEfx, ch);

	// asie: .xm loader filters out arpeggio 00 vs non-arpeggio 00
	if ((ch->effTyp == NO_EFFECT) || ch->effTyp > 35)
		return;

	JumpTab_TickNonZero(ch, ch->effTyp, ch->eff);
}

void Player::getNextPos(void)
{
	state.pattPos++;

	if (state.pattDelTime > 0) {
		state.pattDelTime2 = state.pattDelTime;
		state.pattDelTime = 0;
	}

	if (state.pattDelTime2 > 0) {
		state.pattDelTime2--;
		if (state.pattDelTime2 > 0)
			state.pattPos--;
	}

	if (state.pBreakFlag) {
		state.pBreakFlag = false;
		state.pattPos = state.pBreakPos;
	}

	if (state.pattPos >= state.pattLen || state.posJumpFlag) {
		state.pattPos = state.pBreakPos;
		state.pBreakPos = 0;
		state.posJumpFlag = false;

		// ntxm: handle patternLoop flag
		if (!patternLoop) {
			state.songPos++;
			if (state.songPos >= song->getPotLength()) {
				// ntxm: handle songLoop flag
				if (!songLoop) {
					stop();
					CommandNotifyStop();
					return;
				}
				state.songPos = song->getRestartPosition();
			}

			state.pattNr = song->getPotEntry((uint8_t)state.songPos);
			state.pattLen = song->getPatternLength((uint8_t)state.pattNr);
		}

		CommandUpdatePotPos(state.songPos);
	}

	CommandUpdateRow(state.pattPos);
}

void Player::mainPlayer(void)
{
	int i = 0;
	stmTyp *c = stm;

	// asie: continue playing effects even when the timer is not running
	if (playing) {
		bool tickZero = false;

		state.timer--;
		if (state.timer == 0) {
			state.timer = state.speed;
			tickZero = true;
		}

		const bool readNewNote = tickZero && (state.pattDelTime2 == 0);
		if (readNewNote) {
			for (; i < song->n_channels; i++, c++) {
				const Cell *pattPtr =
				    &song->getPattern(state.pattNr)[i][state.pattPos];
				PMPTmpActiveChannel = i; // 8bb: for P_StartTone()
				getNewNote(c, pattPtr);
				fixaEnvelopeVibrato(c);
			}
		}
	}

	for (; i < MAX_CHANNELS; i++, c++) {
		PMPTmpActiveChannel = i; // 8bb: for P_StartTone()
		doEffects(c);
		fixaEnvelopeVibrato(c);
	}

	if (playing && state.timer == 1)
		getNextPos();
}
