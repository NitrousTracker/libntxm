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

#include <stdlib.h>
#include <string.h>

extern "C" {
  #include "ntxm/demokit.h"
}

#include "ntxm/ntxmtools.h"
#include "ntxm/fifocommand.h"
#include "ntxm/song.h"
#include "ntxm/player.h"
#include "ntxm/vibrato_sine_table.h"

#define MIN(x,y)	((x)<(y)?(x):(y))

extern bool ntxm_recording;

/* ===================== PUBLIC ===================== */

Player::Player(void (*_externalTimerHandler)(void))
	:song(0), externalTimerHandler(_externalTimerHandler)
{
	initState();

	initEffState();

	demoInit();

	startPlayTimer();
}

void* Player::operator new (size_t size) {

	return ntxm_cmalloc(size);

} // default ctor implicitly called here

void Player::operator delete (void *p) {

	if ( NULL != p ) ntxm_free(p);

} // default dtor implicitly called here

void Player::setSong(Song *_song)
{
	song = _song;
	initState();

	// Init fading
	memset(state.channel_fade_active, 0, sizeof(state.channel_fade_active));
	memset(state.channel_fade_ms, 0, sizeof(state.channel_fade_ms));
	memset(state.channel_volume, 0, sizeof(state.channel_volume));
	memset(state.channel_fade_target_volume, 0, sizeof(state.channel_fade_target_volume));
}

// Set the current pattern to looping
void Player::setPatternLoop(bool loopstate) {
	state.patternloop = loopstate;
}

// Plays the song till the end starting at pattern order table position potpos and row row
void Player::play(u8 potpos, u16 row, bool loop)
{
	// Mark all channels inactive
	if(state.playing == false) {
		memset(state.channel_active, 0, sizeof(state.channel_active));
		memset(state.channel_ms_left, 0, sizeof(state.channel_ms_left));
		memset(state.channel_loop, 0, sizeof(state.channel_loop));
	}

	state.potpos = potpos;
	state.row = row;
	state.pattern = song->pattern_order_table[state.potpos];
	state.songloop = loop;

	// Reset ms and tick counter
	state.tick_ms = 0;
	state.row_ticks = 0;

	state.juststarted = true;

	lastms = getTicks();

	initEffState();
	initDefaultPanning();

	state.playing = true;
}

void Player::stop(void)
{
	if (!state.playing)
		return;

	state.playing = false;

	// Stop all playing samples
	u8 end;
	if(song->n_channels < MAX_CHANNELS) {
		end = song->n_channels;
	} else {
		end = MAX_CHANNELS;
	}

	for(u8 chn = 0; chn < end; chn++)
	{
		state.channel_fade_active[chn] = 1;
		state.channel_fade_ms[chn] = FADE_OUT_MS;
		state.channel_fade_target_volume[chn] = 0;
		
		state.channel_porta_accumulator[chn] = 0;
		state.channel_porta_tone_increment[chn] = 0;
		state.channel_porta_increment[chn] = 0;
		state.channel_porta_decrement[chn] = 0;
		state.channel_fine_porta_increment[chn] = 0;
		state.channel_fine_porta_decrement[chn] = 0;
		state.channel_exfine_porta_increment[chn] = 0;
		state.channel_exfine_porta_decrement[chn] = 0;
		state.channel_porta_enabled[chn] = false;
		resetVibrato(chn);
	}
	
	resetPanning();
}

// Play the note with the given settings.
void Player::playNote(u8 note, u8 volume, u8 channel, u8 instidx)
{
	//reset portamento to init
	state.channel_porta_accumulator[channel] = 0;
	state.channel_porta_enabled[channel] = false;
	state.channel_tags[channel] = UNTAGGED;
	
	if( (state.playing == true) && (song->channelMuted(channel) == true) )
		return;

	Instrument *inst = song->getInstrument(instidx);

	if(inst == 0)
		return;

	Sample *smp = inst->getSampleForNote(note);
	if (smp == 0)
		return;

	// Stop possibly active fades
	state.channel_fade_active[channel] = 0;
	state.channel_fade_ms[channel] = 0;
	state.channel_instrument[channel] = instidx;
	
	if(volume == NO_VOLUME) {
		state.channel_volume[channel] = MAX_VOLUME * smp->getVolume() / 255;
	} else {
		state.channel_volume[channel] = volume * smp->getVolume() / 255;
	}
	state.channel_prev_sample_vol[channel] = smp->getVolume(); //Store for later channel volume updates

	if(smp->getLoop() != 0) {
		state.channel_loop[channel] = true;
		state.channel_ms_left[channel] = 0;
	} else {
		state.channel_loop[channel] = false;
		state.channel_ms_left[channel] = inst->calcPlayLength(note);
	}

	state.channel_fade_vol[channel] = state.channel_volume[channel];

	state.channel_note[channel]   = note;
	state.channel_active[channel] = 1;

	//xm standard is to reset effect panning each note
	u8 pan = smp->getBasePanning();
	smp->setPanning(pan);

	// special case: check for sample offset
	u8 effect = song->patterns[state.pattern][channel][state.row].effect;
	u8 param  = song->patterns[state.pattern][channel][state.row].effect_param;

	u8 offs = 0;
	if (effect == EFFECT_SAMPLE_OFFSET)
		offs = param;
	
	inst->play(note, volume, channel, offs);
}

