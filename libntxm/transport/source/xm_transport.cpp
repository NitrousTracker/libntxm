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

#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/stat.h>
#include <algorithm>
#include <iterator>
#include <stdio.h>

#include "ntxm/xm_transport.h"
#include "ntxm/ntxmtools.h"

const char *xmtransporterrors[] =
	{"fat init failed",
	"could not open file",
	"not a valid xm file",
	"memory full",
	"pattern read error",
	"file too big for ram",
	"",
	"pattern too long",
	"file is zero byte",
	"disk is full",
	"xm format hacks not supported",
	"too many channels"};

/* ===================== PUBLIC ===================== */

// Loads a song from a file and puts it in the song argument
// returns 0 on success, an error code else
u16 XMTransport::load(const char *filename, Song **_song)
{
	u32 filesize = ntxm_getFileSize(filename);
	if(filesize == 0)
	{
		ntxm_dprintf("0-byte file!\n");
		return XM_TRANSPORT_FILE_ZERO_BYTE;
	}

	FILE *xmfile = fopen(filename, "rb");
	if((s32)xmfile == -1)
		return XM_TRANSPORT_ERROR_FOPENFAIL;


	// check if the module uses mpt hacks
	char footer[256] = { 0 };

	if (filesize > 255)
	{
		fseek(xmfile, -256, SEEK_END);

		fread(footer, sizeof(char), 255, xmfile);

		const char searchstr[4] = { 0x53, 0x54, 0x50, 0x4D }; // "STPM"
		char* it = std::search(
			std::begin(footer), std::end(footer), std::begin(searchstr),
			std::end(searchstr)); // offset of stuff in the footer varies, so
		// unfortunately we need to search

		if (it != std::end(footer))
		{
			fclose(xmfile);
			return XM_TRANSPORT_MPT_HACKS_UNSUPPORTED;
		}
	}
	
	fseek(xmfile, 0, SEEK_SET);
	setvbuf(xmfile, NULL, _IOFBF, 4096);
	//
	// Read header
	//

	// Magic number
	char magicnumber[18] = {0};
	fread(magicnumber, 1, 17, xmfile);

	if( strcmp(magicnumber, "Extended Module: ") != 0 ) {
		ntxm_dprintf("Not an XM file!\n");
		fclose(xmfile);
		return XM_TRANSPORT_ERROR_MAGICNUMBERINVALID;
	}

	// Song name
	char songname[21] = {0};
	fread(songname, 1, 20, xmfile);

	// Skip uninteresting stuff like tracker name
	fseek(xmfile, 21, SEEK_CUR);

	u16 header_version;
	fread(&header_version, 2, 1, xmfile);
	ntxm_dprintf("XM version %x\n", header_version);

	// Header size
	u32 header_size;
	fread(&header_size, 4, 1, xmfile);

	// Song length (pot size)
	u16 pot_size;
	fread(&pot_size, 2, 1, xmfile);
	//ntxm_dprintf("songlen: %u\n",pot_size);

	// Restart position
	u16 restart_pos;
	fread(&restart_pos, 2, 1, xmfile);
	//ntxm_dprintf("restart: %u\n", restart_pos);

	// Number of channels
	u16 n_channels;
	fread(&n_channels, 2, 1, xmfile);
	//ntxm_dprintf("n chn: %u\n", n_channels);

	if(n_channels > MAX_CHANNELS) {
		ntxm_dprintf("I only support XMs with 16 or fewer channels!\n");
		return XM_TRANSPORT_TOO_MANY_CHANNELS;
	}

	// Number of patterns
	u16 n_patterns;
	fread(&n_patterns, 2, 1, xmfile);
	//ntxm_dprintf("n ptn: %u\n", n_patterns);

	// Number of instruments
	u16 n_inst;
	fread(&n_inst, 2, 1, xmfile);
	ntxm_dprintf("n inst: %u\n", n_inst);

	// Flags, currently only used for the frequency table (0: amiga, 1: linear)
	// TODO: Amiga freq table
	u16 flags;
	fread(&flags, 2, 1, xmfile);
	//ntxm_dprintf("flags: %u\n", flags);

	// Tempo
	u16 tempo;
	fread(&tempo, 2, 1, xmfile);
	if(tempo == 0) tempo = 1; // Found an XM that actually had 0 there
	ntxm_dprintf("tempo: %u\n", tempo);


	// BPM
	u16 bpm;
	fread(&bpm, 2, 1, xmfile);
	//ntxm_dprintf("bpm: %u\n", bpm);
	ntxm_dprintf("new song %u %u %u\n",tempo, bpm, n_channels );
	// Construct the song with the current info
	Song *song = new Song(tempo, bpm, n_channels);
	if(song==NULL)
	{
		fclose(xmfile);
		ntxm_dprintf("memfull on line %d\n", __LINE__);
		delete song;
		return XM_TRANSPORT_ERROR_MEMFULL;
	}

	song->setName(songname);

	song->setRestartPosition(restart_pos);

	// Pattern order table
	u8 i, potentry;

	fread(&potentry, 1, 1, xmfile);
	song->setPotEntry(0, potentry); // The first entry is made automatically by the song

	for(i=1;i<pot_size;++i) {
		fread(&potentry, 1, 1, xmfile);
		song->potAdd(potentry);
	}
	fseek(xmfile, 256-pot_size, SEEK_CUR);

	//
	// Read patterns
	//

	u8 pattern;
	for(pattern=0;pattern<n_patterns;++pattern)
	{
		//ntxm_dprintf("Reading pattern %u\n",pattern);

		// Pattern header length
		u32 pattern_header_length;
		fread(&pattern_header_length, 4, 1, xmfile);

		//ntxm_dprintf("ptn header: %u\n",pattern_header_length);

		// Skip packing type (is always 0)
		fseek(xmfile, 1, SEEK_CUR);

		// Number of rows
		u16 n_rows;
		if( (header_version == 0x104) || (header_version == 0x103) ) {
			fread(&n_rows, 2, 1, xmfile);
		} else {
			u8 u8_n_rows;
			fread(&u8_n_rows, 1, 1, xmfile);
			n_rows = (u16)u8_n_rows + 1;
		}

		if(n_rows > MAX_PATTERN_LENGTH)
		{
			ntxm_dprintf("Pattern too long: %u rows\n", n_rows);
			fclose(xmfile);
			delete song;
			return XM_TRANSPORT_PATTERN_TOO_LONG;
		}

		//ntxm_dprintf("n_rows: %u\n",n_rows);

		// Packed patterndata size
		u16 patterndata_size;
		fread(&patterndata_size, 2, 1, xmfile);
		//TODO: Handle empty patterns (which are left out in the xm format)
		//ntxm_dprintf("patterndata_size: %u (%u/%u)\n", patterndata_size,pattern,n_patterns);

		if(patterndata_size > 0) { // Read the pattern

			u8 *ptn_data = (u8*)ntxm_umemalign(2, patterndata_size);
			if (ptn_data == NULL) {
				fclose(xmfile);
				ntxm_dprintf("memfull on line %d\n", __LINE__);
				delete song;
				return XM_TRANSPORT_ERROR_MEMFULL;
			}

			u32 bytes_read;

			bytes_read = fread(ptn_data, 1, patterndata_size, xmfile);

			if(bytes_read != patterndata_size) {
				ntxm_free(ptn_data);
				fclose(xmfile);
				ntxm_dprintf("pattern read error.\nread:%lu (should be %u)\n", bytes_read, patterndata_size);
				delete song;
				return XM_TRANSPORT_ERROR_PATTERN_READ;
			}

			u32 ptn_data_offset = 0;

			u8 chn;
			u16 row;

			if(pattern>0) {
				song->addPattern();
			}

			song->resizePattern(pattern, n_rows);

			Cell **ptn = song->getPattern(pattern);

			for(row=0;row<n_rows;++row)
			{
				for(chn=0;chn<n_channels;++chn)
				{
					u8 magicbyte = 0, note = EMPTY_NOTE, inst = NO_INSTRUMENT, vol = NO_VOLUME,
						eff_type = NO_EFFECT, eff_param = NO_EFFECT_PARAM, eff2_type = NO_EFFECT,
						eff2_param = NO_EFFECT_PARAM;

					magicbyte = ptn_data[ptn_data_offset];
					ptn_data_offset++;
					//fread(&magicbyte, 1, 1, xmfile);

					bool read_note=true, read_inst=true, read_vol=true,
						read_eff_type=true, read_eff_param=true;

					if(magicbyte & 1<<7) { // It's the magic byte!

						read_note = magicbyte & 1<<0;
						read_inst = magicbyte & 1<<1;
						read_vol = magicbyte & 1<<2;
						read_eff_type = magicbyte & 1<<3;
						read_eff_param = magicbyte & 1<<4;

					} else { // It's the note!

						note = magicbyte;
						read_note = false;

					}

					if(read_note) {
						note = ptn_data[ptn_data_offset];
						ptn_data_offset++;
					}

					if(read_inst) {
						inst = ptn_data[ptn_data_offset];
						ptn_data_offset++;
					} else {
						inst = NO_INSTRUMENT;
					}

					if(read_vol) {
						vol = ptn_data[ptn_data_offset];
						ptn_data_offset++;
					} else {
						vol = 0; // 'Do nothing'
					}

					if(read_eff_type) {
						eff_type = ptn_data[ptn_data_offset];
						ptn_data_offset++;
					} else {
						if(!read_eff_param)
							eff_type = NO_EFFECT;
						else
							eff_type = EFFECT_ARPEGGIO; // If we have params, but no effect, assume arpeggio
					}

					if(read_eff_param) {
						eff_param = ptn_data[ptn_data_offset];
						ptn_data_offset++;
					} else {
						eff_param = NO_EFFECT_PARAM;
					}

					//ntxm_dprintf("note: %u\ninst: %u\nvol: %u\neff_type: %u\neff_param: %u\n",note,inst,vol,eff_type,eff_param);

					// Insert note into song
					if(note > 0 && note < 97) {
						ptn[chn][row].note = note - 1;
					} else if(note==97) {
						ptn[chn][row].note = STOP_NOTE;
					} else {
						ptn[chn][row].note = EMPTY_NOTE;
					}

					if(inst != NO_INSTRUMENT) {
						ptn[chn][row].instrument = inst-1; // XM Inst indices start with 1
					} else {
						ptn[chn][row].instrument = NO_INSTRUMENT;
					}

					// Separate volume column effects from the volume column
					// and put them into the effcts column instead

					if((vol >= 0x10) && (vol <= 0x50))
					{
						u16 volume = (vol-16)*2;
						if(volume>=MAX_VOLUME) volume = MAX_VOLUME;
						ptn[chn][row].volume = volume;
					}
					else if(vol==0)
					{
						ptn[chn][row].volume = NO_VOLUME;
					}
					else if(vol>=0x60)
					{
						// It's an effect!
						u8 volfx_param = vol & 0x0F;

						if( (vol>=0x60)&&(vol<=0x6F) ) { // Volume slide down
							eff2_type = 0x0A;
							eff2_param = volfx_param;
						} else if( (vol>=0x70)&&(vol<=0x7F) ) { // Volume slide up
							eff2_type = 0x0A;
							eff2_param = volfx_param << 4;
						} else if( (vol>=0x80)&&(vol<=0x8F) ) { // Fine volume slide down
							eff2_type = 0x0E;
							eff2_param = 0xB0 | volfx_param;
						} else if( (vol>=0x90)&&(vol<=0x9F) ) { // Fine volume slide up
							eff2_type = 0x0E;
							eff2_param = 0xA0 | volfx_param;
						} else if( (vol>=0xA0)&&(vol<=0xAF) ) { // Set vibrato speed (calls vibrato)
							eff2_type = 0x04;
							eff2_param = volfx_param << 4;
						} else if( (vol>=0xB0)&&(vol<=0xBF) ) { // Vibrato
							eff2_type = 0x04;
							eff2_param = volfx_param; // Vibrato depth
						} else if( (vol>=0xC0)&&(vol<=0xCF) ) { // Set panning
							eff2_type = 0x08;
							eff2_param = volfx_param << 4;
						} else if( (vol>=0xD0)&&(vol<=0xDF) ) { // Panning slide left
							eff2_type = 0x19;
							eff2_param = volfx_param;
						} else if( (vol>=0xD0)&&(vol<=0xDF) ) { // Panning slide right
							eff2_type = 0x19;
							eff2_param = volfx_param << 4;
						} else if( vol>=0xF0 ) { // Tone porta
							eff2_type = 0x03;
							eff2_param = volfx_param << 4;
						}
					}

					ptn[chn][row].effect = eff_type;
					ptn[chn][row].effect_param = eff_param;
					ptn[chn][row].effect2 = eff2_type;
					ptn[chn][row].effect2_param = eff2_param;
				}

			}

			ntxm_free(ptn_data);

		} else { // Make an empty pattern

			if(pattern > 0) {
				song->addPattern();
			}

			song->resizePattern(pattern, n_rows);
		}

	}

	//
	// Read instruments
	//

	for(u8 inst=0; inst<n_inst; ++inst)
	{
		struct InstInfo *instinfo = (struct InstInfo*)ntxm_ucalloc(1, sizeof(struct InstInfo));
		if (instinfo == NULL) {
			fclose(xmfile);
			ntxm_dprintf("memfull on line %d\n", __LINE__);
			delete song;
			return XM_TRANSPORT_ERROR_MEMFULL;
		}

		// Read fields up to number of samples

		fread(&instinfo->inst_size, 1, 4, xmfile);
		fread(&instinfo->name, 1, 22, xmfile);
		fread(&instinfo->inst_type, 1, 1, xmfile);
		fread(&instinfo->n_samples, 1, 2, xmfile);

		Instrument *instrument = new Instrument(instinfo->name);
		if(instrument == 0)
		{
			ntxm_free(instinfo);
			fclose(xmfile);
			ntxm_dprintf("memfull on line %d\n", __LINE__);
			delete song;
			return XM_TRANSPORT_ERROR_MEMFULL;
		}
		song->setInstrument(inst, instrument);

		if(instinfo->n_samples > 0)
		{
			// Read the rest of the instrument info
			fread( &instinfo->sample_header_size, 4, 1, xmfile);
			fread( &instinfo->note_samples, 96, 1, xmfile);
			fread( &instinfo->vol_points, 48, 1, xmfile);
			fread( &instinfo->pan_points, 48, 1, xmfile);
			fread( &instinfo->n_vol_points, 1, 1, xmfile);
			fread( &instinfo->n_pan_points, 1, 1, xmfile);
			fread( &instinfo->vol_sustain_point, 1, 1, xmfile);
			fread( &instinfo->vol_loop_start_point, 1, 1, xmfile);
			fread( &instinfo->vol_loop_end_point, 1, 1, xmfile);
			fread( &instinfo->pan_sustain_point, 1, 1, xmfile);
			fread( &instinfo->pan_loop_start_point, 1, 1, xmfile);
			fread( &instinfo->pan_loop_end_point, 1, 1, xmfile);
			fread( &instinfo->vol_type, 1, 1, xmfile);
			fread( &instinfo->pan_type, 1, 1, xmfile);
			fread( &instinfo->vibrato_type, 1, 1, xmfile);
			fread( &instinfo->vibrato_sweep, 1, 1, xmfile);
			fread( &instinfo->vibrato_depth, 1, 1, xmfile);
			fread( &instinfo->vibrato_rate, 1, 1, xmfile);
			fread( &instinfo->vol_fadeout, 2, 1, xmfile);
			fread( &instinfo->reserved_bytes, 11, 1, xmfile);

			bool vol_env_on, vol_env_sustain, vol_env_loop, pan_env_on, pan_env_sustain, pan_env_loop;
			
			vol_env_on      = instinfo->vol_type & BIT(0);
			vol_env_sustain = instinfo->vol_type & BIT(1);
			vol_env_loop    = instinfo->vol_type & BIT(2);
			pan_env_on      = instinfo->pan_type & BIT(0);
			pan_env_sustain = instinfo->pan_type & BIT(1);
			pan_env_loop    = instinfo->pan_type & BIT(2);

			instrument->setVolumeEnvelope(instinfo->vol_points, instinfo->n_vol_points, instinfo->vol_sustain_point, 
					vol_env_on, vol_env_sustain, vol_env_loop);
			instrument->setPanningEnvelope(instinfo->pan_points, instinfo->n_pan_points, instinfo->pan_sustain_point, 
					pan_env_on, pan_env_sustain, pan_env_loop);

			// Skip the rest of the header if is longer than the current position
			// This was really strange and took some time (and debugging with Tim)
			// to figure out. Why the fsck is the instrument header that much longer?
			// Well, don't care, skip it.

			//if(instinfo->inst_size > 252) // <- apparently wrong! samples can even be nested, so we have to seek back here o_O
			fseek(xmfile, instinfo->inst_size-252, SEEK_CUR);

			for(u8 i=0; i<96; ++i)
				instrument->setNoteSample(i,instinfo->note_samples[i]);

			// Load the sample(s)

			// Headers
			u8 *sample_headers = (u8*)ntxm_umemalign(2, instinfo->n_samples*40);
			if (sample_headers == NULL) {
				ntxm_free(instinfo);
				fclose(xmfile);
				ntxm_dprintf("memfull on line %d\n", __LINE__);
				delete song;
				return XM_TRANSPORT_ERROR_MEMFULL;
			}
			fread(sample_headers, 40, instinfo->n_samples, xmfile);

			for(u8 sample_id=0; sample_id < instinfo->n_samples; sample_id++)
			{
				// Sample length
				u32 sample_length;
				sample_length = *(u32*)(sample_headers+40*sample_id + 0);
				ntxm_dprintf("sample length: %lu\n", sample_length);

				// Sample loop start
				u32 sample_loop_start;
				sample_loop_start = *(u32*)(sample_headers+40*sample_id + 4);
				ntxm_dprintf("sample loop start: %lu\n", sample_loop_start);

				// Sample loop length
				u32 sample_loop_length;
				sample_loop_length = *(u32*)(sample_headers+40*sample_id + 8);
				ntxm_dprintf("sample loop length: %lu\n", sample_loop_length);

				// Volume (0-64)
				u8 sample_volume;
				sample_volume = *(u8*)(sample_headers+40*sample_id + 12);
				//ntxm_dprintf("sample volume: %u\n",sample_volume);

				if(sample_volume == 64) { // Convert scale to 0-255
					sample_volume = 255;
				} else {
					sample_volume *= 4;
				}

				// Finetune
				s8 sample_finetune;
				sample_finetune = *(s8*)(sample_headers+40*sample_id + 13);
				//ntxm_dprintf("sample finetune: %d\n",sample_finetune);

				// Type byte (loop type and wether it's 8 or 16 bit)
				u8 sample_type;
				sample_type = *(u8*)(sample_headers+40*sample_id + 14);

				u8 loop_type = sample_type & 3;
				if(sample_loop_length == 0)
					loop_type = NO_LOOP;

				bool sample_is_16_bit = sample_type & 0x10;
				if(sample_is_16_bit)
					ntxm_dprintf("16 bit\n");
				else
					ntxm_dprintf("8 bit\n");

				// Panning
				u8 sample_panning;
				sample_panning = *(u8*)(sample_headers+40*sample_id + 15);
				//ntxm_dprintf("panning: %u\n", sample_panning);

				// Relative note
				s8 sample_rel_note;
				sample_rel_note = *(s8*)(sample_headers+40*sample_id + 16);
				//ntxm_dprintf("rel note: %d\n", sample_rel_note);

				// Sample name
				char sample_name[22 + 1];
				memset(sample_name, 0, sizeof(sample_name));
				memcpy(sample_name, sample_headers+40*sample_id + 18, 22);

				// Cut off trailing spaces
				int i = sizeof(sample_name) - 2;
				while(i >= 0 && sample_name[i] == ' ')
					--i;
				++i;
				sample_name[i] = '\0';

				//ntxm_dprintf("sample name: '%s' (%u)\n", sample_name, strlen(sample_name));

				// Sample data
				ntxm_dprintf("loading data\n");
				void *sample_data = 0;
				if(sample_length > 0)
				{
					sample_data = ntxm_umemalign(2, sample_length);

					if(sample_data==NULL)
					{
						ntxm_free(sample_headers);
						ntxm_free(instinfo);
						fclose(xmfile);
						ntxm_dprintf("memfull on line %d\n", __LINE__);
						delete song;
						return XM_TRANSPORT_ERROR_MEMFULL;
					}

					fread(sample_data, sample_length, 1, xmfile);
				}

				// Delta-decode
				ntxm_dprintf("delta decode\n");
				if(sample_is_16_bit) {
					s16 last = 0;
					s16 *smp = (s16*)sample_data;
					for(u32 i=0;i<sample_length/2;++i) {
						smp[i] += last;
						last = smp[i];
					}

				} else {
					s8 last = 0;
					s8 *smp = (s8*)sample_data;
					for(u32 i=0;i<sample_length;++i) {
						smp[i] += last;
						last = smp[i];
					}
				}

				// Insert sample into the instrument
				u32 n_samples;
				if(sample_is_16_bit) {
					n_samples = sample_length/2;
					sample_loop_start /= 2;
					sample_loop_length /= 2;
				} else {
					n_samples = sample_length;
				}
				Sample *sample = new Sample(sample_data, n_samples, 8363, sample_is_16_bit);
				if(sample==NULL)
				{
					ntxm_free(sample_data);
					ntxm_free(sample_headers);
					ntxm_free(instinfo);
					fclose(xmfile);
					ntxm_dprintf("memfull on line %d\n", __LINE__);
					delete song;
					return XM_TRANSPORT_ERROR_MEMFULL;
				}

				sample->setVolume(sample_volume);
				sample->setRelNote(sample_rel_note);
				sample->setFinetune(sample_finetune);
				sample->setPanning(sample_panning);

				sample->setLoop(loop_type);
				sample->setLoopStartAndLength(sample_loop_start, sample_loop_length);
				sample->setName(sample_name);
				instrument->addSample(sample);

				ntxm_dprintf("Sample loaded\n");
			}

			ntxm_free(sample_headers);

			//ntxm_dprintf("inst loaded\n");

		}
		else
		{
			// If the instrument has no samples, skip the rest of the instrument header
			// (which should contain rubbish anyway)
			fseek(xmfile, instinfo->inst_size-29, SEEK_CUR);
		}

		ntxm_free(instinfo);
	}

	ntxm_dprintf("XM Loaded.\n");

	//
	// Finish up
	//
	fclose(xmfile);

	*_song = song;

	return 0;
}



