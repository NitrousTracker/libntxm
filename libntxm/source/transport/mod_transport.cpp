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

#include "ntxm/mod_transport.h"

extern "C" {
    #include "../common/tables.h"
}

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ntxm/ntxmtools.h"

struct ModSampleInfo
{
	char name[23];
	u16 length;
	u8 finetune;
	u8 volume;
	u16 repeat_offset;
	u16 repeat_length;
};

static void wordToHost(u16& v)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    v = __builtin_bswap16(v);
#endif
}

#define MIN(x,y)	((x)<(y)?(x):(y))

// Loads a song from a file and puts it in the song argument
// returns 0 on success, an error code else
FormatTransportError ModTransport::load(const char *filename, Song **_song)
{
    u32 filesize = ntxm_getFileSize(filename);
	if(filesize == 0)
		return FormatTransportError::FILE_ZERO_BYTE;

	FILE *modfile = fopen(filename, "rb");
	if(!modfile)
		return FormatTransportError::FOPEN_FAIL;

	setvbuf(modfile, NULL, _IOFBF, 4096);

	// Read name

	char song_name[21];
	fread(song_name, 1, 20, modfile); song_name[20] = 0;
	ntxm_dprintf("It's called %s\n", song_name);

	// Read samples

	ModSampleInfo sampleinfo[31];

	for(int i=0; i<31; ++i)
	{
		fread(&sampleinfo[i].name, 1, 22, modfile); sampleinfo[i].name[22] = 0;
		fread(&sampleinfo[i].length, 2, 1, modfile); wordToHost(sampleinfo[i].length);
		fread(&sampleinfo[i].finetune, 1, 1, modfile);
		fread(&sampleinfo[i].volume, 1, 1, modfile);
		fread(&sampleinfo[i].repeat_offset, 2, 1, modfile); wordToHost(sampleinfo[i].repeat_offset);
		fread(&sampleinfo[i].repeat_length, 2, 1, modfile); wordToHost(sampleinfo[i].repeat_length);

		ntxm_dprintf("%s %d (%d/%d)\n", sampleinfo[i].name, sampleinfo[i].length, sampleinfo[i].repeat_offset, sampleinfo[i].repeat_length);
	}

	// Read header

	u8 potlen;
	fread(&potlen, 1, 1, modfile);

	u8 restart_pos;
	fread(&restart_pos, 1, 1, modfile);

	u8 pot[128];
	fread(pot, 1, 128, modfile);

	u32 fmt;
	fread(&fmt, 1, 4, modfile);

	// Parse the format tag
	int n_channels = 0;

	if(fmt == 0x2E4B2E4D || fmt == 0x214B214D) { // M.K., M!K!
	    n_channels = 4;
	} else if(fmt == 0x4154434F) { // OCTA
	    n_channels = 8;
	} else if((fmt >> 8) == 0x4E4843 && isdigit(fmt & 0xFF)) { // *CHN
	    n_channels = (fmt & 0xFF) - '0';
	} else if((fmt & 0xFFFFFF) == 0x544C46 && isdigit(fmt >> 24)) { // FLT*
	    n_channels = (fmt >> 24) - '0';
	} else if((fmt >> 16) == 0x4843 && isdigit(fmt & 0xFF) && isdigit((fmt >> 8) & 0xFF)) { // **CH
	    n_channels = (((fmt & 0xFF) - '0') * 10) + (((fmt >> 8) & 0xFF) - '0');
	}
	if(n_channels < 1 || n_channels > 32) {
	    fclose(modfile);
	    return FormatTransportError::MAGIC_NUMBER_INVALID;
	} else if(MAX_CHANNELS < 32 && n_channels > MAX_CHANNELS) {
        fclose(modfile);
        return FormatTransportError::TOO_MANY_CHANNELS;
	}

	Song *song = new Song(6, 125, n_channels, false);
	song->setName(song_name);
	song->setRestartPosition(restart_pos);

	int n_patterns = 0;
	song->setPotEntry(0, pot[0]);
	for(int i=0; i<128; ++i) {
	    if (i > 0 && i < potlen)
			song->potAdd(pot[i]);
		if(pot[i] >= n_patterns)
			n_patterns = pot[i]+1;
	}

	//
	// Read Patterns
	//
	u16 patterndata_size = 4 * n_channels * 64;

	u8 *ptn_data = (u8*)ntxm_ucalloc(patterndata_size, 1);
	if(!ptn_data)
	{
    	fclose(modfile);
    	delete song;
    	return FormatTransportError::MEM_FULL;
	}

	for(int i=0; i<n_patterns; ++i)
	{
		fread(ptn_data, patterndata_size, 1, modfile);

		if(i > 0)
		    song->addPattern();
		song->resizePattern(i, 64);
		Cell **ptn = song->getPattern(i);

		u8 *notedata = ptn_data;

		for(u8 row=0; row<64; ++row)
		{
			for(u8 chn=0; chn<n_channels; ++chn, notedata += 4)
			{
				u8 sample;
				u16 period, effect;
				sample = ( (notedata[0] >> 4) << 4 ) | ( notedata[2] >> 4 );
				period = ( ( notedata[0] & 0x0F ) << 8 ) | notedata[1];
				effect = ( ( notedata[2] & 0x0F ) << 8 ) | notedata[3];

				ptn[chn][row].instrument = period ? (sample - 1) : NO_INSTRUMENT;
				ptn[chn][row].note = EMPTY_NOTE;
				for (int n = 0; n < 96; n++) {
				    if (period >= amigaPeriod[n]) {
					    ptn[chn][row].note = n;
						break;
					}
				}

				u8 volume = NO_VOLUME;
				u8 effect_type = effect >> 8;
				u8 effect_param = effect & 0xFF;

				// Most effect conversion login from pmplay
				if (effect_type == 0xC) {
				    // Convert "Set note volume" to the volume column
					volume = (effect_param >= MAX_VOLUME ? MAX_VOLUME : effect_param) + 0x10;
					effect_type = NO_EFFECT;
				} else if (effect_type == 0x1 || effect_type == 0x2 || effect_type == 0xA) {
				    if (effect_param == 0)
						effect_type = NO_EFFECT;
				} else if (effect_type == 0x5 || effect_type == 0x6) {
				    if (effect_param == 0)
						effect_type -= 2;
				} else if (effect_type == 0xE) {
				    u8 effect_e = effect_param >> 4;
					if (effect_e == 1 || effect_e == 2 || effect_e == 0xA || effect_e == 0xB)
					    if (!(effect_param & 0xF))
							effect_type = NO_EFFECT;
				}

				ptn[chn][row].volume = volume;
				ptn[chn][row].effect = effect_type;
				ptn[chn][row].effect_param = effect_param;
			}
		}
	}

	ntxm_free(ptn_data);

	for(int i=0; i<31; ++i)
	{
		Instrument *inst = new Instrument(sampleinfo[i].name);
		if(!inst)
		{
			fclose(modfile);
			delete song;
			return FormatTransportError::MEM_FULL;
		}
		song->setInstrument(i, inst);

		void *sound_data = nullptr;
		if(sampleinfo[i].length)
		{
#if defined(NT_PLATFORM_NDS) || defined(NT_PLATFORM_3DS)
    		sound_data = ntxm_umemalign(4, ((sampleinfo[i].length << 1) + 3) & ~3);
#else
            sound_data = ntxm_umalloc(sampleinfo[i].length << 1);
#endif
    		if(!sound_data)
    		{
          		fclose(modfile);
          		delete song;
          		return FormatTransportError::MEM_FULL;
    		}
    		fread(sound_data, sampleinfo[i].length << 1, 1, modfile);
		}

		Sample *sample = new Sample(sound_data, sampleinfo[i].length << 1, 8363, false);
		if(!sample)
		{
		    ntxm_free(sound_data);
			fclose(modfile);
    		delete song;
    		return FormatTransportError::MEM_FULL;
		}

		sample->setVolume(sampleinfo[i].volume);
		sample->setFinetune(8 * ((2 * ((sampleinfo[i].finetune & 0xF) ^ 0x8)) - 16));

		if(sampleinfo[i].repeat_length > 1)
       	{
            sample->setLoop(FORWARD_LOOP);
    		sample->setLoopStartAndLength(sampleinfo[i].repeat_offset << 1, sampleinfo[i].repeat_length << 1);
    	}
		inst->addSample(sample);
	}

	// ......................

	ntxm_dprintf("MOD Loaded.\n");

	//
	// Finish up
	//

	fclose(modfile);

	*_song = song;

	return FormatTransportError::SUCCESS;
}

// Saves a song to a file
FormatTransportError ModTransport::save(const char *filename, Song *song)
{
	return FormatTransportError::INIT_FAIL;
}
