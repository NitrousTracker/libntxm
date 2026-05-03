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
#include <stdio.h>
#include <string.h>
#include <nds.h>

#ifdef ARM9
#include <fat.h>
#endif

#include "ntxm/wav.h"

#ifdef ARM9
#include "ntxm/ntxmtools.h"
#endif

/* ===================== PUBLIC ===================== */

Wav::Wav()
	:compression_(CMP_PCM), n_channels_(1), sampling_rate_(22050), bit_per_sample_(8),
	n_samples_(0), audio_data_(0), loop_type_(0), loop_start_(0), loop_end_(-1)
{

}

Wav::~Wav() {

}

#define FOURCC_RIFF 0x46464952
#define FOURCC_WAVE 0x45564157
#define FOURCC_JUNK 0x4b4e554a
#define FOURCC_fmt 0x20746D66
#define FOURCC_data 0x61746164
#define FOURCC_smpl 0x6C706D73

bool Wav::load(const char *filename)
{
#if defined(ARM9)
	// Init

	FILE *fileh;

	fileh = fopen(filename, "rb");

	if(!fileh)
		return false;

	u32 cc;

	// RIFF header
	fread(&cc, 4, 1, fileh);
	if(cc != FOURCC_RIFF) {
		fclose(fileh);
		return false;
	}

	u32 riff_size;
	fread(&riff_size, 4, 1, fileh);

	// WAVE header
	fread(&cc, 4, 1, fileh);
	if(cc != FOURCC_WAVE) {
		fclose(fileh);
		return false;
	}

	while(true) {
		if(fread(&cc, 4, 1, fileh) <= 0 || feof(fileh)) {
			fclose(fileh);
			return false;
		}
		// fmt chunk?
		if(cc == FOURCC_fmt) {
			break;
		}
		// JUNK chunk?
		if(cc == FOURCC_JUNK) {
			long junksize;
			fread(&junksize, 4, 1, fileh);
			fseek(fileh, junksize, SEEK_CUR);
			continue;
		}
		// unknown chunk
		fclose(fileh);
		return false;
	}

	u32 fmt_chunk_size;
	fread(&fmt_chunk_size, 4, 1, fileh);

	u16 compression_code;
	fread(&compression_code, 2, 1, fileh);

	if(compression_code == 1 || compression_code == 0xFFFE) {
		// 1 = PCM, 0xFFFE = extensible
		compression_ = CMP_PCM;
	} else {
		fclose(fileh);
		return false;
	}

	u16 n_channels; // They really thought they were cool when making this 16 bit.
	fread(&n_channels, 2, 1, fileh);

	if(n_channels > 2) {
		fclose(fileh);
		return false;
	} else {
		n_channels_ = n_channels;
	}

	u32 sampling_rate;
	fread(&sampling_rate, 4, 1, fileh);

	sampling_rate_ = sampling_rate;

	u32 avg_bytes_per_sec; // We don't need this
	fread(&avg_bytes_per_sec, 4, 1, fileh);

	u16 block_align; // We don't need this
	fread(&block_align, 2, 1, fileh);

	u16 bit_per_sample;
	fread(&bit_per_sample, 2, 1, fileh);
	if(!(bit_per_sample & 7) && bit_per_sample >= 8) {
		bit_per_sample_ = bit_per_sample > 16 ? 16 : bit_per_sample;
	} else {
		fclose(fileh);
		return false;
	}

	// Skip extra bytes
	fseek(fileh, fmt_chunk_size - 16, SEEK_CUR);

	audio_data_ = NULL;
	int sample_size = bit_per_sample/8;

	loop_type_ = 0;

	while(true) {
		if(feof(fileh))
			break;

		u32 chunk_size;
		if (fread(&cc, 4, 1, fileh) <= 0)
			break;
		if (fread(&chunk_size, 4, 1, fileh) <= 0)
			break;

		u32 chunk_pos = ftell(fileh);
		u32 loop_id, loop_type = 0, loop_start, loop_end;

		if(cc == FOURCC_data) {
			if(compression_ == CMP_PCM) {
				n_samples_ = chunk_size / sample_size;
			} else {
				n_samples_ = chunk_size * 2 * sample_size;
			}

			audio_data_ = (u8*)ntxm_umalloc(chunk_size);
			if(audio_data_ == 0) {
				ntxm_dprintf("Could not alloc mem(%ld) for wav.\n", chunk_size);
				fclose(fileh);
				return false;
			}

			// Read the data
			if(bit_per_sample > bit_per_sample_) {
				int byte_per_sample = bit_per_sample_/8;

				u8 *audio_data_tmp = (u8*)ntxm_umalloc(chunk_size);
				if (audio_data_tmp == 0) {
					ntxm_dprintf("Could not alloc mem(%ld) for wav.\n", chunk_size);
					ntxm_free(audio_data_);
					fclose(fileh);
					return false;
				}
				fread(audio_data_tmp, 1, chunk_size, fileh);

				u32 i=0;
				for(; i < n_samples_; i++) {
					memcpy(audio_data_ + i * byte_per_sample, audio_data_tmp + i * byte_per_sample + skip_bytes, 
						   (bit_per_sample_ / 8));
				}

				ntxm_free(audio_data_tmp);
			} else {
				size_t bytes_read = fread(audio_data_, 1, chunk_size, fileh);
				if (bytes_read < chunk_size) {
					memset(audio_data_ + bytes_read, 0, chunk_size - bytes_read);
				}
			}

			// Convert 8 bit samples from unsigned to signed
			if(bit_per_sample == 8) {
				ntxm_unsigned2signed_8(audio_data_, chunk_size);
			}

			// Read everything, so continue
			continue;
		} else if (cc == FOURCC_smpl) {
			u32 loop_count;

			fseek(fileh, 0x24 - 0x08, SEEK_CUR);
			fread(&loop_count, 4, 1, fileh);
			fseek(fileh, 0x2C - 0x28, SEEK_CUR);

			while(loop_count--) {
				fread(&loop_id, 4, 1, fileh);
				fread(&loop_type, 4, 1, fileh);
				fread(&loop_start, 4, 1, fileh);
				fread(&loop_end, 4, 1, fileh);
				fseek(fileh, 8, SEEK_CUR);

				if (loop_type >= 2)
					continue;

				loop_type_ = loop_type + 1;
				loop_start_ = loop_start;
				loop_end_ = loop_end;
				break;
			}
		}

		fseek(fileh, chunk_pos + chunk_size, SEEK_SET);
	}

	fclose(fileh);
	if (!audio_data_)
		return false;

	if (!loop_type_)
	{
		loop_start_ = 0;
		loop_end_ = n_samples_ - 1;
	}
#endif
	return true;
}

