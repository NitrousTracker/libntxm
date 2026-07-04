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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

extern "C" {
  #include "tables.h"
}

#include "ntxm/sample.h"
#include "ntxm/fifocommand.h"
#include "ntxm/ntxmtools.h"
#include "ntxm/ntxmsound.h"

#define MAX(x,y)						((x)>(y)?(x):(y))
#define LOOKUP_FREQ(note,finetune)		(linear_freq_table_lookup(MAX(0,N_FINETUNE_STEPS*(note)+(finetune))))
#define GET_FREQ_DIRECT(fine_step)		(linear_freq_table_lookup(MAX(0,fine_step)))

/* ===================== PUBLIC ===================== */

uint32_t ntxmGetFrequencyValue(uint16_t period, bool linear) {
    if (!period) {
            return 1;
    }
    if (linear) {
        const uint16_t invPeriod = (12 * 192 * 4) - period; // 8bb: this intentionally underflows uint16_t to be accurate to FT2

		const uint32_t quotient = invPeriod / 768;
		const uint32_t remainder = invPeriod % 768;

		const int32_t octShift = (14 - quotient) & 31; // 8bb: added needed 32-bit bitshift mask

		return (uint32_t)(((int64_t)logTab[remainder] * 2140928) >> 24) >> octShift;
    } else {
        return 14317456 / period;
    }
}

#define N_FINETUNE_STEPS 128
#define BASE_NOTE 48
#define N_RELNOTES (BASE_NOTE + 48)
#define LINEAR_FREQ_TABLE_MAX (N_RELNOTES*N_FINETUNE_STEPS)

inline u32 linear_freq_table_lookup(u32 freqpos)
{
    bool linear = true;
	u32 finetune = freqpos%N_FINETUNE_STEPS;
	u32 note = freqpos/N_FINETUNE_STEPS;

    return ntxmGetFrequencyValue(10*12*16*4 - note*16*4 - finetune/2, linear);
}

#ifndef ARM7

Sample::Sample(void *_sound_data, u32 _n_samples, u16 _sampling_frequency, bool _is_16_bit,
	u8 _loop, u8 _volume)
	:pingpong_data(0), n_samples(_n_samples), is_16_bit(_is_16_bit), loop(_loop),
	loop_start(0), loop_length(0), volume(_volume), panning(128)
{
	sound_data = _sound_data;

	memset(name, 0, SAMPLE_NAME_LENGTH);

	calcSize();

	setFormat();
	calcRelnoteAndFinetune(_sampling_frequency);

	setLoopStartAndLength(0, _n_samples);
}

Sample::Sample(const char *filename, u8 _loop, bool *_success)
	:pingpong_data(0), loop(_loop), loop_start(0), loop_length(0), volume(255),
	panning(128)
{
	sound_data = (void**)ntxm_ccalloc(20*sizeof(void*), 1);

	if(!wav.load(filename))
	{
		ntxm_dprintf("WAV loading failed\n");
		*_success = false;
		return;
	}

	const char *smpname = strrchr(filename, '/') + 1;
	strncpy(name, smpname, SAMPLE_NAME_LENGTH);
	name[SAMPLE_NAME_LENGTH] = 0;

	if (sound_data) ntxm_free(sound_data);
	sound_data = wav.getAudioData();

	calcRelnoteAndFinetune( wav.getSamplingRate() );

	u8 bit_per_sample = wav.getBitPerSample();
	is_16_bit = (bit_per_sample == 16);

	if(wav.getCompression() == CMP_ADPCM)
		sound_format = NTXMSOUND_FORMAT_ADPCM;
	else
		setFormat();

	n_samples = wav.getNSamples();

	/*
	if(sound_format == NTXMSOUND_FORMAT_ADPCM) {
		n_samples = wav.getNSamples() * 4; // ADPCM compresses 4 samples in 1
	} else {
		n_samples = wav.getNSamples();
	}*/

	calcSize();

	if(wav.isStereo() == true)
	{
		if(!convertStereoToMono())
		{
			ntxm_dprintf("Stereo 2 Mono conversion failed\n");
			*_success = false;
			return;
		}
	}

	setLoopStartAndLength(wav.getLoopStart(), wav.getLoopEnd() - wav.getLoopStart() + 1);
	setLoop(wav.getLoopType());

	*_success = true;
}

