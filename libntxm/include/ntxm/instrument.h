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

#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include "sample.h"

#define MAX_OCTAVE 9
#define MAX_NOTE (MAX_OCTAVE * 12 - 1)

#define INST_SAMPLE 0
#define INST_SYNTH 1 // Yes, who knows!

#define MAX_INST_NAME_LENGTH 22

#define MAX_ENV_X 324
#define MAX_ENV_Y 64
#define MAX_ENV_POINTS 12

#define STOP_NOTE 254

class Instrument
{
	friend class EnvelopeEditor;
	friend class XMTransport;

public:
	Instrument(const char *_name, u8 _type = INST_SAMPLE, u8 _volume = 255);
	Instrument(const char *_name, Sample *_sample, u8 _volume = 255);
	~Instrument();

	void addSample(Sample *sample);
	Sample *getSample(u8 idx); // If not present, 0 is returned
	void setSample(u8 idx, Sample *sample);
	Sample *getSampleForNote(u8 _note);
	void setNoteSample(u16 note, u8 sample_id);
	u8 getNoteSample(u16 note);
	void setVolEnvEnabled(bool is_enabled);
	inline bool getVolEnvEnabled(void) { return vol_env_on; }
	void setPanEnvEnabled(bool is_enabled);
	inline bool getPanEnvEnabled(void) { return pan_env_on; }

	inline u8 getVibratoType(void) { return vibrato_type; }
	inline u8 getVibratoSweep(void) { return vibrato_sweep; }
	inline u8 getVibratoDepth(void) { return vibrato_depth; }
	inline u8 getVibratoRate(void) { return vibrato_rate; }
	inline u16 getFadeOutVolume(void) { return fadeout_vol; }
	inline bool getMute(void) { return mute; }

	// Calculate how long in ms the instrument will play note given note
	u32 calcPlayLength(u8 note);

	const char *getName(void);
	void setName(const char *_name);

	u16 getSamples(void);

	void setVolumeEnvelope(u16 *envelope, u8 n_points, u8 v_sustain_point,
	                       bool vol_env_on_, bool vol_env_sustain_,
	                       bool vol_env_loop_);
	void setPanningEnvelope(u16 *envelope, u8 n_points, u8 p_sustain_point,
	                        bool pan_env_on_, bool pan_env_sustain_,
	                        bool pan_env_loop_);
	void setVibrato(u8 type, u8 sweep, u8 depth, u8 rate);
	void setFadeOutVolume(u16 value);
	void setMute(bool value);

	void setVolumeEnvelopePoints(u16 *xs, u16 *ys, u16 n_points);
	void toggleVolumeEnvelopeSustain(bool is_enabled);
	void setVolumeEnvelopeSustainPoint(u8 sus_point);

	void setPanningEnvelopePoints(u16 *xs, u16 *ys, u16 n_points);
	void togglePanningEnvelopeSustain(bool is_enabled);
	void setPanningEnvelopeSustainPoint(u8 sus_point);

	u16 getVolumeEnvelope(u16 **xs, u16 **ys);
	u16 getPanningEnvelope(u16 **xs, u16 **ys);

	inline bool getVolumeEnvelopeSustainFlag(void) { return vol_env_sustain; }

	inline u8 getVolumeEnvelopeSustainPoint(void) { return vol_sustain_point; }

	inline bool getPanningEnvelopeSustainFlag(void) { return pan_env_sustain; }

	inline u8 getPanningEnvelopeSustainPoint(void) { return pan_sustain_point; }

	void updateEnvelopePos(u8 bpm, u8 ms_passed, u8 channel, u8 note);
	u16 getEnvelopeAmp(u8 channel, u8 note);

private:
	char *name;

	u8 type;
	u8 volume;

	// Actually this would become dirty if more types emerge.
	// I think making an abstract Instrument base class that
	// subclasses SampleInstrument and SynthInstrument would
	// be derived from would be the best choice. But I'll do
	// this when required ^^

	Sample **samples;
	u16 n_samples;

	// Synth *synth;

	u8 *note_samples;

	u16 vol_envelope_x[MAX_ENV_POINTS];
	u16 vol_envelope_y[MAX_ENV_POINTS];
	u8 n_vol_points;
	bool vol_env_on;
	bool vol_env_sustain;
	bool vol_env_loop;
	u8 vol_sustain_point;

	u16 pan_envelope_x[MAX_ENV_POINTS];
	u16 pan_envelope_y[MAX_ENV_POINTS];
	u8 n_pan_points;
	bool pan_env_on;
	bool pan_env_sustain;
	bool pan_env_loop;
	u8 pan_sustain_point;

	u8 vol_loop_start_point;
	u8 vol_loop_end_point;
	u8 pan_loop_start_point;
	u8 pan_loop_end_point;
	u8 vibrato_type;
	u8 vibrato_sweep;
	u8 vibrato_depth;
	u8 vibrato_rate;
	u16 fadeout_vol;

	bool mute;

	u16 envelope_ms[MAX_CHANNELS];
	u16 envelope_pixels[MAX_CHANNELS]; // Pixel of the FT2 envelope editor :-)

	friend class Player;
};

#endif
