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

#ifdef ARM9
#include <stdio.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "ntxm/instrument.h"
#include "ntxm/ntxmtools.h"
#include "ntxm/fifocommand.h"

#ifdef ARM9

Instrument::Instrument(const char *_name, u8 _type, u8 _volume)
	:type(_type), volume(_volume),
	 n_vol_points(0), vol_env_on(false), vol_env_sustain(false), vol_env_loop(false),
	 n_pan_points(0), pan_env_on(false), pan_env_sustain(false), pan_env_loop(false)
{
	name = (char*)ntxm_cmalloc(MAX_INST_NAME_LENGTH+1);
	name[MAX_INST_NAME_LENGTH] = 0;
	strncpy(name, _name, MAX_INST_NAME_LENGTH);
	
	note_samples = (u8*)ntxm_ccalloc(sizeof(u8)*MAX_OCTAVE*12, 1);
	
	samples = NULL;
	n_samples = 0;
}

Instrument::Instrument(const char *_name, Sample *_sample, u8 _volume)
	:type(INST_SAMPLE), volume(_volume),
	 n_vol_points(0), vol_env_on(false), vol_env_sustain(false), vol_env_loop(false),
	 n_pan_points(0), pan_env_on(false), pan_env_sustain(false), pan_env_loop(false)
{
	name = (char*)ntxm_cmalloc(MAX_INST_NAME_LENGTH+1);
	name[MAX_INST_NAME_LENGTH] = 0;
	strncpy(name, _name, MAX_INST_NAME_LENGTH);
	
	samples = (Sample**)ntxm_ccalloc(1, sizeof(Sample*)*1);
	samples[0] = _sample;
	n_samples = 1;
	
	note_samples = (u8*)ntxm_cmalloc(sizeof(u8)*MAX_OCTAVE*12);
	for(u16 i=0;i<MAX_OCTAVE*12; ++i)
		note_samples[i] = 0;
}

Instrument::~Instrument()
{
	for(u8 i=0;i<n_samples;++i) {
		if(samples[i] != NULL)
			delete samples[i];
	}
	if(samples != NULL)
		ntxm_free(samples);
	
	ntxm_free(note_samples);
	
	ntxm_free(name);
}

void Instrument::addSample(Sample *sample)
{
	n_samples++;
	samples = (Sample**)ntxm_crealloc(samples, sizeof(Sample*)*n_samples);
	samples[n_samples-1] = sample;
}

void Instrument::setSample(u8 idx, Sample *sample)
{
	// Delete the sample if it already exists
	if( (idx < n_samples) && (samples[idx] != 0) )
		delete samples[idx];
	
	// Resize sample list if necessary
	if(n_samples < idx + 1)
	{
		samples = (Sample**)ntxm_crealloc(samples, sizeof(Sample*) * (idx + 1));
		
		// Initialize new samples with 0
		while(n_samples < idx + 1)
		{
			samples[n_samples] = 0;
			++n_samples;
		}
	}
	
	samples[idx] = sample;
}

#endif

Sample *Instrument::getSample(u8 idx)
{
	if((n_samples>0) && (idx<n_samples))
		return samples[idx];
	else
		return NULL;
}

Sample *Instrument::getSampleForNote(u8 _note) {
	return samples[note_samples[_note]];
}

#ifdef ARM7

void Instrument::play(u8 _note, u8 _volume, u8 _channel /* effects here */)
{
	envelope_ms[_channel] = 0;
	envelope_pixels[_channel] = 0;
	
	if(_note > MAX_NOTE) {
		//CommandDbgOut("Note (%u) > MAX_NOTE (%u)\n",_note,MAX_NOTE);
		return;
	}
	
	u8 play_volume = volume * _volume / 255;
	
	if((vol_env_on)&&(n_vol_points>0))
		play_volume = play_volume * vol_envelope_y[0] / 64;
	
	switch(type) {
		case INST_SAMPLE:
			if( (n_samples > 0) && (samples[note_samples[_note]] != 0) )
				samples[note_samples[_note]]->play(_note, play_volume, _channel);
			break;
	}
}

void Instrument::bendNote(u8 _note, u8 _basenote, s16 _finetune, u8 _channel)
{
	if(_note > MAX_NOTE)
		return;
	
	switch(type) {
		case INST_SAMPLE:
			if(n_samples > 0)
				samples[note_samples[_note]]->bendNote(_note, _basenote, _finetune, _channel);
			break;
	}
}

void Instrument::bendNoteDirect(u8 _note, s16 _fine_step, u8 _channel)
{
	if(_fine_step > 19968) // 19968 is the highest intermediary pitch calculation
		return;								// before converted to an index value for lookup table
	
	switch(type) {
		case INST_SAMPLE:
			if(n_samples > 0)
				samples[note_samples[_note]]->bendNoteDirect(_fine_step, _channel);
			break;
	}
}

#endif

#ifdef ARM9

void Instrument::setNoteSample(u16 note, u8 sample_id) {
	note_samples[note] = sample_id;
}

