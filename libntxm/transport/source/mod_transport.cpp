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

/*
 * This is the beginning of a mod importer.
 * 
#include "mod_transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *modtransporterrors[] =
	{"fat init failed",
	"could not open file",
	"not a valid xm file",
	"memory full",
	"pattern read error",
	"file too big for ram",
	"",
	"pattern too long",
	"file is zero byte"};

// ===================== PUBLIC ===================== 

// Loads a song from a file and puts it in the song argument
// returns 0 on success, an error code else
u16 ModTransport::load(const char *filename, Song **_song)
{
	//
	// Init
	//
	
	FILE *modfile = fopen(filename, "rb");
	
	if((s32)modfile == -1)
		return MOD_TRANSPORT_ERROR_FOPENFAIL;

	setvbuf(modfile, NULL, _IOFBF, 4096);
	
	// Check if the song fits into RAM
	struct stat fstats;
	stat(filename, &fstats);
	u32 filesize = fstats.st_size;
	
	if(filesize > MAX_MOD_FILESIZE)
	{
		fclose(modfile);
		ntxm_dprintf("file too big for ram\n");
		return MOD_TRANSPORT_FILE_TOO_BIG_FOR_RAM;
	}
	
	if(filesize == 0)
	{
		fclose(modfile);
		ntxm_dprintf("0-byte file!\n");
		return MOD_TRANSPORT_FILE_ZERO_BYTE;
	}
	
	Song *song = new Song();
	
	//
	// Read name
	//
	char song_name[20];
	calloc(song_name, 20, 1);
	fread(song_name, 1, 20, modfile);
	ntxm_dprintf("It's called %s\n", song_name);
	
	song->setName(song_name);
	
	//
	// Read Samples
	//
	
	Samplenfo *sample[31];
		
	for(u8 smp=0; smp<31; ++smp)
	{
		SampleInfo *sample[smp] = (*SampleInfo)calloc(sizeof(SampleInfo), 1);
		
		fread(&sample->name, 1, 22, modfile);
		fread(&sample->length, 2, 1, modfile);
		fread(&sample->finetune, 1, 1, modfile);
		fread(&sample->volume, 1, 1, modfile);
		fread(&sample->repeat_offset, 2, 1, modfile);
		fread(&sample->repeat_length, 2, 1, modfile);
		
		//Instrument *inst = new Instrument(sample->name);
		//Sample *smp = new Sample(void *_sound_data, u32 _n_samples, u16 _sampling_frequency=44100,
		//	bool _is_16_bit=true, u8 _loop=NO_LOOP, u8 _volume=255);
	}
	
	
	
	//
	// Read header
	//
	
	u8 potlen;
	fread(&potlen, 1, 1, modfile);
	
	u8 restartpos;
	fread(&restartpos, 1, 1, modfile);
	
	u8 pot[128];
	fread(pot, 1, 128, modfile);
	
	char fmt[4];
	fread(fmt, 1, 4, modfile);
	
	u8 n_patterns = 0;
	for(i=0; i<128; ++i)
		if(pot[i] > n_patterns)
			n_patterns = pot[i];
	
	// Parse the format tag
	u8 n_channels;
	
	if	( 	   ( strcmp(fmt, "M.K.") == 0 ) || ( strcmp(fmt, "FLT4") == 0 )
			|| ( strcmp(fmt, "M!K!") == 0 ) || ( strcmp(fmt, "4CHN") == 0 ) )
		
		n_channels = 4;
	
	else if ( strcmp(fmt, "6CHN") == 0 )
		
		n_channels = 6;
	
	else if ( ( strcmp(fmt, "8CHN") == 0) || ( strcmp(fmt, "OCTA") == 0 ) )
		
		n_channels = 8;
	
	else
		
		ntxm_dprintf("Unsupported format!\n");
	
	//
	// Read Patterns
	//
	u16 patterndata_size = 4 * n_channels * 64;
	
	for(u8 ptn=0; ptn<n_patterns; ++ptn)
	{
		u8 *ptn_data = (u8*)calloc(patterndata_size, 1);
		fread(ptn_data, patterndata_size, 1, modfile);
		
		for(u8 row=0; row<64; ++row)
		{
			for(u8 chn=0; chn<n_channels; ++chn)
			{
				u8 notedata[4];
				fread(notedata, 1, 4, modfile);
				
				u8 sample;
				u16 period, effect;
				sample = ( (notedata[0] >> 4) << 4 ) | ( notedata[2] >> 4 );
				period = ( ( notedata[0] & 0x0F ) << 8 ) | notedata[1];
				effect = ( ( notedata[2] & 0x0F ) << 8 ) | notedata[3];
				
				u16 frequency = 70937892 / period / 20; // PAL Amiga conversion
			}
		}
		
		ntxm_free(ptn_data);
	}
	
	// ......................
	
	
	for(u8 smp=0; smp<31; ++smp)
		ntxm_free(sample[smp]);
	
	ntxm_dprintf("MOD Loaded.\n");
	
	//
	// Finish up
	//
	
	fclose(modfile);
	
	*_song = song;
	
	return 0;
}

// Saves a song to a file
u16 ModTransport::save(const char *filename, Song *song)
{
	return 42;
}

const char *ModTransport::getError(u16 error_id)
{
	return modtransporterrors[error_id-1];
}
*/