Sample::~Sample()
{
	if(pingpong_data)
		removePingPongLoop();

	if(sound_data)
		ntxm_free(sound_data);
}

void Sample::saveAsWav(char *filename)
{
	wav.setCompression(0);
	wav.setNChannels(1);
	wav.setSamplingRate(LOOKUP_FREQ(rel_note+96,finetune));
	wav.setBitPerSample(is_16_bit?16:8);
	wav.setNSamples(n_samples);
	wav.setAudioData((u8*)getData());
	wav.setLoopType(getLoop());
	wav.setLoopStart(getLoopStart());
	wav.setLoopEnd(getLoopStart() + getLoopLength() - 1);
	wav.save(filename);
}

#endif

#if defined(ARM7) || !defined(NT_PLATFORM_NDS)

// volume_ ranges from 0-127. The value 255 means "no volume", i.e. the sample's own volume shall be used.
void Sample::play(u8 channel, u8 panning, u8 volume, u8 offs)
{
	if(channel>MAX_CHANNELS) return;

	u32 loop_bit;
	if( ( ( loop == FORWARD_LOOP ) || (loop == PING_PONG_LOOP) ) && (loop_length > 0) )
		loop_bit = NTXMSOUND_REPEAT;
	else
		loop_bit = NTXMSOUND_ONE_SHOT;

	ntxm_sound_channel_stop(channel);

	u32 offs_samps = FT_OFFSET_SCALAR * offs * (sound_format == NTXMSOUND_FORMAT_8BIT ? 1 : 2);

	// todo: only semi working with looping samples (for now)
	// if the offset is less than the loop start it works fine (ty to exelotl :-D)

	// a fully working version of this would probably have to allocate more memory at the
	// start of the sound data specifically for the initial offset playback, either that
	// or treat it as two separate notes and play the second one from the loop start as
	// soon as the offset one ends

	if (offs_samps > size)
	{
		return;
	}
	if (loop == NO_LOOP)
	{
		ntxm_sound_channel_set_source(channel, (uint8_t*)sound_data + offs_samps, 0, size - offs_samps);
	}
	else if( loop == FORWARD_LOOP || (loop == PING_PONG_LOOP && !pingpong_data) )
	{
		u32 loop_offs_samps = ntxm_clamp(offs_samps, 0, loop_start);
		ntxm_sound_channel_set_source(channel, (uint8_t*)sound_data + loop_offs_samps, loop_start - loop_offs_samps, loop_length);
	}
	else if( loop == PING_PONG_LOOP )
	{
		u32 loop_offs_samps = ntxm_clamp(offs_samps, 0, loop_start);
		ntxm_sound_channel_set_source(channel, (uint8_t*)pingpong_data + loop_offs_samps, loop_start - loop_offs_samps, loop_length << 1);
	}

    ntxm_sound_channel_play(channel, loop_bit, sound_format, panning, volume);
}

#endif

u32 Sample::calcPlayLength(u8 note)
{
	u32 samples_per_second = LOOKUP_FREQ(48+note+rel_note,finetune);
	if (samples_per_second == 0) return 0;
	return n_samples * 1000 / samples_per_second;
}

#ifndef ARM7

void Sample::setRelNote(s8 _rel_note) {
	rel_note = _rel_note;
}

void Sample::setFinetune(s8 _finetune) {
	finetune = _finetune;
}

#endif

u8 Sample::getRelNote(void) {
	return rel_note;
}

s8 Sample::getFinetune(void) {
	return finetune;
}

u32 Sample::getSize(void)
{
	return size;
}

u32 Sample::getNSamples(void)
{
	return n_samples;
}

void *Sample::getData(void)
{
	return sound_data;
}

u8 Sample::getLoop(void) {
	return loop;
}

