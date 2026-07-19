/*
 * fifocommand7.cpp
 *
 *  Created on: Mar 28, 2010
 *      Author: tob
 */

#include <nds.h>
#include <stdarg.h>

#include "ntxm/fifocommand.h"

#include "ntxm/ntxm7.h"
#include "ntxm/player.h"

extern NTXM7 *ntxm7;
bool ntxm_recording = false;
bool ntxm_mic_recording = false;
bool ntxm_stereo_output = false;
int ntxm_record_buffer_size = 0;
int ntxm_record_max_buffer_size = 0;

static void MicBufSwapCallback(u8 *completedBuffer, int length)
{
	if (!ntxm_mic_recording)
		return;

	if (length > 0) {
		ntxm_record_buffer_size += length;
		if (ntxm_record_buffer_size >= ntxm_record_max_buffer_size) {
			ntxm_mic_recording = false;
			micStopRecording();
		}
	}
}

static void RecvCommandPlaySample(PlaySampleCommand *ps)
{
	ntxm7->playSample(ps->sample, ps->note, ps->volume, ps->channel);
}

static void RecvCommandStopSample(StopSampleSoundCommand *ss)
{
	ntxm7->stopChannel(ss->channel);
}

static void RecvCommandOnSongSpeedChanged(void)
{
	ntxm7->onSongSpeedChanged();
}

static void RecvCommandMicOn(void)
{
	micOn();
}

static void RecvCommandMicOff(void)
{
	micOff();
}

static void RecvCommandStartRecording(StartRecordingCommand *sr)
{
	ntxm_mic_recording = true;
	ntxm_recording = true;
	ntxm_record_buffer_size = 0;
	ntxm_record_max_buffer_size = sr->length;
	micStartRecording((u8 *)sr->buffer, sr->length, 16384, 1, false,
	                  MicBufSwapCallback);
}

static void RecvCommandStopRecording()
{
	if (ntxm_mic_recording)
		micStopRecording(); // buffer size in samples
	fifoSendValue32(FIFO_NTXM, ntxm_record_buffer_size);
	ntxm_mic_recording = false;
	ntxm_recording = false;
}

static void RecvCommandSetSong(SetSongCommand *c)
{
	ntxm7->setSong((Song *)c->ptr);
}

static void RecvCommandStartPlay(StartPlayCommand *c)
{
	ntxm7->play(c->loop, c->potpos, c->row);
}

static void RecvCommandStopPlay(StopPlayCommand *c)
{
	ntxm7->stop();
}

static void RecvCommandPlayInst(PlayInstCommand *c)
{
	ntxm7->playNote(c->inst, c->note, c->volume, c->channel);
}

static void RecvCommandStopInst(StopInstCommand *c)
{
	ntxm7->stopChannel(c->channel);
}

static void RecvCommandPlayNoteAuto(PlayNoteAutoCommand *c)
{
	ntxm7->playNoteAuto(c->inst, c->note, c->volume, c->tag);
}

static void RecvCommandStopNoteAuto(StopNoteAutoCommand *c)
{
	ntxm7->stopNoteAuto(c->tag);
}

static void RecvCommandStopMatchingInst(StopMatchingInstCommand *c)
{
	ntxm7->stopAllNotes(c->note, c->inst);
}

static void RecvCommandPatternLoop(PatternLoopCommand *c)
{
	ntxm7->setPatternLoop(c->state);
}

static void RecvCommandSetStereoOutput(SetStereoOutputCommand *c)
{
	ntxm_stereo_output = c->state;
}

void CommandUpdateRow(u16 row)
{
	UpdateRowCommand command;
	command.type = UPDATE_ROW;
	command.row = row;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandUpdatePotPos(u16 potpos)
{
	UpdatePotPosCommand command;
	command.type = UPDATE_POTPOS;
	command.potpos = potpos;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandNotifyStop(void)
{
	NtxmCommand command;
	command.type = NOTIFY_STOP;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandSampleFinish(void)
{
	NtxmCommand command;
	command.type = SAMPLE_FINISH;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandRecvHandler(int bytes, void *user_data)
{
	u8 command[NTXM_MESSAGE_MAX_LENGTH];

	fifoGetDatamsg(FIFO_NTXM, bytes, command);

	switch (command[0]) {
	case PLAY_SAMPLE:
		RecvCommandPlaySample((PlaySampleCommand *)command);
		break;
	case STOP_SAMPLE:
		RecvCommandStopSample((StopSampleSoundCommand *)command);
		break;
	case START_RECORDING:
		RecvCommandStartRecording((StartRecordingCommand *)command);
		break;
	case STOP_RECORDING: RecvCommandStopRecording(); break;
	case SET_SONG: RecvCommandSetSong((SetSongCommand *)command); break;
	case START_PLAY: RecvCommandStartPlay((StartPlayCommand *)command); break;
	case STOP_PLAY: RecvCommandStopPlay((StopPlayCommand *)command); break;
	case PLAY_INST: RecvCommandPlayInst((PlayInstCommand *)command); break;
	case STOP_INST: RecvCommandStopInst((StopInstCommand *)command); break;
	case STOP_MATCHING_INST:
		RecvCommandStopMatchingInst((StopMatchingInstCommand *)command);
		break;
	case PLAY_NOTE_AUTO:
		RecvCommandPlayNoteAuto((PlayNoteAutoCommand *)command);
		break;
	case STOP_NOTE_AUTO:
		RecvCommandStopNoteAuto((StopNoteAutoCommand *)command);
		break;
	case MIC_ON: RecvCommandMicOn(); break;
	case MIC_OFF: RecvCommandMicOff(); break;
	case PATTERN_LOOP:
		RecvCommandPatternLoop((PatternLoopCommand *)command);
		break;
	case SET_STEREO_OUTPUT:
		RecvCommandSetStereoOutput((SetStereoOutputCommand *)command);
		break;
	case ON_SONG_SPEED_CHANGED: RecvCommandOnSongSpeedChanged(); break;
	default: break;
	}
}

bool CommandInit(void)
{
	fifoSetDatamsgHandler(FIFO_NTXM, CommandRecvHandler, 0);
	//fifoSetValue32Handler(FIFO_NTXM, CommandRecvHandler, 0);
	return true;
}

void CommandExit(void)
{
}