// Play the given sample (and send a notification when done)
void Player::playSample(Sample *sample, u8 note, u8 volume, u8 channel)
{
	// Stop playing sample if necessary
	if(state.playing_single_sample == true) {
		state.playing_single_sample = false;
		state.single_sample_ms_remaining = 0;
		CommandSampleFinish();
	}

	// Calculate length
	u32 length = sample->calcPlayLength(note);

	// Set sample playing state
	state.playing_single_sample = true;
	state.single_sample_ms_remaining = length;
	state.single_sample_channel = channel;

	// Play
	sample->play(note, volume, channel);
}

void Player::stopAllNotes(u8 note, u8 instidx)
{
	for (u8 i = 0; i < MAX_CHANNELS; i++) {
		if(state.channel_note[i] == note && state.channel_instrument[i] == instidx) {
			stopChannel(i);
		}
	}
}

// Stop playback on a channel
void Player::stopChannel(u8 channel)
{
	// Stop single sample if it's played on this channel
	if((state.playing_single_sample == true) && (state.single_sample_channel == channel))
	{
		SCHANNEL_CR(channel) = 0;

		state.playing_single_sample = false;
		state.single_sample_ms_remaining = 0;

		CommandSampleFinish();
	}
	else if(SCHANNEL_CR(channel) & BIT(31))
	{
		state.channel_fade_active[channel]        = 1;
		state.channel_fade_ms[channel]            = FADE_OUT_MS;
		state.channel_fade_target_volume[channel] = 0;
	}
	
	state.channel_tags[channel] = UNTAGGED;
}

int Player::getChannelForTag(u16 tag)
{
	int i;
	
	// First look for existing channels using this tag, just in case.
	for (i = 0; i < MAX_CHANNELS; i++) {
		if (state.channel_tags[i] == tag) {
			return i;
		}
	}
	// Look for inactive channels.
	for (i = MAX_CHANNELS-1; i > 0; i--) {
		if (state.channel_active[i] == 0) {
			return i;
		}
	}
	// Look for deprioritized channels.
	for (i = MAX_CHANNELS-1; i > 0; i--) {
		if (state.channel_active[i] == 2) {
			return i;
		}
	}
	// Fail
	return -1;
}

void Player::playNoteAuto(u8 instidx, u8 note, u8 volume, u16 tag)
{
	int channel = getChannelForTag(tag);
	if (channel != -1) {
		playNote(note, volume, channel, instidx);
		state.channel_tags[channel] = tag;
	}
}

void Player::stopNoteAuto(u16 tag)
{
	for (int i = 0; i < MAX_CHANNELS; i++) {
		if (state.channel_tags[i] == tag) {
			stopChannel(i);
			return;
		}
	}
}