#ifndef ARM7

bool Sample::setLoop(u8 loop_) // Set loop type. Can fail due to memory constraints
{
	if(loop_ == loop)
		return true;

	if(loop_ >= LOOP_TYPE_COUNT)
		loop_ = NO_LOOP;

	if(loop == PING_PONG_LOOP) // Switching from ping-pong to sth else
		removePingPongLoop();

	loop = loop_;

	if(loop_ == NO_LOOP)
	{
		setLoopStartAndLength(0, n_samples);
	}

	if(loop_ == PING_PONG_LOOP)
	{
		if (!setupPingPongLoop())
			return false;
	}

	return true;
}

#endif

bool Sample::is16bit(void) {
	return is_16_bit;
}

u32 Sample::getLoopStart(void)
{
	if(is_16_bit)
		return loop_start / 2;
	else
		return loop_start;
}

#ifndef ARM7

void Sample::setLoopStartAndLength(u32 _loop_start, u32 _loop_length)
{
	u32 min_loop_length = (4 >> (is_16_bit ? 1 : 0)) >> (loop == PING_PONG_LOOP ? 1 : 0);

	// Clamping
	if(_loop_start >= (n_samples - min_loop_length)) _loop_start = n_samples - min_loop_length;
	if(_loop_length >= (n_samples - _loop_start)) _loop_length = n_samples - _loop_start;

	// NDS fix: If loop length is 0, it won't play the beginning of the sample until the loop
	if(_loop_length < min_loop_length)
		_loop_length = min_loop_length;

	if(is_16_bit)
	{
		_loop_length *= 2;
		_loop_start *= 2;
	}

	if(loop_length != _loop_length || loop_start != _loop_start)
	{
		loop_length = _loop_length;
		loop_start = _loop_start;

		onSampleDataChanged();
	}
}

#endif

u32 Sample::getLoopLength(void)
{
	if(is_16_bit)
		return loop_length / 2;
	else
		return loop_length;
}

void Sample::setVolume(u8 vol) {
	volume = vol;
}

u8 Sample::getVolume(void) {
	return volume;
}

void Sample::setPanning(u8 pan)
{
	panning = pan;
}

u8 Sample::getPanning(void)
{
	return panning;
}

void Sample::setName(const char *name_)
{
	strncpy(name, name_, SAMPLE_NAME_LENGTH-1);
}

const char *Sample::getName(void)
{
	return name;
}

#ifndef ARM7


void Sample::delAll(void)
{
	if(sound_data)
		ntxm_free(sound_data);
	sound_data = NULL;

	n_samples = 0;

	loop = NO_LOOP;
	loop_start = loop_length = 0;

	onSampleDataChanged();
	return;
}

// Deletes the part between start sample and end sample
void Sample::delPart(u32 startsample, u32 endsample)
{
	if(endsample >= n_samples)
		endsample = n_samples-1;

	// Special case: everything is deleted
	if((startsample==0)&&(endsample==n_samples-1))
	{
		delAll();
		return;
	}

	u32 new_n_samples = n_samples - (endsample - startsample + 1);

	u8 bps;
	if(is_16_bit) bps=2; else bps=1;

	// Copy the data after the deleted part
	if(endsample < n_samples - 1)
	{
		memmove((u8*)sound_data + startsample * bps, (u8*)sound_data + (endsample + 1) * bps, ((n_samples - 1) - endsample) * bps);
	}
	sound_data = ntxm_crealloc(sound_data, new_n_samples * bps);

	n_samples = new_n_samples;

	// Now everything's clear and we set the variables right
	calcSize();

	// Update Loop
	u32 loop_end = loop_start + loop_length;
	u32 start = startsample * bps;
	u32 end = endsample * bps;
	u32 del = end - start + 1;

	if(loop != NO_LOOP)
	{
		if(start < loop_end)
		{
			if(start > loop_start)
			{
				if(end < loop_end)
				{
					loop_length -= del;
				}
				else
				{
					loop_length = start - loop_start;
				}
			}
			else
			{
				if(end > loop_end)
				{
					loop_start = 0;
					loop_length = getSize();
				}
				else if(end > loop_start)
				{
					loop_length -= end - loop_start;
					loop_start = start;
				}
				else
				{
					loop_start -= del;
				}
			}
		}
	}

	setLoopStartAndLength(getLoopStart(), getLoopLength());
	onSampleDataChanged();
}

