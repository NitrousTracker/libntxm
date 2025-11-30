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

#ifndef SAMPLE_H
#define SAMPLE_H

#include <nds.h>
#include "wav.h"
#include "linear_freq_table.h"

#define BASE_NOTE				96	// Index if C-4 (FT2 base note)
#define SILENCE_THRESHOLD_16	2000
#define CROP_IGNORE_START		200

#define NO_VOLUME				255

enum LoopType
{
	NO_LOOP = 0,
	FORWARD_LOOP = 1,
	PING_PONG_LOOP = 2,
	LOOP_TYPE_COUNT = 3
};

#define SAMPLE_NAME_LENGTH		24

#define FT_OFFSET_SCALAR 256

class Sample
{
	public:
		Sample(void *_sound_data, u32 _n_samples, u16 _sampling_frequency=44100,
			bool _is_16_bit=true, u8 _loop=NO_LOOP, u8 _volume=255);
		Sample(const char *filename, u8 _loop, bool *_success);
		~Sample();

		void saveAsWav(char *filename);

		void play(u8 note, u8 volume_, u8 channel  /* effects here */, u8 offs = 0);
		void bendNote(u8 note, u8 basenote, s16 _finetune, u8 channel);
		void bendNoteDirect(s16 fine_step, u8 channel);
		u32 calcPlayLength(u8 note);

		void setRelNote(s8 _rel_note);
		void setFinetune(s8 _finetune);

		u8 getRelNote(void);
		s8 getFinetune(void);

		u32 getSize(void); // Get the size in bytes
		u32 getNSamples(void); // Get the numer of (PCM) samples

		void *getData(void);
		u32 getMaxAmplitude(u32 startsample, u32 endsample);
		u32 getDynamicRange(void);
		
		u8 getLoop(void); // 0: no loop, 1: loop, 2: ping pong loop
		bool setLoop(u8 loop_); // Set loop type. Can fail due to memory constraints
		bool is16bit(void);

		u32 getLoopStart(void); // in samples
		u32 getLoopLength(void); // in samples

		// Sets loop start and length, arguments are given in samples
		void setLoopStartAndLength(u32 _loop_start, u32 _loop_length);

		void setVolume(u8 vol);
		u8 getVolume(void);

		void setPanning(u8 pan);
		u8 getPanning(void);
		void setBasePanning(void);
		u8 getBasePanning(void);
		void updatePanning(u8 channel);

		void setName(const char *name_);
		const char *getName(void);

		// Deletes the part between start sample and end sample
		void delPart(u32 startsample, u32 endsample);
		void delAll(void);
		void fadeIn(u32 startsample, u32 endsample);
		void fadeOut(u32 startsample, u32 endsample);
		bool reverse(u32 startsample, u32 endsample);
		void normalize(u16 percent, u32 startsample, u32 endsample);
		

		// Draws a line into the sample
		void drawLine(int x1, int y1, int x2, int y2);
		//void cutSilence(void); // Heuristically cut silence in the beginning

	private:
		void calcSize(void);
		void setFormat(void);
		void calcRelnoteAndFinetune(u32 freq);
		u16 findClosestFreq(u32 freq);
		bool convertStereoToMono(void);

		void fade(u32 startsample, u32 endsample, bool in);

		bool setupPingPongLoop(void);
		void removePingPongLoop(void);
		bool onSampleDataChanged(void);

		void *sound_data;
		void *pingpong_data;
		u32 n_samples;
		bool is_16_bit;
		u8 loop;
		s8 rel_note;		// Offset in the frequency table from base note
		s8 finetune;		// -128: one halftone down, +127: one halftone up
		u32 loop_start;		// In bytes, not in samples!
		u32 loop_length;	// In bytes, not in samples!
		u8 volume;
		u8 panning;
		u8 base_panning; // xm panning effects resets when a new note is played
		char name[SAMPLE_NAME_LENGTH + 1];

		// These are calculated in the constructor
		u32 size;
		u32 sound_format;

		Wav wav;
		// Other formats may follow
};

#endif