void Player::playTimerHandler(void)
{
	if(ntxm_recording && !state.playing)
		return;

	u32 passed_time = getTicks() - lastms;
	lastms = getTicks();

	// Fading stuff
	handleFade(passed_time);

	// Are we playing a single sample (a sample not from the song)?
	// (Built in for games etc)
	if(state.playing_single_sample)
	{
		// Count down, and send signal when done
		if(state.single_sample_ms_remaining < passed_time)
		{
			SCHANNEL_CR(state.single_sample_channel) = 0;

			state.playing_single_sample = false;
			state.single_sample_ms_remaining = 0;

			CommandSampleFinish();
		}
		else
		{
			state.single_sample_ms_remaining -= passed_time;
		}
	}

	if(song == 0) {
		return;
	}

	// Update tick ms
	state.tick_ms += passed_time << 16;

	// Check if we are shortly before the next tick. (As long as a fade would take)
	if(state.tick_ms >= song->getMsPerTick() - (FADE_OUT_MS << 16))
	{
		// Is there a request to set the volume?
		for(u8 channel=0; channel<song->n_channels && channel<MAX_CHANNELS; ++channel)
		{
			if(effstate.channel_setvol_requested[channel])
			{
				state.channel_fade_active[channel] = 1;
				state.channel_fade_ms[channel] = FADE_OUT_MS;
				effstate.channel_setvol_requested[channel] = false;
			}
		}

		// Is this the last tick before the next row?
		if(state.row_ticks >= song->getTempo()-1)
		{
			// If so, check if for any of the active channels a new note starts in the next row.
			u8 nextNote;
			for(u8 channel=0; channel<song->n_channels && channel<MAX_CHANNELS; ++channel)
			{
				if(state.channel_active[channel] == 1)
				{
					u16 nextrow;
					u8 nextpattern, nextpotpos;

					if(!calcNextPos(&nextrow, &nextpotpos))
					{
						nextpattern = song->pattern_order_table[nextpotpos];

						nextNote = song->patterns[nextpattern][channel][nextrow].note;
						if((nextNote!=EMPTY_NOTE) && (state.channel_fade_active[channel] == 0))
						{
							// If so, fade out to avoid a click.
							state.channel_fade_active[channel] = 1;
							state.channel_fade_ms[channel] = FADE_OUT_MS;
							state.channel_fade_target_volume[channel] = 0;
						}
					}
				}
			}
		}
	}

	// Update active channels
	for(u8 channel=0; channel<MAX_CHANNELS; ++channel)
	{
		if(state.channel_ms_left[channel] > 0)
		{
			if(state.channel_ms_left[channel] > passed_time) {
				state.channel_ms_left[channel] -= passed_time;
			} else {
				state.channel_ms_left[channel] = 0;
			}
			//state.channel_ms_left[channel]--; // WTF?
			if((state.channel_ms_left[channel]==0)&&(state.channel_loop[channel]==false)) {
				state.channel_active[channel] = 0;
			}
		}
	}

	// Update envelopes
	for(u8 channel=0; channel<MAX_CHANNELS; ++channel)
	{
		if(state.channel_active[channel])
		{
			Instrument *inst = song->getInstrument(state.channel_instrument[channel]);
			if(inst != NULL)
			{
				inst->updateEnvelopePos(song->getBPM(), passed_time, channel, state.channel_note[channel]);
				state.channel_env_vol[channel] = inst->getEnvelopeAmp(channel, state.channel_note[channel]);
			}
			else
				state.channel_env_vol[channel] = 64;
		}
	}

	// Update channel volumes
	for(u8 channel=0; channel<MAX_CHANNELS; ++channel)
	{
		if(state.channel_active[channel])
		{
			// The master formula!
			// The magic numbers are actually scaling factors to keep all volume ranges within 0x00 to 0x1f
			u8 chnvol;
			if (state.channel_fade_active[channel] == 1)
			{
				chnvol = (u8)(((state.channel_volume[channel]) * ((state.channel_env_vol[channel] << 8) / 0x210) * (state.channel_fade_vol[channel] << 8) / 0x418) / 0x3c1);
				} else {
				chnvol = (u8)((state.channel_volume[channel]) * ((state.channel_env_vol[channel] << 8) / 0x210) / 0x1f);
				}

			SCHANNEL_VOL(channel) = SOUND_VOL(chnvol);

			if(state.channel_active[channel] == CHANNEL_TO_BE_DISABLED)
			{
				state.channel_active[channel] = 0;
				SCHANNEL_CR(channel) = 0;
			}
		}
	}

	if(state.playing == false)
		return;

	if( state.juststarted == true ) // Play current row
	{
		state.juststarted = false;

		playRow();

		handleEffects();

		handleTickEffects();

		CommandUpdateRow(state.row);
	}

	// if the number of ms per tick is reached, go to the next tick
	if(state.tick_ms >= song->getMsPerTick())
	{
		// Go to the next tick
		state.row_ticks++;

		if(state.row_ticks >= song->getTempo())
		{
			state.row_ticks = 0;
			
			if (effstate.pattern_delay_store > 0)
			{
				effstate.pattern_delay = effstate.pattern_delay_store;
				effstate.pattern_delay_store = 0;
			}
			
			bool finished = calcNextPos(&state.row, &state.potpos);
			if(finished || state.waitrow)
			{
				stop();
				CommandNotifyStop();
				state.waitrow = false;
				return;
			}
			else
			{
				state.pattern = song->pattern_order_table[state.potpos];
			}

			if(effstate.pattern_break_requested == true)
				CommandUpdatePotPos(state.potpos);

			finishEffects();
			
			
			if(effstate.pattern_delay > 1)
			{
				effstate.pattern_delay--;
			}
			else
			{
				effstate.pattern_delay = 0;
				playRow();
			}

			handleEffects();

			CommandUpdateRow(state.row);

			if(state.row == 0) {
				CommandUpdatePotPos(state.potpos);
			}
		}

		handleTickEffects();
		for (u8 channel = 0; channel < MAX_CHANNELS; channel++)
		{
		  state.channel_prev_note[channel] = state. channel_note[channel];
		}
		
		state.tick_ms -= song->getMsPerTick();
	}
}

/* ===================== PRIVATE ===================== */

void Player::startPlayTimer(void)
{
	TIMER0_DATA = TIMER_FREQ_64(1000); // Call handler every millisecond
	TIMER0_CR = TIMER_ENABLE | TIMER_IRQ_REQ | TIMER_DIV_64;
}

void Player::playRow(void)
{
	// Play all notes in this row
	for(u8 channel=0; channel < song->n_channels && channel<MAX_CHANNELS; ++channel)
	{
		u8 note    = song->patterns[state.pattern][channel][state.row].note;
		u8 volume  = song->patterns[state.pattern][channel][state.row].volume;
		u8 instidx = song->patterns[state.pattern][channel][state.row].instrument;
		u8 effect  = song->patterns[state.pattern][channel][state.row].effect;
		u8 param   = song->patterns[state.pattern][channel][state.row].effect_param;
		u16 test_delay = (((effect << 8) & 0x0f00) | (param & 0xf0));

		if(instidx == NO_INSTRUMENT)
			instidx = state.channel_instrument[channel];
		else
			state.channel_instrument[channel] = instidx;

		effect = (effect >> 4) & 0xf;

		//Skip new note if doing porta to note, we'll slide towards it instead
		if((note!=EMPTY_NOTE)&&(note!=STOP_NOTE)&&(effect != EFFECT_PORTA_TONE)&&(test_delay != DELAY_CMD))
		{
			Instrument *inst = song->getInstrument(instidx);
			if(inst != NULL) {
				auto smp = inst->getSampleForNote(note);

				playNote(note, volume, channel, instidx);

				state.channel_active[channel] = 1;
				if(smp != NULL && smp->getLoop() != 0) {
					state.channel_loop[channel] = true;
					state.channel_ms_left[channel] = 0;
				} else {
					state.channel_loop[channel] = false;
					state.channel_ms_left[channel] = inst->calcPlayLength(note);
				}
			}
		}
		updateChannelVol(volume, channel);
	}
}