void Sample::fadeIn(u32 startsample, u32 endsample)
{
	fade(startsample, endsample, true);

	onSampleDataChanged();
}

void Sample::fadeOut(u32 startsample, u32 endsample)
{
	fade(startsample, endsample, false);

	onSampleDataChanged();
}

bool Sample::reverse(u32 startsample, u32 endsample)
{
	void *data = getData();
	u32 nsamples = getNSamples();

	if(endsample >= nsamples)
		endsample = nsamples-1;

	s32 offset = startsample;
	s32 length = endsample - startsample;

	// Do it!
	if(is_16_bit == true)
	{
		s16 *new_sounddata = (s16*)ntxm_umalloc(2 * length);
		if (new_sounddata == NULL)
			return false;
		s16 *sounddata = (s16*)(data);

		// First reverse the selected region
		for(s32 i=0;i<length;++i) {
			new_sounddata[i] = sounddata[offset+length-1-i];
		}

		// Then copy it into the sample
		memcpy(sounddata + offset, new_sounddata, 2 * length);

		ntxm_free(new_sounddata);

	} else {

		s8 *new_sounddata = (s8*)ntxm_umalloc(length);
		if (new_sounddata == NULL)
			return false;
		s8 *sounddata = (s8*)(data);

		// First reverse the selected region
		for(s32 i=0;i<length;++i) {
			new_sounddata[i] = sounddata[offset+length-1-i];
		}

		// Then copy it into the sample
		memcpy(sounddata + offset, new_sounddata, length);

		ntxm_free(new_sounddata);
	}

	onSampleDataChanged();

	return true;
}

u32 Sample::getDynamicRange(void)
{
	if (is_16_bit == true)
		return 0xffff + 1;
	else
		return 0xff + 1;
}

u32 Sample::getMaxAmplitude(u32 startsample, u32 endsample)
{
	void *data = getData();
	u32 max_smp = 0;
	u32 dr = getDynamicRange() / 2;

	if(is_16_bit == true)
	{
		s16 *sounddata = (s16*)(data);

		for(u32 i=startsample;i<endsample;++i) {
			u32 ampl = abs((s32)sounddata[i]);
			max_smp = MAX(ampl, max_smp);
			if (ampl == dr) return dr;
		}

	} else {
		s8 *sounddata = (s8*)(data);

		for(u32 i=startsample;i<endsample;++i) {
			u32 ampl = abs((s32)sounddata[i]);
			max_smp = MAX(ampl, max_smp);
			if (ampl == dr) return dr;
		}
	}

	return max_smp;
}

void Sample::normalize(u16 percent, u32 startsample, u32 endsample)
{
	void *data = getData();

	if(is_16_bit == true)
	{
		s16 *sounddata = (s16*)(data);
		s32 smp;

		for(u32 i=startsample;i<endsample;++i) {
			smp = (s32)percent * (s32)sounddata[i] / 100;

			smp = ntxm_clamp(smp, -32768, 32767);

			sounddata[i] = smp;
		}

	} else {

		s8 *sounddata = (s8*)(data);
		s16 smp;

		for(u32 i=startsample;i<endsample;++i) {
			smp = (s32)percent * (s32)sounddata[i] / 100;

			smp = ntxm_clamp(smp, -128, 127);

			sounddata[i] = smp;
		}
	}

	onSampleDataChanged();
}