#endif

u8 Instrument::getNoteSample(u16 note) {
	return note_samples[note];
}

#ifdef ARM9

void Instrument::setVolEnvEnabled(bool is_enabled)
{
	vol_env_on = is_enabled;
	DC_FlushAll();
}

#endif

bool Instrument::getVolEnvEnabled(void)
{
	return vol_env_on;
}

// Calculate how long in ms the instrument will play note given note
u32 Instrument::calcPlayLength(u8 note) {
	return samples[note_samples[note]]->calcPlayLength(note);
}

#ifdef ARM9

const char *Instrument::getName(void) {
	return name;
}

void Instrument::setName(const char *_name) {
	strncpy(name, _name, MAX_INST_NAME_LENGTH);
}

#endif

u16 Instrument::getSamples(void) {
	return n_samples;
}

#ifdef ARM9

void Instrument::setVolumeEnvelope(u16 *envelope, u8 n_points, u8 v_sustain_point, bool vol_env_on_, bool vol_env_sustain_, bool vol_env_loop_)
{
	n_vol_points = n_points;
	for(u8 i=0; i<n_points; ++i)
	{
		vol_envelope_x[i] = envelope[2*i];
		vol_envelope_y[i] = envelope[2*i+1];
	}
	
	vol_sustain_point = v_sustain_point;
	vol_env_on      = vol_env_on_;
	vol_env_sustain = vol_env_sustain_;
	vol_env_loop    = vol_env_loop_;
}

void Instrument::setPanningEnvelope(u16 *envelope, u8 n_points, u8 p_sustain_point,  bool pan_env_on_, bool pan_env_sustain_, bool pan_env_loop_)
{
	n_pan_points = n_points;
	for(u8 i=0; i<n_points; ++i)
	{
		pan_envelope_x[i] = envelope[2*i];
		pan_envelope_y[i] = envelope[2*i+1];
	}
	
	pan_sustain_point = p_sustain_point;
	pan_env_on      = pan_env_on_;
	pan_env_sustain = pan_env_sustain_;
	pan_env_loop    = pan_env_loop_;
}

void Instrument::setVolumeEnvelopePoints(u16 *xs, u16 *ys, u16 n_points)
{
	n_vol_points = n_points;
	for(u8 i=0; i<n_points; ++i)
	{
		vol_envelope_x[i] = xs[i];
		vol_envelope_y[i] = ys[i];
	}
}

void Instrument::setVolumeEnvelopeSustainPoint(u8 sus_point)
{
  vol_sustain_point = sus_point;
}

void Instrument::toggleVolumeEnvelopeSustain(bool is_enabled)
{
  vol_env_sustain = is_enabled;
}

u16 Instrument::getVolumeEnvelope(u16 **xs, u16 **ys)
{
	*xs = vol_envelope_x;
	*ys = vol_envelope_y;
	
	return n_vol_points;
}

u16 Instrument::getPanningEnvelope(u16 **xs, u16 **ys)
{
	*xs = pan_envelope_x;
	*ys = pan_envelope_y;
	
	return n_pan_points;
}

bool Instrument::getVolumeEnvelopeSustainFlag(void)
{
  return vol_env_sustain;
}

u8 Instrument::getVolumeEnvelopeSustainPoint(void)
{
  return vol_sustain_point;
}
#endif

void Instrument::updateEnvelopePos(u8 bpm, u8 ms_passed, u8 channel, u8 note)
{
	
	if ((note != STOP_NOTE) && ((vol_env_sustain == true) && (envelope_pixels[channel] >= vol_envelope_x[vol_sustain_point]) && (envelope_pixels[channel] < vol_envelope_x[vol_sustain_point + 1])))
	{
	  envelope_pixels[channel] = vol_envelope_x[vol_sustain_point];
	  return;
	}
	envelope_ms[channel] += ms_passed;
	envelope_pixels[channel] = envelope_ms[channel] * bpm * 50 / 120 / 1000; // 50 pixels per second at 120 BPM
}

u16 Instrument::getEnvelopeAmp(u8 channel, u8 note)
{
	if( (n_vol_points == 0) || (vol_env_on == false) )
		return 64;
	
	u8 envpoint = 0;
	while( (envpoint < n_vol_points - 1) && (envelope_pixels[channel] >= vol_envelope_x[envpoint+1]) )
	{
	    envpoint++;
	}
	
	if(envpoint >= n_vol_points - 1) // Last env point?
		return vol_envelope_y[envpoint];
	
	u16 x1 = vol_envelope_x[envpoint];
	u16 y1 = vol_envelope_y[envpoint];
	u16 x2 = vol_envelope_x[envpoint+1];
	u16 y2 = vol_envelope_y[envpoint+1];
	
	u16 rel_x = envelope_pixels[channel] - x1;
	
	u16 y = y1 + rel_x * (y2 - y1) / (x2 - x1);
	if(y > 64)
		y = 64;
	
	return y;
}