void Player::updateChannelVol(u8 volume, u8 channel)
{
	if(volume == NO_VOLUME) {
		return;
	} else {
		state.channel_volume[channel] = volume * state.channel_prev_sample_vol[channel] / 255;
	}
}

void Player::handleEffects(void)
{
	effstate.pattern_loop_jump_now = false;
	effstate.pattern_break_requested = false;
	effstate.position_jump_requested = false;

	for(u8 channel=0; channel < song->n_channels && channel<MAX_CHANNELS; ++channel)
	{
		u8 effect = song->patterns[state.pattern][channel][state.row].effect;
		u8 param  = song->patterns[state.pattern][channel][state.row].effect_param;
		u8 instidx = state.channel_instrument[channel];
		Instrument *inst = song->getInstrument(instidx);
		
		if(effect != NO_EFFECT)
		{
			switch(effect)
			{
				case(EFFECT_E): // If the effect is E, the effect type is specified in the 1st param nibble
				{
					u8 e_effect_type  = (param >> 4);
					u8 e_effect_param = (param & 0x0F);

					switch(e_effect_type)
					{
						case (EFFECT_E_FINE_PORTA_UP):
						{
							if (inst == NULL)
								continue;
					
							if (e_effect_param != 0)
								state.channel_fine_porta_increment[channel] = (u16) e_effect_param;
							
							if (state.channel_porta_enabled[channel] == false)
							{
								state.channel_porta_enabled[channel] = true;
								u8 note = state.channel_note[channel];
								u8 rel = inst->getSampleForNote(note)->getRelNote();
								s8 fine = inst->getSampleForNote(note)->getFinetune();
								note += (48 + rel);
								state.channel_porta_accumulator[channel] = (s32)((128 * note) + fine) << PORTA_PRECISION; // 128 is max finesteps per note
							}
							
							state.channel_porta_accumulator[channel] += PORTA_FIX(state.channel_fine_porta_increment[channel]);
							if (state.channel_porta_accumulator[channel] > (19968 << PORTA_PRECISION))
							{
								state.channel_porta_accumulator[channel] = (19968 << PORTA_PRECISION);
							}
							inst->bendNoteDirect(state.channel_note[channel], state.channel_porta_accumulator[channel] >> PORTA_PRECISION, channel);
							break;
						}

						case (EFFECT_E_FINE_PORTA_DOWN):
						{
							if (inst == NULL)
								continue;
					
							if (e_effect_param != 0)
								state.channel_porta_decrement[channel] = (u16) e_effect_param;
							
							if (state.channel_porta_enabled[channel] == false)
							{
								state.channel_porta_enabled[channel] = true;
								u8 note = state.channel_note[channel];
								u8 rel = inst->getSampleForNote(note)->getRelNote();
								s8 fine = inst->getSampleForNote(note)->getFinetune();
								note += (48 + rel);
								state.channel_porta_accumulator[channel] = (s32)((128 * note) + fine) << PORTA_PRECISION; // 128 is max finesteps per note
							}
							
							state.channel_porta_accumulator[channel] -= PORTA_FIX(state.channel_porta_decrement[channel]);
							if (state.channel_porta_accumulator[channel] < 0)
							{
								state.channel_porta_accumulator[channel] = 0;
							}
							inst->bendNoteDirect(state.channel_note[channel], state.channel_porta_accumulator[channel] >> PORTA_PRECISION, channel);
							break;
						}
						case(EFFECT_E_SET_LOOP):
						{
							// If param is 0, the loop start is set at the current row.
							// If param is >0, the loop end is set at the current row and
							// the effect param is the loop count
							if(e_effect_param == 0)
							{
								effstate.pattern_loop_begin = state.row;
							}
							else
							{
								if(effstate.pattern_loop_count > 0) // If we are already looping
								{
									effstate.pattern_loop_count--;
									if(effstate.pattern_loop_count == 0) {
										effstate.pattern_loop_begin = 0;
									}
								} else {
									effstate.pattern_loop_count = e_effect_param;
								}

								if(effstate.pattern_loop_count > 0)
								{
									effstate.pattern_loop_jump_now = true;
								}
							}
							break;
						}
						
						case(EFFECT_E_PATTERN_DELAY):
						{
							if (effstate.pattern_delay == 0)
							{
								effstate.pattern_delay_store = e_effect_param + 1;
							}
							break;
						}
					}

					break;
				}

				case EFFECT_POSITION_JUMP:
				{
					effstate.pattern_break_requested = true;
					effstate.position_jump_requested = true;
					effstate.pattern_break_row = 0;
					effstate.position_jump_pos = param;
					break;
				}

				case EFFECT_SET_VOLUME:
				{
					u8 target_volume = MIN(MAX_VOLUME, param * 2);

					// Request volume change and set target volume
					effstate.channel_setvol_requested[channel] = true;
					state.channel_fade_target_volume[channel] = target_volume;

					break;
				}

				case EFFECT_PATTERN_BREAK:
				{
					// The row at which the next pattern is continued
					// is calculated strangely:
					u8 b1, b2, newrow;
					b1 = param >> 4;
					b2 = param & 0x0F;

					newrow = b1 * 10 + b2;

					effstate.pattern_break_requested = true;
					effstate.pattern_break_row = newrow;
					break;
				}

				case EFFECT_SET_SPEED_TEMPO:
				{
					if(param < 0x20)
					{
						song->setTempo(param);
					}
					else {
						song->setBpm(param);
						state.tick_ms = song->getMsPerTick();
					}
					break;
				}

				case EFFECT_PORTA_UP:
				{
					if (inst == NULL)
						continue;
					
					if (param != 0)
						state.channel_porta_increment[channel] = (u16) param;
					
					if (state.channel_porta_enabled[channel] == false)
					{
						state.channel_porta_enabled[channel] = true;
						u8 note = state.channel_note[channel];
						u8 rel = inst->getSampleForNote(note)->getRelNote();
						s8 fine = inst->getSampleForNote(note)->getFinetune();
						note += (48 + rel); 	// Add 48 to the note, because otherwise note can get negative (later on)
																	// Also add rel note and finetune from sample settings so the effect won't be out of tune
						state.channel_porta_accumulator[channel] = (s32)((128 * note) + fine) << PORTA_PRECISION; // 128 is max finesteps per note
					}
					break;
				}

				case EFFECT_PORTA_DOWN:
				{
					if (inst == NULL)
						continue;
					
					if (param != 0)
						state.channel_porta_decrement[channel] = (u16) param;
					
					if (state.channel_porta_enabled[channel] == false)
					{
						state.channel_porta_enabled[channel] = true;
						u8 note = state.channel_note[channel];
						u8 rel = inst->getSampleForNote(note)->getRelNote();
						s8 fine = inst->getSampleForNote(note)->getFinetune();
						note += (48 + rel); 	// Add 48 to the note, because otherwise note can get negative (later on)
																	// Also add rel note and finetune from sample settings so the effect won't be out of tune
						state.channel_porta_accumulator[channel] = (s32)((128 * note) + fine) << PORTA_PRECISION; // 128 is max finesteps per note
					}
					break;
				}

				case EFFECT_PORTA_TONE:
				{
					if (inst == NULL)
						continue;
					
					if (param != 0)
						state.channel_porta_tone_increment[channel] = (u16) param;
					
					if (state.channel_porta_enabled[channel] == false)
					{
						state.channel_porta_enabled[channel] = true;
						u8 note = state.channel_prev_note[channel];
						u8 rel = inst->getSampleForNote(note)->getRelNote();
						s8 fine = inst->getSampleForNote(note)->getFinetune();
						note += (48 + rel); 	// Add 48 to the note, because otherwise note can get negative (later on)
																	// Also add rel note and finetune from sample settings so the effect won't be out of tune
						state.channel_porta_accumulator[channel] = (s32)((128 * note) + fine) << PORTA_PRECISION; // 128 is max finesteps per note
						note = state.channel_note[channel];
						note += (48 + rel); 

						//Target note uses first note's sample, so we'll borrow it's finetune and rel note
						// to stay in tune
						state.channel_porta_tone_target[channel] = (s32)((128 * note) + fine) << PORTA_PRECISION;

						if (state.channel_porta_tone_target[channel] > state.channel_porta_accumulator[channel])
						{
							state.channel_porta_up[channel] = true;
						} else if (state.channel_porta_tone_target[channel] < state.channel_porta_accumulator[channel])
						{
							state.channel_porta_up[channel] = false;
						} else {
							//it's equal.... don't do porta
							state.channel_porta_tone_increment[channel] = 0;
						}
					}
					break;
				}

				case EFFECT_VIBRATO:
				{
					state.channel_vib_phase_increment[channel] = (u8)(((((param >> 4) & 0x0f) * 0x280) - 0x100) >> 8);
					break;
				}

				case EFFECT_SET_PAN:
				{
					if (inst == NULL)
						continue;
					u8 inst = state.channel_instrument[channel];
					u8 note = state.channel_note[channel];
					song->instruments[inst]->getSampleForNote(note)->setPanning(param);
					song->instruments[inst]->getSampleForNote(note)->updatePanning(channel);
					break;
				}
				
				case EFFECT_EXTRA_FINE_PORTAMENTO:
				{
					u8 extra_fine_type  = (param >> 4);
					u8 extra_fine_param = (param & 0x0F);

					switch(extra_fine_type)
					{
						case (EFFECT_EXTRA_FINE_PORTAMENTO_UP):
						{
							if (inst == NULL)
								continue;
							
							if (extra_fine_param != 0)
								state.channel_exfine_porta_increment[channel] = (u16) extra_fine_param;
							
							if (state.channel_porta_enabled[channel] == false)
							{
								state.channel_porta_enabled[channel] = true;
								u8 note = state.channel_note[channel];
								u8 rel = inst->getSampleForNote(note)->getRelNote();
								s8 fine = inst->getSampleForNote(note)->getFinetune();
								note += (48 + rel);
								state.channel_porta_accumulator[channel] = (s32)((128 * note) + fine) << PORTA_PRECISION; // 128 is max finesteps per note
							}
							
							state.channel_porta_accumulator[channel] += PORTA_FIX(state.channel_exfine_porta_increment[channel]) >> 2;
							if (state.channel_porta_accumulator[channel] > (19968 << PORTA_PRECISION))
							{
								state.channel_porta_accumulator[channel] = (19968 << PORTA_PRECISION);
							}
							inst->bendNoteDirect(state.channel_note[channel], state.channel_porta_accumulator[channel] >> PORTA_PRECISION, channel);
							break;
						}

						case (EFFECT_EXTRA_FINE_PORTAMENTO_DOWN):
						{
							if (inst == NULL)
								continue;
							
							if (extra_fine_param != 0)
								state.channel_exfine_porta_decrement[channel] = (u16) extra_fine_param;
							
							if (state.channel_porta_enabled[channel] == false)
							{
								state.channel_porta_enabled[channel] = true;
								u8 note = state.channel_note[channel];
								u8 rel = inst->getSampleForNote(note)->getRelNote();
								s8 fine = inst->getSampleForNote(note)->getFinetune();
								note += (48 + rel);
								state.channel_porta_accumulator[channel] = (s32)((128 * note) + fine) << PORTA_PRECISION; // 128 is max finesteps per note
							}
							
							state.channel_porta_accumulator[channel] -= PORTA_FIX(state.channel_exfine_porta_decrement[channel]) >> 2;
							if (state.channel_porta_accumulator[channel] < 0)
							{
								state.channel_porta_accumulator[channel] = 0;
							}
							inst->bendNoteDirect(state.channel_note[channel], state.channel_porta_accumulator[channel] >> PORTA_PRECISION, channel);
							break;
						}
					}
					
					break;
				}
			}
		}
	}
}