void Sample::drawLine(int x1, int y1, int x2, int y2)
{
	x1 = ntxm_clamp(x1, 0, n_samples-1);
	x2 = ntxm_clamp(x2, 0, n_samples-1);
	int minval = is_16_bit?-32768:-128;
	int maxval = is_16_bit?32767:127;
	y1 = ntxm_clamp(y1, minval, maxval);
	y2 = ntxm_clamp(y2, minval, maxval);

	void *data = getData();
	s16 *sounddata16 = (s16*)(data);
	s8 *sounddata8 = (s8*)(data);

	// Guarantees that all lines go from left to right
	if ( x2 < x1 ) {
		int tmp = x2; x2 = x1; x1 = tmp;
		tmp = y2; y2 = y1; y1 = tmp;
	}
	s32 dy = y2 - y1, dx = x2 - x1;
	// If the gradient is greater than one we have to flip the axes
	if ( abs(dy) < dx )	{
		s32 add = 1;
		int xp = x1, yp = y1;
		if(dy < 0) {
			dy = -dy;
			add =- 1;
		}
		s32 d = 2*dy - dx;
		for(; xp<=x2; xp++)	{
			if(d > 0) {
				yp += add;
				d -= 2 * dx;
			}
			if(is_16_bit) sounddata16[xp] = yp; else sounddata8[xp] = yp;
			d += 2 * dy;
		}
	} else {
		int tmp = x1; x1 = y1; y1 = tmp;
		tmp = x2; x2 = y2; y2 = tmp;
		if ( x2 < x1 ) {
			tmp = x2; x2 = x1; x1 = tmp;
			tmp = y2; y2 = y1; y1 = tmp;
		}
		dy = y2 - y1; dx = x2 - x1;
		s32 add = 1;
		if(dy < 0) {
			dy = -dy;
			add=-1;
		}
		int xp = x1, yp = y1;
		s32 d = 2 * dy - dx;
		for(xp=x1; xp<=x2; xp++) {
			if(d > 0) {
				yp += add;
				d -= 2 * dx;
			}
			if(is_16_bit) sounddata16[yp] = xp; else sounddata8[yp] = xp;
			d += 2 * dy;
		}
	}

	onSampleDataChanged();
}

#endif

/* ===================== PRIVATE ===================== */

void Sample::calcSize(void)
{
	if(is_16_bit) {
		size = n_samples*2;
	} else {
		size = n_samples;
	}
}

#ifndef ARM7

void Sample::setFormat(void) {

	// TODO ADPCM and stuff
	if(is_16_bit) {
		sound_format = NTXMSOUND_FORMAT_16BIT;
	} else {
		sound_format = NTXMSOUND_FORMAT_8BIT;
	}
}

int fncompare (const void *elem1, const void *elem2 )
{
	if ( *(u16*)elem1 < *(u16*)elem2) return -1;
	else if (*(u16*)elem1 == *(u16*)elem2) return 0;
	else return 1;
}

// Takes the sampling rate in hz and searches for FT2-compatible values for
// finetune and rel_note in the freq_table
void Sample::calcRelnoteAndFinetune(u32 freq)
{
	u16 freqpos = findClosestFreq(freq);

	finetune = freqpos%N_FINETUNE_STEPS;
	rel_note = freqpos/N_FINETUNE_STEPS - BASE_NOTE;

	ntxm_dprintf("freq=%d -> relnote=%d finetune=%d\n", freq, rel_note, finetune);
}

// finds the freq in the freq table that is closest to freq ^^
u16 Sample::findClosestFreq(u32 freq)
{
    size_t left = 0;
    size_t right = LINEAR_FREQ_TABLE_MAX;

    if (freq <= linear_freq_table_lookup(0))
        return 0;
    if (freq >= linear_freq_table_lookup(right - 1))
        return right - 1;

    while (left <= right)
    {
        size_t middle = (left + right) / 2;
        u32 middle_freq = linear_freq_table_lookup(middle);

        if (freq == middle_freq)
            return middle;
        else if (freq < middle_freq)
            right = middle - 1;
        else
            left = middle + 1;
    }

    // left > right
    int diff_left = linear_freq_table_lookup(left) - freq;
    int diff_right = freq - linear_freq_table_lookup(right);
    return diff_left <= diff_right ? left : right;
}

