/*
 * fifocommand.h
 *
 *  Created on: Mar 27, 2010
 *      Author: tob
 */

#ifndef FIFOCOMMAND_H_
#define FIFOCOMMAND_H_

#include "ntxm/sample.h"
#include <stdbool.h>
#include <stdio.h>

#define FIFO_NTXM FIFO_USER_01
#ifdef DEBUG
#define DEBUGSTRSIZE 40
#define NTXM_MESSAGE_MAX_LENGTH 48
#else
#define NTXM_MESSAGE_MAX_LENGTH 16
#endif

typedef enum {
	PLAY_SAMPLE,
	STOP_SAMPLE,
	START_RECORDING,
	STOP_RECORDING,
	SET_SONG,
	START_PLAY,
	STOP_PLAY,
	DBG_OUT,
	UPDATE_ROW,
	UPDATE_POTPOS,
	PLAY_INST,
	STOP_INST,
	STOP_MATCHING_INST,
	PLAY_NOTE_AUTO,
	STOP_NOTE_AUTO,
	NOTIFY_STOP,
	MIC_ON,
	MIC_OFF,
	PATTERN_LOOP,
	SAMPLE_FINISH,
	SET_STEREO_OUTPUT,
	ON_SONG_SPEED_CHANGED
} NTXMFifoMessageType;

struct NtxmCommand {
	u8 type;
};

struct PlaySampleCommand {
	u8 type;
	u8 note;
	u8 volume;
	u8 channel;
	Sample *sample;
};

/* Command parameters for stopping a sample */
struct StopSampleSoundCommand {
	u8 type;
	u8 channel;
};

/* Command parameters for starting to record from the microphone */
struct StartRecordingCommand {
	u8 type;
	u16 frequency;
	u16 *buffer;
	int length;
};

struct SetSongCommand {
	u8 type;
	void *ptr;
};

struct StartPlayCommand {
	u8 type;
	u16 row;
	u8 potpos;
	bool loop;
};

struct StopPlayCommand {
	u8 type;
};

struct UpdateRowCommand {
	u8 type;
	u16 row;
};

struct UpdatePotPosCommand {
	u8 type;
	u16 potpos;
};

struct PlayInstCommand {
	u8 type;
	u8 inst;
	u8 note;
	u8 volume;
	u8 channel;
};

struct StopInstCommand {
	u8 type;
	u8 channel;
};

struct StopMatchingInstCommand {
	u8 type;
	u8 inst;
	u8 note;
};

struct PlayNoteAutoCommand {
	u8 type;
	u8 inst;
	u8 note;
	u8 volume;
	u16 tag;
};

struct StopNoteAutoCommand {
	u8 type;
	u16 tag;
};

struct PatternLoopCommand {
	u8 type;
	bool state;
};

struct SetStereoOutputCommand {
	u8 type;
	bool state;
};

bool CommandInit();
void CommandExit();

#if !defined(NT_PLATFORM_NDS) || defined(ARM9)
void CommandPlayOneShotSample(int channel, int frequency, const void *data,
                              int length, int volume, int format, bool loop);
void CommandPlaySample(Sample *sample, u8 note, u8 volume, u8 channel);
void CommandPlaySample(Sample *sample);
void CommandStopSample(int channel);
void CommandStartRecording(int frequency, u16 *buffer, int length);
int CommandStopRecording(void);
void CommandSetSong(void *song);
void CommandStartPlay(u8 potpos, u16 row, bool loop);
void CommandStopPlay(void);
void CommandPlayInst(u8 inst, u8 note, u8 volume, u8 channel);
void CommandStopInst(u8 channel);
void CommandStopMatchingInst(u8 inst, u8 note);
void CommandPlayNoteAuto(u8 inst, u8 note, u8 volume, u16 tag);
void CommandStopNoteAuto(u16 tag);
void CommandMicOn(void);
void CommandMicOff(void);
void CommandSetPatternLoop(bool state);
void CommandSetStereoOutput(bool state);
void CommandOnSongSpeedChanged(void);

void RegisterRowCallback(void (*onUpdateRow_)(u16));
void RegisterStopCallback(void (*onStop_)(void));
void RegisterPlaySampleFinishedCallback(void (*onPlaySampleFinished_)(void));
void RegisterPotPosChangeCallback(void (*onPotPosChange_)(u16));
#endif

#if !defined(NT_PLATFORM_NDS) || defined(ARM7)
void CommandUpdateRow(u16 row);
void CommandUpdatePotPos(u16 potpos);
void CommandNotifyStop(void);
void CommandSampleFinish(void);
#endif

#if !defined(NT_PLATFORM_NDS)
void CommandSetPlaybackFrequency(u32 freq);
#endif

#endif /* FIFOCOMMAND_H_ */