void Player::handleTickEffects(void)
{
	for(u8 channel=0; channel < song->n_channels && channel<MAX_CHANNELS; ++channel)
	{
		u8 effect  = song->patterns[state.pattern][channel][state.row].effect;
		u8 param   = song->patterns[state.pattern][channel][state.row].effect_param;
		u8 instidx = state.channel_instrument[channel];
		Instrument *inst = song->getInstrument(instidx);

		if(effect != NO_EFFECT)
		{
			state.channel_effect[channel] = effect;
			state.channel_effect_param[channel] = param;

			switch(effect)
			{
				case(EFFECT_ARPEGGIO):
				{
					u8 halftone1, halftone2;
					halftone2 = (param & 0xF0) >> 4;
					halftone1 = param & 0x0F;

					if (inst == NULL)
						continue;

					switch(state.row_ticks % 3)
					{
						case(0):
							inst->bendNote(state.channel_note[channel] + 0,
									state.channel_note[channel], 0, channel);
							break;
						case(1):
							inst->bendNote(state.channel_note[channel] + halftone1,
									state.channel_note[channel], 0, channel);
							break;
						case(2):
							inst->bendNote(state.channel_note[channel] + halftone2,
									state.channel_note[channel], 0, channel);
							break;
					}

					break;
				}

				case(EFFECT_PORTA_UP):
				{
					if (inst == NULL)
						continue;

					state.channel_porta_accumulator[channel] += PORTA_FIX(state.channel_porta_increment[channel]);
					if (state.channel_porta_accumulator[channel] > (19968 << PORTA_PRECISION))
					{
						state.channel_porta_accumulator[channel] = (19968 << PORTA_PRECISION);
					}
					inst->bendNoteDirect(state.channel_note[channel], state.channel_porta_accumulator[channel] >> PORTA_PRECISION, channel);
					break;
				}

				case(EFFECT_PORTA_DOWN):
				{
					if (inst == NULL)
						continue;

					state.channel_porta_accumulator[channel] -= PORTA_FIX(state.channel_porta_decrement[channel]);
					if (state.channel_porta_accumulator[channel] < 0)
					{
						state.channel_porta_accumulator[channel] = 0;
					}
					inst->bendNoteDirect(state.channel_note[channel], state.channel_porta_accumulator[channel] >> PORTA_PRECISION, channel);
					break;
				}

				case(EFFECT_PORTA_TONE):
				{
					if (inst == NULL)
						continue;

					if (state.channel_porta_up[channel] == true)
					{
						state.channel_porta_accumulator[channel] += PORTA_FIX(state.channel_porta_tone_increment[channel]);
						if (state.channel_porta_accumulator[channel] > state.channel_porta_tone_target[channel])
						{
							state.channel_porta_accumulator[channel] = state.channel_porta_tone_target[channel];
						}
					} else {
						state.channel_porta_accumulator[channel] -= PORTA_FIX(state.channel_porta_tone_increment[channel]);
						if (state.channel_porta_accumulator[channel] < state.channel_porta_tone_target[channel])
						{
							state.channel_porta_accumulator[channel] = state.channel_porta_tone_target[channel];
						}
					}

					if (state.channel_porta_accumulator[channel] < 0)
					{
						state.channel_porta_accumulator[channel] = 0;
					}

					if (state.channel_porta_accumulator[channel] > (19968 << PORTA_PRECISION))
					{
						state.channel_porta_accumulator[channel] = (19968 << PORTA_PRECISION);
					}
					inst->bendNoteDirect(state.channel_note[channel], state.channel_porta_accumulator[channel] >> PORTA_PRECISION, channel);
					break;
				}

				case(EFFECT_VIBRATO):
				{
					if (inst == NULL)
						continue;

					u8 note = state.channel_note[channel];
					u8 vib_depth = (param & 0x0f);
					s16 fine = ((s16)(vibrato_sine_table[state.channel_vib_accumulator[channel]] * vib_depth) / 2);
					inst->bendNote(note, note, fine, channel);
					state.channel_vib_accumulator[channel] += state.channel_vib_phase_increment[channel];
					break;
				}

				case(EFFECT_E): // If the effect is E, the effect type is specified in the 1st param nibble
				{
					u8 e_effect_type  = (param >> 4);
					u8 e_effect_param = (param & 0x0F);

					switch(e_effect_type)
					{
						case(EFFECT_E_NOTE_CUT):
						{
							if(e_effect_param == state.row_ticks)
							{
								effstate.channel_setvol_requested[channel] = true;
								state.channel_fade_target_volume[channel] = 0;
							}
							break;
						}

						case(EFFECT_E_NOTE_DELAY):
						{
							if (inst == NULL)
								continue;

							if (state.row_ticks == (e_effect_param))
							{
								u8 note   = song->patterns[state.pattern][channel][state.row].note;
								u8 volume = song->patterns[state.pattern][channel][state.row].volume;
								u8 inst   = song->patterns[state.pattern][channel][state.row].instrument;
								playNote(note, volume, channel, inst);

								state.channel_active[channel] = 1;
								if(song->instruments[inst]->getSampleForNote(note)->getLoop() != 0) {
									state.channel_loop[channel] = true;
									state.channel_ms_left[channel] = 0;
								} else {
									state.channel_loop[channel] = false;
									state.channel_ms_left[channel] = song->instruments[inst]->calcPlayLength(note);
								}
							}
							break;
						}
					}
					break;
				}

				case(EFFECT_VOLUME_SLIDE):
				{
					if(state.row_ticks == 0) //
						break;

					s8 slidespeed;

					if(param == 0)
						slidespeed = effstate.channel_last_slidespeed[channel];
					else if( (param & 0x0F) == 0 )
						slidespeed = (param >> 4) * 2;
					else
						slidespeed = -(param & 0x0F) * 2;

					effstate.channel_last_slidespeed[channel] = slidespeed;

					s16 targetvolume = state.channel_volume[channel] + slidespeed;
					if(targetvolume > MAX_VOLUME)
						targetvolume = MAX_VOLUME;
					else if(targetvolume < 0)
						targetvolume = 0;

					effstate.channel_setvol_requested[channel] = true;
					state.channel_fade_target_volume[channel] = targetvolume;

					break;
				}

			}
		}
	}
}