// Saves a song to a file
u16 XMTransport::save(const char *filename, Song *song)
{
	//
	// Init
	//

	// TODO: Let the Arm7 update the RTC first
	// TODO: Check if there's enough space on the card

	FILE *xmfile = fopen(filename, "wb");

	if((s32)xmfile == -1) {
		return XM_TRANSPORT_ERROR_FOPENFAIL;
	}

	//
	// Write header
	//

	// Magic number
	char magicnumber[18] = "Extended Module: ";
	fwrite(magicnumber, 1, 17, xmfile);

	// Song name
	char songname[21] = {0};
	strcpy(songname, song->getName());
	fwrite(songname, 1, 20, xmfile);

	// wtf
	u8 versionbyte = 0x1a;
	fwrite(&versionbyte, 1, 1, xmfile);

	// Tracker Name
	char trackername[21] = "NitroTracker";
	fwrite(trackername, 1, 20, xmfile);

	// Version number
	u16 version = 0x104; // FT2 version number (else soundtracker won't accept the file
	fwrite(&version, 2, 1, xmfile);

	// Header size
	u32 headersize = 0x114;
	fwrite(&headersize, 4, 1, xmfile);

	// Song Length
	u16 songlength = song->getPotLength();
	fwrite(&songlength, 2, 1, xmfile);

	// Restart position
	u16 restartpos = song->getRestartPosition();
	fwrite(&restartpos, 2, 1, xmfile);

	// Number of channels
	u16 n_channels = song->getChannels();
	fwrite(&n_channels, 2, 1, xmfile);

	// Number of patterns
	u16 n_patterns = song->getNumPatterns();
	fwrite(&n_patterns, 2, 1, xmfile);

	// Number of instruments
	u16 n_inst = song->getInstruments();
	fwrite(&n_inst, 2, 1, xmfile);

	// Flags
	u16 flags = 1; // Means linear freq table (amiga table support maybe soon)
	fwrite(&flags, 2, 1, xmfile);

	// Tempo
	u16 tempo = song->getTempo();
	fwrite(&tempo, 2, 1, xmfile);

	// BPM
	u16 bpm = song->getBPM();
	fwrite(&bpm, 2, 1, xmfile);

	// POT
	u8 pot[256] = {0};
	for(int i=0; i<song->getPotLength(); ++i) {
		pot[i] = song->getPotEntry(i);
	}
	fwrite(pot, 1, 256, xmfile);

	//
	// Write patterns
	//

	// TODO: Skip empty pattern

	for(u8 ptn = 0; ptn < n_patterns; ++ptn) {

		ntxm_dprintf("ptn %u\n", ptn);

		// Pattern header length
		u32 ptnheaderlength = 9;
		fwrite(&ptnheaderlength, 4, 1, xmfile);

		// Packing type
		u8 packtype = 0; // Is always 0
		fwrite(&packtype, 1, 1, xmfile);

		// Number of rows
		u16 n_rows = song->getPatternLength(ptn);
		fwrite(&n_rows, 2, 1, xmfile);

		if(n_rows > 256) {
			ntxm_dprintf("%u rows!\n",n_rows);
			while(1);
		}

		// Pack the pattern in memory, then save it
		u8 *patterndata = (u8*)ntxm_umalloc(5*32*256);

		if(patterndata==0) {
			fclose(xmfile);
			return XM_TRANSPORT_ERROR_MEMFULL;
		}

		memset(patterndata, 0, 5*32*256);
		Cell **pattern = song->getPattern(ptn);

		u16 datapos = 0;

		Cell cell;
		for(u16 row=0; row<n_rows; ++row) {
			for(u16 chn=0; chn<n_channels; ++chn) {

				//ntxm_dprintf("ptn:%u row:%u chn:%u datapos:%u\n",ptn,row,chn,datapos);

				cell = pattern[chn][row];

				u8 write_note = cell.note != EMPTY_NOTE;
				u8 write_instrument = cell.instrument != NO_INSTRUMENT;
				u8 write_volume = (cell.volume != NO_VOLUME) || (cell.effect2 != NO_EFFECT);
				u8 write_effect = cell.effect != NO_EFFECT;
				u8 write_effect_param = cell.effect_param != NO_EFFECT_PARAM;

				u8 magicbyte = 0;
				magicbyte |= write_note         << 0;
				magicbyte |= write_instrument   << 1;
				magicbyte |= write_volume       << 2;
				magicbyte |= write_effect       << 3;
				magicbyte |= write_effect_param << 4;

				// Check if everything in the cell is set. If not, use the magic byte
				if(magicbyte != 31) {
					magicbyte |= 1 << 7;
					patterndata[datapos] = magicbyte;
					datapos++;
				}

				if(write_note) {
					if(cell.note == EMPTY_NOTE) {
						patterndata[datapos] = 0;
					} else if(cell.note == STOP_NOTE) {
						patterndata[datapos] = 97;
					} else {
						patterndata[datapos] = cell.note+1;
					}

					datapos++;
				}
				if(write_instrument) {
					patterndata[datapos] = cell.instrument+1;
					datapos++;
				}
				if(write_volume) {
					// Volume or volume effect?
					if(cell.volume == NO_VOLUME) // Volume is not set, so it's a volume effect
					{
						// Convert "real" effect to volume effect
						u8 eff2_type = cell.effect2;
						u8 eff2_param = cell.effect2_param;

						u8 volbyte = 0;

						switch(eff2_type) {
							case(0x0A): { // Volume slide
								if(eff2_param > 0x0F) { // Up
									volbyte = 0x70 | (eff2_param >> 4);
								} else { // Down
									volbyte = 0x60 | (eff2_param & 0x0F);
								}
								break;
							}
							case(0x0E): { // Fine volume slide
								if((eff2_param & 0xF0) == 0xA0) { // Up
									volbyte = 0x90 | (eff2_param & 0x0F);
								} else if((eff2_param & 0xF0) == 0xB0) { // Down
									volbyte = 0x80 | (eff2_param & 0x0F);
								}
								break;
							}
							case(0x04): { // Vibrato
								if(eff2_param > 0x0F) { // Speed
									volbyte = 0xA0 | (eff2_param >> 4);
								} else { // Depth
									volbyte = 0xB0 | (eff2_param & 0x0F);
								}
								break;
							}
							case(0x08): { // Set panning
								volbyte = 0xC0 | (eff2_param >> 4);
								break;
							}
							case(0x19): { // Panning slide
								if(eff2_param > 0x0F) { // Right
									volbyte = 0xE0 | (eff2_param >> 4);
								} else { // Left
									volbyte = 0xD0 | (eff2_param & 0x0F);
								}
								break;
							}
							case(0x03): { // Tone porta
								volbyte = 0xF0 | (eff2_param >> 4);
								break;
							}
						}

						patterndata[datapos] = volbyte;

					} else {
						patterndata[datapos] = (cell.volume+1)/2+16;
					}
					datapos++;
				}
				if(write_effect) {
					patterndata[datapos] = cell.effect;
					datapos++;
				}
				if(write_effect_param) {
					patterndata[datapos] = cell.effect_param;
					datapos++;
				}

			}
		}

		// Packed patterndata size
		u16 packed_ptn_size = datapos+1;
		//ntxm_dprintf("saving packed size\n");
		fwrite(&packed_ptn_size, 2, 1, xmfile);
		// Packed patterndata
		ntxm_dprintf("saving ptn (%u bytes) %p\n",packed_ptn_size,patterndata);
		fwrite(patterndata, 1, packed_ptn_size, xmfile);

		ntxm_free(patterndata);
	}

	ntxm_dprintf("patterns ready\n");

	//
	// Write instruments
	//

	Instrument *instrument = 0;

	for(u8 inst=0; inst<song->getInstruments(); ++inst) {

		ntxm_dprintf("saving inst %u\n", inst);

		instrument = song->getInstrument(inst);

		// We also have to save empty instruments
		bool empty_inst = false;
		if(instrument==NULL) {
			instrument = new Instrument("");
			if (instrument == NULL) {
				fclose(xmfile);
				ntxm_dprintf("memfull on line %d\n", __LINE__);
				return XM_TRANSPORT_ERROR_MEMFULL;
			}
			empty_inst = true;
		}

		// Instrument size (always 0x107)
		u32 inst_size = 0x107;
		fwrite(&inst_size, 4, 1, xmfile);

		// Instrument name
		char inst_name[33] = {0};
		strcpy(inst_name, instrument->getName());
		fwrite(inst_name, 1, 22, xmfile);

		// Instrument type (always 0)
		u8 inst_type = 0;
		fwrite(&inst_type, 1, 1, xmfile);

		// Number of samples in instrument
		u16 inst_n_samples = instrument->getSamples();
		fwrite(&inst_n_samples, 2, 1, xmfile);

		ntxm_dprintf("n samples: %u\n", inst_n_samples);

		if(inst_n_samples > 0) {

			// Second part of inst header:

			struct InstInfo *instinfo = (InstInfo*)ntxm_ucalloc(1, sizeof(InstInfo));
			if (instinfo == NULL) {
				if (empty_inst) delete instrument;
				fclose(xmfile);
				ntxm_dprintf("memfull on line %d\n", __LINE__);
				return XM_TRANSPORT_ERROR_MEMFULL;
			}

			// Sample header size (always 0x28)
			instinfo->sample_header_size = 0x28;

			// Sample number for all notes
			for(u8 i=0;i<96;++i)
				instinfo->note_samples[i] = instrument->getNoteSample(i);

			// Volume envelope points
			for(u8 i=0; i<12; ++i)
			{
				instinfo->vol_points[2*i] = instrument->vol_envelope_x[i];
				instinfo->vol_points[2*i+1] = instrument->vol_envelope_y[i];
			}

			instinfo->n_vol_points = instrument->n_vol_points;

			// Panning envelope points
			for(u8 i=0; i<12; ++i)
			{
				instinfo->pan_points[2*i] = instrument->pan_envelope_x[i];
				instinfo->pan_points[2*i+1] = instrument->pan_envelope_y[i];
			}

			instinfo->n_pan_points = instrument->n_pan_points;

			// Vol env sustain, start, end (not used for now)
			instinfo->vol_sustain_point = instrument->getVolumeEnvelopeSustainPoint();
			instinfo->vol_loop_start_point = 0;
			instinfo->vol_loop_end_point = 0;

			// Volume envelope type
			instinfo->vol_type = 0;
			if(instrument->vol_env_on)
				instinfo->vol_type |= BIT(0);
			if(instrument->vol_env_sustain)
				instinfo->vol_type |= BIT(1);
			if(instrument->vol_env_loop)
				instinfo->vol_type |= BIT(2);

			// Panning envelope type
			instinfo->pan_type = 0;
			if(instrument->pan_env_on)
				instinfo->pan_type |= BIT(0);
			if(instrument->pan_env_sustain)
				instinfo->pan_type |= BIT(1);
			if(instrument->pan_env_loop)
				instinfo->pan_type |= BIT(2);

			// Vibrato stuff and fadeout are skipped for now

			fwrite( &instinfo->sample_header_size, 4, 1, xmfile);
			fwrite( &instinfo->note_samples, 96, 1, xmfile);
			fwrite( &instinfo->vol_points, 48, 1, xmfile);
			fwrite( &instinfo->pan_points, 48, 1, xmfile);
			fwrite( &instinfo->n_vol_points, 1, 1, xmfile);
			fwrite( &instinfo->n_pan_points, 1, 1, xmfile);
			fwrite( &instinfo->vol_sustain_point, 1, 1, xmfile);
			fwrite( &instinfo->vol_loop_start_point, 1, 1, xmfile);
			fwrite( &instinfo->vol_loop_end_point, 1, 1, xmfile);
			fwrite( &instinfo->pan_sustain_point, 1, 1, xmfile);
			fwrite( &instinfo->pan_loop_start_point, 1, 1, xmfile);
			fwrite( &instinfo->pan_loop_end_point, 1, 1, xmfile);
			fwrite( &instinfo->vol_type, 1, 1, xmfile);
			fwrite( &instinfo->pan_type, 1, 1, xmfile);
			fwrite( &instinfo->vibrato_type, 1, 1, xmfile);
			fwrite( &instinfo->vibrato_sweep, 1, 1, xmfile);
			fwrite( &instinfo->vibrato_depth, 1, 1, xmfile);
			fwrite( &instinfo->vibrato_rate, 1, 1, xmfile);
			fwrite( &instinfo->vol_fadeout, 2, 1, xmfile);
			fwrite( &instinfo->reserved_bytes, 11, 1, xmfile);

			ntxm_free(instinfo);

			Sample *sample = 0;

			// Fill up to header size = 0x107 = 263. We have written 252 bytes up will now.
			u8* moreemptydata = (u8*)ntxm_cmalloc(inst_size - 252);
			memset(moreemptydata, 0, inst_size - 252);
			fwrite(moreemptydata, 1, inst_size - 252, xmfile);
			ntxm_free(moreemptydata);

			// Write sample headers

			for(u8 smp=0; smp<inst_n_samples; ++smp)
			{
				sample = instrument->getSample(smp);

				bool empty_sample = false;
				if(sample == NULL)
				{
					sample = new Sample(NULL, 0);
					if (sample == NULL) {
						if (empty_inst) delete instrument;
						fclose(xmfile);
						ntxm_dprintf("memfull on line %d\n", __LINE__);
						return XM_TRANSPORT_ERROR_MEMFULL;
					}
					empty_sample = true;
				}

				// Sample length
				u32 smp_length = sample->getSize();
				fwrite(&smp_length, 4, 1, xmfile);

				// Loop stuff
				u32 smp_loop_start = sample->getLoopStart();
				u32 smp_loop_length = sample->getLoopLength();

				if(sample->is16bit())
				{
					smp_loop_start *= 2;
					smp_loop_length *= 2;
				}

				fwrite(&smp_loop_start, 4, 1, xmfile);
				fwrite(&smp_loop_length, 4, 1, xmfile);

				// Sample Volume
				u8 smp_vol = (sample->getVolume() + 1) / 4; // Convert scale to 0-64
				fwrite(&smp_vol, 1, 1, xmfile);

				// Finetune
				s8 smp_finetune = sample->getFinetune();
				fwrite(&smp_finetune, 1, 1, xmfile);

				// Type
				u8 smp_type = 0;
				smp_type |= sample->getLoop();
				if(sample->is16bit()) {
					smp_type |= 1<<4;
				}
				fwrite(&smp_type, 1, 1, xmfile);

				// Panning
				u8 smp_panning = sample->getBasePanning();
				fwrite(&smp_panning, 1, 1, xmfile);

				// Relative note
				s8 smp_relnote = sample->getRelNote();
				fwrite(&smp_relnote, 1, 1, xmfile);

				// Reserved byte (what a crappy standard)
				u8 smp_funky_reserved_byte = 0x80;
				fwrite(&smp_funky_reserved_byte, 1, 1, xmfile);

				// Sample name
				char sample_name[23] = "                      ";
				strncpy(sample_name, sample->getName(), strlen(sample->getName())); // Don't copy \0 character
				fwrite(sample_name, 1, 22, xmfile);

				if(empty_sample == true)
					ntxm_free(sample);
			}

			// Write sample data

			for(u8 smp=0; smp<inst_n_samples; ++smp)
			{
				sample = instrument->getSample(smp);

				bool empty_sample = false;
				if(sample == NULL)
				{
					sample = new Sample(NULL, 0);
					if (sample == NULL) {
						if (empty_inst) delete instrument;
						fclose(xmfile);
						ntxm_dprintf("memfull on line %d\n", __LINE__);
						return XM_TRANSPORT_ERROR_MEMFULL;
					}
					empty_sample = true;
				}

				if(sample->is16bit())
				{
					s16 *sample_data = (s16*)sample->getData();
					s16 *sample_data_enc = (s16*)ntxm_umalloc( sample->getSize() );

					if(sample_data_enc != 0)
					{
						s16 last = 0;
						for(u32 i=0; i<sample->getNSamples(); ++i)
						{
							sample_data_enc[i] = sample_data[i] - last;
							last = sample_data[i];
						}
						fwrite(sample_data_enc, 1, sample->getSize(), xmfile);

						ntxm_free(sample_data_enc);

					} else { // slow uncached fallback if ram is nearly full

						ntxm_dprintf("saving with slow uncached fallback\n");
						s16 last = 0, curr;
						for(u32 i=0; i<sample->getNSamples(); ++i) {
							curr = sample_data[i] - last;
							fwrite(&curr, 1, 2, xmfile);
							last = sample_data[i];
						}
						ntxm_dprintf("done\n");
					}

				} else {

					s8 *sample_data = (s8*)sample->getData();
					s8 *sample_data_enc = (s8*)ntxm_umalloc( sample->getSize() );

					if(sample_data_enc != 0)
					{
						s8 last = 0;
						for(u32 i=0; i<sample->getNSamples(); ++i)
						{
							sample_data_enc[i] = sample_data[i] - last;
							last = sample_data[i];
						}

						fwrite(sample_data_enc, 1, sample->getSize(), xmfile);

						ntxm_free(sample_data_enc);
					}
					else
					{
						ntxm_dprintf("saving with slow uncached fallback\n");
						s8 last = 0, curr;
						for(u32 i=0; i<sample->getNSamples(); ++i)
						{
							curr = sample_data[i] - last;
							fwrite(&curr, 1, 2, xmfile);
							last = sample_data[i];
						}
						ntxm_dprintf("done\n");
					}

				}

				if(empty_sample == true)
					ntxm_free(sample);

			}
		}
		else
		{
			// fill up the instrument header with 0es
			u8 *zeroes = (u8*)ntxm_umemalign(2, inst_size - 29);
			if (zeroes != NULL) {
				memset(zeroes, 0, inst_size - 29);
				fwrite(zeroes, inst_size - 29, 1, xmfile);
				ntxm_free(zeroes);
			} else {
				ntxm_dprintf("saving with slow uncached fallback\n");
				for (u32 i=0; i<inst_size - 29; ++i)
					fputc(0, xmfile);
				ntxm_dprintf("done\n");
			}
		}

		if(empty_inst == true) {
			delete instrument;
		}

	}

	//
	// Finish up
	//
	fclose(xmfile);

	ntxm_dprintf("song saved as :\"%s\"\n", filename);

	return 0;
}

const char *XMTransport::getError(u16 error_id)
{
	return xmtransporterrors[error_id-1];
}

/* ===================== PRIVATE ===================== */
