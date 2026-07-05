#include "ntxm/fifocommand.h"
#include "ntxm/ntxmtools.h"
#include "ntxm/player.h"
#include <cstdio>

Player *player = new Player();
bool ntxm_stereo_output = false;
bool ntxm_recording = false;

void (*onUpdateRow)(u16 row) = 0;
void (*onStop)(void) = 0;
void (*onPlaySampleFinished)(void) = 0;
void (*onPotPosChange)(u16 potpos) = 0;

extern bool NtxmPlayerLock(void);
extern void NtxmPlayerUnlock(void);

void RegisterRowCallback(void (*onUpdateRow_)(u16))
{
	onUpdateRow = onUpdateRow_;
}

void RegisterStopCallback(void (*onStop_)(void)) { onStop = onStop_; }

void RegisterPlaySampleFinishedCallback(void (*onPlaySampleFinished_)(void))
{
	onPlaySampleFinished = onPlaySampleFinished_;
}

void RegisterPotPosChangeCallback(void (*onPotPosChange_)(u16))
{
	onPotPosChange = onPotPosChange_;
}

void CommandUpdateRow(u16 row)
{
	if (onUpdateRow)
		onUpdateRow(row);
}

void CommandUpdatePotPos(u16 potpos)
{
	if (onPotPosChange)
		onPotPosChange(potpos);
}

void CommandNotifyStop(void)
{
	if (onStop)
		onStop();
}

void CommandSampleFinish(void)
{
	if (onPlaySampleFinished)
		onPlaySampleFinished();
}

void CommandPlaySample(Sample *sample, u8 note, u8 volume, u8 channel)
{
	if (!player || !NtxmPlayerLock())
		return;
	player->playSample(sample, note, volume, channel);
	NtxmPlayerUnlock();
}

void CommandStopSample(int channel)
{
	if (!player || !NtxmPlayerLock())
		return;
	player->stopChannel(channel);
	NtxmPlayerUnlock();
}

void CommandStartRecording(u16 *buffer, int length) { ntxm_recording = true; }

int CommandStopRecording(void)
{
	ntxm_recording = false;
	return 0;
}

void CommandSetSong(void *song)
{
	if (!player || !NtxmPlayerLock())
		return;
	player->setSong((Song *)song);
	NtxmPlayerUnlock();
}

void CommandStartPlay(u8 potpos, u16 row, bool loop)
{
	if (!player || !NtxmPlayerLock())
		return;
	player->play(potpos, row, loop);
	NtxmPlayerUnlock();
}

void CommandStopPlay(void)
{
	if (!player || !NtxmPlayerLock())
		return;
	player->stop();
	NtxmPlayerUnlock();
}

void CommandPlayInst(u8 inst, u8 note, u8 volume, u8 channel)
{
	if (!player || !NtxmPlayerLock())
		return;
	player->playNote(note, volume, channel, inst);
	NtxmPlayerUnlock();
}

void CommandStopInst(u8 channel)
{
	if (!player || !NtxmPlayerLock())
		return;
	player->stopChannel(channel);
	NtxmPlayerUnlock();
}

void CommandStopMatchingInst(u8 inst, u8 note)
{
	if (!player || !NtxmPlayerLock())
		return;
	player->stopAllNotes(note, inst);
	NtxmPlayerUnlock();
}

void CommandPlayNoteAuto(u8 inst, u8 note, u8 volume, u16 tag)
{
	if (!player || !NtxmPlayerLock())
		return;
	player->playNoteAuto(inst, note, volume, tag);
	NtxmPlayerUnlock();
}

void CommandStopNoteAuto(u16 tag)
{
	if (!player || !NtxmPlayerLock())
		return;
	player->stopNoteAuto(tag);
	NtxmPlayerUnlock();
}

void CommandMicOn(void) {}

void CommandMicOff(void) {}

void CommandSetPatternLoop(bool state)
{
	if (!player || !NtxmPlayerLock())
		return;
	player->setPatternLoop(state);
	NtxmPlayerUnlock();
}

void CommandSetStereoOutput(bool state) { ntxm_stereo_output = state; }