void Player::finishEffects(void)
{
	for(u8 channel = 0; channel < song->n_channels && channel<MAX_CHANNELS; ++channel)
	{
		u8 effect = state.channel_effect[channel];
		u8 new_effect = song->patterns[state.pattern][channel][state.row].effect;
		u8 instidx = state.channel_instrument[channel];
		Instrument *inst = song->getInstrument(instidx);

		if( (effect != NO_EFFECT) && (new_effect != effect) )
		{
			switch(effect)
			{
				case(EFFECT_ARPEGGIO):
				{
					if (inst == NULL)
						continue;

					// Reset note
					inst->bendNote(state.channel_note[channel] + 0,
									state.channel_note[channel], 0, channel);

					break;
				}
				
				case(EFFECT_PORTA_UP):
				{
				  break;
				}
				
				case(EFFECT_PORTA_DOWN):
				{
				  break;
				}
				
				case(EFFECT_VIBRATO):
				{
					if (inst == NULL)
						continue;
					resetVibrato(channel);
					break;
				}
			}
		}
	}
}

void Player::initState(void)
{
	state.row = 0;
	state.pattern = 0;
	state.potpos = 0;
	state.playing = false;
	state.songloop = true;
	state.waitrow = false;
	state.patternloop = false;
	memset(state.channel_active, 0, sizeof(state.channel_active));
	memset(state.channel_ms_left, 0, sizeof(state.channel_ms_left));
	memset(state.channel_note, EMPTY_NOTE, sizeof(state.channel_note));
	memset(state.channel_prev_note, EMPTY_NOTE, sizeof(state.channel_prev_note));
	memset(state.channel_instrument, NO_INSTRUMENT, sizeof(state.channel_instrument));
	memset(state.channel_effect, NO_EFFECT, sizeof(state.channel_effect));
	memset(state.channel_effect_param, NO_EFFECT_PARAM, sizeof(state.channel_effect_param));
	memset(state.channel_fade_active, 0, sizeof(state.channel_fade_active));
	memset(state.channel_fade_ms, 0, sizeof(state.channel_fade_ms));
	memset(state.channel_fade_target_volume, 0, sizeof(state.channel_fade_target_volume));
	memset(state.channel_volume, 0, sizeof(state.channel_volume));
	memset(state.channel_env_vol, 63, sizeof(state.channel_env_vol));
	memset(state.channel_fade_vol, 127, sizeof(state.channel_fade_vol));
	memset(state.channel_prev_sample_vol, 0, sizeof(state.channel_prev_sample_vol));
	memset(state.channel_porta_accumulator, 0, sizeof(state.channel_porta_accumulator));
	memset(state.channel_porta_tone_target, 0, sizeof(state.channel_porta_tone_target));
	memset(state.channel_porta_tone_increment, 0, sizeof(state.channel_porta_tone_increment));
	memset(state.channel_porta_increment, 0, sizeof(state.channel_porta_increment));
	memset(state.channel_porta_decrement, 0, sizeof(state.channel_porta_decrement));
	memset(state.channel_fine_porta_increment, 0, sizeof(state.channel_fine_porta_increment));
	memset(state.channel_fine_porta_decrement, 0, sizeof(state.channel_fine_porta_decrement));
	memset(state.channel_exfine_porta_increment, 0, sizeof(state.channel_exfine_porta_increment));
	memset(state.channel_exfine_porta_decrement, 0, sizeof(state.channel_exfine_porta_decrement));
	memset(state.channel_porta_up, false, sizeof(state.channel_porta_up));
	memset(state.channel_porta_enabled, false, sizeof(state.channel_porta_enabled));
	memset(state.channel_vib_accumulator, 0, sizeof(state.channel_vib_accumulator));
	memset(state.channel_vib_phase_increment, 0, sizeof(state.channel_vib_phase_increment));
	memset(state.channel_vib_depth, 0, sizeof(state.channel_vib_depth));
	memset(state.channel_tags, 0xff, sizeof(state.channel_tags));  // fill with UNTAGGED (u16)
	state.playing_single_sample = false;
	state.single_sample_ms_remaining = 0;
	state.single_sample_channel = 0;
}