bool Wav::save(const char *filename)
{
	FILE *fileh = fopen(filename, "wb");
	if(fileh == NULL)
		return false;

	// RIFF header
	fwrite("RIFF", 1, 4, fileh);

	u32 data_chunk_size = bit_per_sample_ / 8 * n_channels_ * n_samples_;

	u32 riff_size = data_chunk_size + 32;
	fwrite(&riff_size, 4, 1, fileh);

	// WAVE header
	fwrite("WAVE", 1, 4, fileh);

	// fmt chunk
	fwrite("fmt ", 1, 4, fileh);

	u32 fmt_chunk_size = 16;
	fwrite(&fmt_chunk_size, 4, 1, fileh);

	u16 compression_code = 1; // Always PCM
	fwrite(&compression_code, 2, 1, fileh);

	u16 nch = n_channels_;
	fwrite(&nch, 2, 1, fileh);

	u32 sampling_rate = sampling_rate_;
	fwrite(&sampling_rate, 4, 1, fileh);

	u32 avg_bytes_per_sec = sampling_rate_ * bit_per_sample_ / 8 * n_channels_;
	fwrite(&avg_bytes_per_sec, 4, 1, fileh);

	u16 block_align = bit_per_sample_ / 8 * n_channels_;
	fwrite(&block_align, 2, 1, fileh);

	u16 bit_per_sample = bit_per_sample_;
	fwrite(&bit_per_sample, 2, 1, fileh);

	// data chunk
	fwrite("data", 1, 4, fileh);

	fwrite(&data_chunk_size, 4, 1, fileh);

	ntxm_dprintf("rate: %u\ndata: %lu\n", sampling_rate_, data_chunk_size);

	if(bit_per_sample == 8)
	{
		// Convert from unsigned to signed and back
		ntxm_unsigned2signed_8(audio_data_, data_chunk_size);
		fwrite(audio_data_, data_chunk_size, 1, fileh);
		ntxm_unsigned2signed_8(audio_data_, data_chunk_size);
	}
	else if(bit_per_sample == 16)
	{
		u16 *audio = (u16*)audio_data_;
		fwrite(audio, data_chunk_size, 1, fileh);
	}

	if(loop_type_)
	{
		fwrite("smpl", 1, 4, fileh);

		u32 smpl_chunk_size = 56;
		fwrite(&smpl_chunk_size, 4, 1, fileh);

		u32 tmp = 0;
		fwrite(&tmp, 4, 1, fileh); // manufacturer
		fwrite(&tmp, 4, 1, fileh); // product
		u32 sample_period = 1000000000 / sampling_rate_;
		fwrite(&sample_period, 4, 1, fileh);
		fwrite(&tmp, 4, 1, fileh); // MIDI unity note
		fwrite(&tmp, 4, 1, fileh); // MIDI pitch fraction
		fwrite(&tmp, 4, 1, fileh); // SMPTE format
		fwrite(&tmp, 4, 1, fileh); // SMPTE offset
		tmp = 1;
		fwrite(&tmp, 4, 1, fileh); // number of sample loops
		tmp = 0;
		fwrite(&tmp, 4, 1, fileh); // sample data bytes

		u32 loop_id = 0;
		fwrite(&loop_id, 4, 1, fileh);
		u32 loop_type = loop_type_ - 1;
		fwrite(&loop_type, 4, 1, fileh);
		fwrite(&loop_start_, 4, 1, fileh);
		fwrite(&loop_end_, 4, 1, fileh);
		fwrite(&tmp, 4, 1, fileh); // fraction
		fwrite(&tmp, 4, 1, fileh); // number of repeats
	}

	fclose(fileh);

	return true; // Hehe
}

/* ===================== PRIVATE ===================== */
