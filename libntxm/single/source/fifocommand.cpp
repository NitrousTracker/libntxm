// TODO

#include <3ds.h>
#include "ntxm/fifocommand.h"
#include "ntxm/ntxmtools.h"

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
    if(onUpdateRow)
        onUpdateRow(c->row);
}

void RecvCommandUpdatePotPos(UpdatePotPosCommand *c)
{
    if(onPotPosChange)
        onPotPosChange(c->potpos);
}

void RecvCommandNotifyStop(void)
{
    if(onStop)
        onStop();
}

void RecvCommandSampleFinish(void) {
    if(onPlaySampleFinished)
        onPlaySampleFinished();
}

void CommandInit() {
}

void CommandPlaySample(Sample *sample, u8 note, u8 volume, u8 channel)
{
}

void CommandStopSample(int channel)
{
}

void CommandStartRecording(u16* buffer, int length)
{
}

int CommandStopRecording(void)
{
    return 0;
}

void CommandSetSong(void *song)
{
}

void CommandStartPlay(u8 potpos, u16 row, bool loop)
{
}

void CommandStopPlay(void)
{
}

void CommandPlayInst(u8 inst, u8 note, u8 volume, u8 channel)
{
}

void CommandStopInst(u8 channel)
{
}

void CommandStopMatchingInst(u8 inst, u8 note)
{
}

void CommandPlayNoteAuto(u8 inst, u8 note, u8 volume, u16 tag)
{
}

void CommandStopNoteAuto(u16 tag)
{
}

void CommandMicOn(void)
{
}

void CommandMicOff(void)
{
}

void CommandSetPatternLoop(bool state)
{
}

void CommandSetStereoOutput(bool state)
{
}