void Player::initEffState(void)
{
	effstate.pattern_loop_begin = 0;
	effstate.pattern_loop_count = 0;
	effstate.pattern_loop_jump_now = false;
	memset(effstate.channel_setvol_requested, false, sizeof(effstate.channel_setvol_requested));
	memset(effstate.channel_last_slidespeed, 0, sizeof(effstate.channel_last_slidespeed));
	effstate.pattern_break_requested = false;
	effstate.position_jump_requested = false;
	effstate.pattern_break_row = 0;
	effstate.position_jump_pos = 0;
	effstate.pattern_delay_store = 0;
	effstate.pattern_delay = 0;
}

void Player::initDefaultPanning(void)
{
	u8 instcount = song->getInstruments();
	u16 smpidx = 0;
	Instrument *inst;
	
	for ( u8 i = 0; i < instcount; i++)
	{
		inst = song->instruments[i];
		if (inst == 0) continue;
		smpidx = inst->getSamples();
		for (u16 j = 0; j < smpidx; j++)
		{
			inst->getSample(j)->setBasePanning();
		}
	}
}

void Player::resetPanning(void)
{
	u8 instcount = song->getInstruments();
	u16 smpidx = 0;
	Instrument *inst;
	u8 base_pan = 0;

	for ( u8 i = 0; i < instcount; i++)
	{
		inst = song->instruments[i];
		if (inst == 0) continue;
		smpidx = inst->getSamples();
		for (u16 j = 0; j < smpidx; j++)
		{
			base_pan = inst->getSample(j)->getBasePanning();
			inst->getSample(j)->setPanning(base_pan);
		}
	}
}