bool Sample::convertStereoToMono(void)
{
	void *_tmpbuf = ntxm_umalloc(size);
	if(!_tmpbuf)
	{
		ntxm_dprintf("not enough ram for stereo 2 mono conversion\n");
		return false;
	}

	if(is_16_bit == true)
	{
		// Make a buffer for the converted sample
		s16 *tmpbuf = (s16*)_tmpbuf;
		s16 *src = (s16*)sound_data;

		// Convert the sample down
		s32 smp;
		for(u32 i=0; i<size/2; ++i) {
			smp = src[2*i] + src[2*i+1];
			tmpbuf[i] = smp / 2;
		}

		// Overwrite the original with the converted sample
		memcpy(sound_data, tmpbuf, size);

		// Delete the temporary buffer
		ntxm_free(tmpbuf);
	}
	else
	{
		// Make a buffer for the converted sample
		s8 *tmpbuf = (s8*)_tmpbuf;
		s8 *src = (s8*)sound_data;

		// Convert the sample down
		s32 smp;
		for(u32 i=0; i<size; ++i) {
			smp = src[2*i] + src[2*i+1];
			tmpbuf[i] = smp / 2;
		}

		// Overwrite the original with the converted sample
		memcpy(sound_data, tmpbuf, size);

		// Delete the temporary buffer
		ntxm_free(tmpbuf);
	}
	return true;
}

void Sample::fade(u32 startsample, u32 endsample, bool in)
{
	void *data = getData();
	u32 nsamples = getNSamples();

	if(endsample >= nsamples)
		endsample = nsamples-1;

	s32 offset = startsample;
	s32 length = endsample - startsample + 1;

	if(is_16_bit == true)
	{
		s16 *sounddata = (s16*)(data);

		if(in==true) {
			for(s32 i=0;i<length;++i) {
				sounddata[offset+i] = (((i * 1024) / length) * sounddata[offset+i]) / 1024;
			}
		} else {
			for(s32 i=0;i<length;++i) {
				sounddata[offset+i] = ((((length-i) * 1024) / length) * sounddata[offset+i]) / 1024;
			}
		}
	}
	else
	{
		s8 *sounddata = (s8*)(data);

		if(in==true) {
			for(s32 i=0;i<length;++i) {
				sounddata[offset+i] = i * sounddata[offset+i] / length;
			}
		} else {
			for(s32 i=0;i<length;++i) {
				sounddata[offset+i] = (length-i) * sounddata[offset+i] / length;
			}
		}
	}

	onSampleDataChanged();
}

bool Sample::setupPingPongLoop(void)
{
	pingpong_data = ntxm_umalloc(size + loop_length);
	if (!pingpong_data)
		return false;

	// Copy sound data until loop end
	memcpy(pingpong_data, sound_data, loop_start + loop_length);

	// Copy reverse loop
	if(is_16_bit)
	{
		s16 *orig = (s16*)sound_data;
		s16 *pp = (s16*)pingpong_data;
		u32 pos = (loop_start + loop_length) / 2;

		for(u32 i=0; i<loop_length/2; ++i)
			pp[pos+i] = orig[pos-i-1];
	}
	else
	{
		s8 *orig = (s8*)sound_data;
		s8 *pp = (s8*)pingpong_data;
		u32 pos = loop_start + loop_length;

		for(u32 i=0; i<loop_length; ++i)
			pp[pos+i] = orig[pos-i-1];
	}

	// Copy rest
	u32 pos = loop_start + loop_length;

	memcpy((u8*)pingpong_data + pos + loop_length, (u8*)sound_data + pos, size - pos);

	ntxm_flush_dcache();
	return true;
}

void Sample::removePingPongLoop(void)
{
	if (pingpong_data)
	{
		ntxm_free(pingpong_data);
		pingpong_data = NULL;
	}
}

bool Sample::onSampleDataChanged(void)
{
	if(pingpong_data)
		removePingPongLoop();

	calcSize();

	if(loop == PING_PONG_LOOP)
		if(!setupPingPongLoop())
			return false;
	return true;
}

#endif
