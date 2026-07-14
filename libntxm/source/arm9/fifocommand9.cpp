/*
 * fifocommand9.cpp
 *
 *  Created on: Mar 27, 2010
 *      Author: tob
 */

#include "ntxm/fifocommand.h"
#include "ntxm/ntxmtools.h"
#include <nds/ndstypes.h>

void (*onUpdateRow)(u16 row) = 0;
void (*onStop)(void) = 0;
void (*onPlaySampleFinished)(void) = 0;
void (*onPotPosChange)(u16 potpos) = 0;

void RegisterRowCallback(void (*onUpdateRow_)(u16))
{
	onUpdateRow = onUpdateRow_;
}

void RegisterStopCallback(void (*onStop_)(void))
{
	onStop = onStop_;
}

void RegisterPlaySampleFinishedCallback(void (*onPlaySampleFinished_)(void))
{
	onPlaySampleFinished = onPlaySampleFinished_;
}

void RegisterPotPosChangeCallback(void (*onPotPosChange_)(u16))
{
	onPotPosChange = onPotPosChange_;
}

void RecvCommandUpdateRow(UpdateRowCommand *c)
{
	if (onUpdateRow)
		onUpdateRow(c->row);
}

void RecvCommandUpdatePotPos(UpdatePotPosCommand *c)
{
	if (onPotPosChange)
		onPotPosChange(c->potpos);
}

void RecvCommandNotifyStop(void)
{
	if (onStop)
		onStop();
}

void RecvCommandSampleFinish(void)
{
	if (onPlaySampleFinished)
		onPlaySampleFinished();
}

void CommandRecvHandler(int bytes, void *user_data)
{
	u8 msg[NTXM_MESSAGE_MAX_LENGTH];

	fifoGetDatamsg(FIFO_NTXM, bytes, msg);

	switch (msg[0]) {
#ifdef DEBUG
	case DBG_OUT: // TODO it's not safe to do this in an interrupt handler
		ntxm_dprintf(((DbgOutCommand *)msg)->msg);
		break;
#endif

	case UPDATE_ROW: RecvCommandUpdateRow((UpdateRowCommand *)msg); break;

	case UPDATE_POTPOS:
		RecvCommandUpdatePotPos((UpdatePotPosCommand *)msg);
		break;

	case NOTIFY_STOP: RecvCommandNotifyStop(); break;

	case SAMPLE_FINISH: RecvCommandSampleFinish(); break;

	default: break;
	}
}

bool CommandInit()
{
	fifoSetDatamsgHandler(FIFO_NTXM, CommandRecvHandler, 0);
	//fifoSetValue32Handler(FIFO_NTXM, CommandRecvHandler, 0);
	return true;
}

void CommandExit()
{
}

void CommandPlaySample(Sample *sample, u8 note, u8 volume, u8 channel)
{
	PlaySampleCommand command;
	command.type = PLAY_SAMPLE;
	command.sample = sample;
	command.note = note;
	command.volume = volume;
	command.channel = channel;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandStopSample(int channel)
{
	StopSampleSoundCommand command;
	command.type = STOP_SAMPLE;
	command.channel = channel;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandStartRecording(u16 *buffer, int length)
{
	StartRecordingCommand command;
	command.type = START_RECORDING;
	command.buffer = buffer;
	command.length = length;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

int CommandStopRecording(void)
{
	NtxmCommand command;
	command.type = STOP_RECORDING;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
	fifoWaitValue32(FIFO_NTXM);
	return (int)fifoGetValue32(FIFO_NTXM);
}

void CommandSetSong(void *song)
{
	SetSongCommand command;
	command.type = SET_SONG;
	command.ptr = song;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandStartPlay(u8 potpos, u16 row, bool loop)
{
	StartPlayCommand command;
	command.type = START_PLAY;
	command.potpos = potpos;
	command.row = row;
	command.loop = loop;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandStopPlay(void)
{

	StopPlayCommand command;
	command.type = STOP_PLAY;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandPlayInst(u8 inst, u8 note, u8 volume, u8 channel)
{
	PlayInstCommand command;
	command.type = PLAY_INST;
	command.inst = inst;
	command.note = note;
	command.volume = volume;
	command.channel = channel;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandStopInst(u8 channel)
{
	StopInstCommand command;
	command.type = STOP_INST;
	command.channel = channel;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandStopMatchingInst(u8 inst, u8 note)
{
	StopMatchingInstCommand command;
	command.type = STOP_MATCHING_INST;
	command.note = note;
	command.inst = inst;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandPlayNoteAuto(u8 inst, u8 note, u8 volume, u16 tag)
{
	PlayNoteAutoCommand command;
	command.type = PLAY_NOTE_AUTO;
	command.inst = inst;
	command.note = note;
	command.volume = volume;
	command.tag = tag;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandStopNoteAuto(u16 tag)
{
	StopNoteAutoCommand command;
	command.type = STOP_NOTE_AUTO;
	command.tag = tag;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandMicOn(void)
{
	NtxmCommand command;
	command.type = MIC_ON;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandMicOff(void)
{
	NtxmCommand command;
	command.type = MIC_OFF;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandOnSongSpeedChanged(void)
{
	NtxmCommand command;
	command.type = ON_SONG_SPEED_CHANGED;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandSetPatternLoop(bool state)
{
	PatternLoopCommand command;
	command.type = PATTERN_LOOP;
	command.state = state;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}

void CommandSetStereoOutput(bool state)
{
	SetStereoOutputCommand command;
	command.type = SET_STEREO_OUTPUT;
	command.state = state;
	fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8 *)&command);
}