void Player::resetVibrato(u8 channel)
{
	state.channel_vib_accumulator[channel] = 0;
	state.channel_vib_phase_increment[channel] = 0;
	state.channel_vib_depth[channel] = 0;
}

void Player::handleFade(u32 passed_time)
{
	// Find channels that need to be faded
	for(u8 channel=0; channel<MAX_CHANNELS; ++channel)
	{
		if(state.channel_fade_active[channel] == 1)
		{
			// Decrement ms until fade is complete
			if(state.channel_fade_ms[channel] > passed_time)
				state.channel_fade_ms[channel] -= passed_time;
			else
				state.channel_fade_ms[channel] = 0;

			// Calculate volume from initial volume, target volume and remaining fade time
			// Can be done way quicker using fixed point
			/*
			float fslope = (float)(state.channel_volume[channel] - state.channel_fade_target_volume[channel])
					/ (float)FADE_OUT_MS;

			float fvolume = (float)state.channel_fade_target_volume[channel]
					+ fslope * (float)(state.channel_fade_ms[channel]);
			*/
			int fvol_diff = state.channel_volume[channel] - state.channel_fade_target_volume[channel];
			int fslope = (fvol_diff << 12) / FADE_OUT_MS; // 20.12
			int fvolume = (state.channel_fade_target_volume[channel] << 12) + (fslope * state.channel_fade_ms[channel]); // 20.12

			u8 volume = (u8) (fvolume >> 12);

			state.channel_fade_vol[channel] = volume;

			// If we reached 0 ms, disable the fader (and the channel)
			if(state.channel_fade_ms[channel] == 0)
			{
				state.channel_fade_active[channel] = 0;

				state.channel_volume[channel] = state.channel_fade_target_volume[channel];

				// Set channel volume to target volume just to be sure
				state.channel_fade_vol[channel] = state.channel_fade_target_volume[channel];

				if(state.channel_volume[channel] == 0)
					state.channel_active[channel] = CHANNEL_TO_BE_DISABLED;
			}
		}
	}
}

bool Player::calcNextPos(u16 *nextrow, u8 *nextpotpos) // Calculate next row and pot position
{
	if(effstate.pattern_delay > 1)
	{
		*nextrow = state.row;
		*nextpotpos = state.potpos;
		
		return false;
	}
	
	if(effstate.pattern_loop_jump_now == true)
	{
		*nextrow = effstate.pattern_loop_begin;
		*nextpotpos = state.potpos;

		return false;
	}

	if(effstate.pattern_break_requested == true)
	{
		*nextrow = effstate.pattern_break_row;
		
		int next_pos = effstate.position_jump_requested ? effstate.position_jump_pos : state.potpos + 1;

		if(next_pos < song->getPotLength())
			*nextpotpos = next_pos;
		else
			*nextpotpos = song->getRestartPosition();

		return false;
	}

	if(state.row + 1 >= song->patternlengths[state.pattern])
	{
		if(state.patternloop == false) // Don't jump when looping is enabled
		{
			if(state.potpos < song->getPotLength() - 1)
				*nextpotpos = state.potpos + 1;
			else if(state.songloop == true)
				*nextpotpos = song->getRestartPosition();
			else
				return true;
		} else {
			*nextpotpos = state.potpos;
		}
		*nextrow = 0;
	}
	else
	{
		*nextrow = state.row + 1;
		*nextpotpos = state.potpos;
	}

	return false;
}
