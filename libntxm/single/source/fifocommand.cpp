// TODO

#include "ntxm/fifocommand.h"
#include "ntxm/ntxmtools.h"
#include "ntxm/player.h"

static Player *player = new Player();
bool ntxm_stereo_output = false;

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
    player->playSample(sample, note, volume, channel);
}

void CommandStopSample(int channel)
{
    player->stopChannel(channel);
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
    player->setSong((Song*) song);
}

void CommandStartPlay(u8 potpos, u16 row, bool loop)
{
    player->play(loop, potpos, row);
}

void CommandStopPlay(void)
{
    player->stop();
}

void CommandPlayInst(u8 inst, u8 note, u8 volume, u8 channel)
{
    player->playNote(inst, note, volume, channel);
}

void CommandStopInst(u8 channel)
{
    player->stopChannel(channel);
}

void CommandStopMatchingInst(u8 inst, u8 note)
{
    player->stopAllNotes(note, inst);
}

void CommandPlayNoteAuto(u8 inst, u8 note, u8 volume, u16 tag)
{
    player->playNoteAuto(inst, note, volume, tag);
}

void CommandStopNoteAuto(u16 tag)
{
    player->stopNoteAuto(tag);
}

void CommandMicOn(void)
{
}

void CommandMicOff(void)
{
}

void CommandSetPatternLoop(bool state)
{
    player->setPatternLoop(state);
}

void CommandSetStereoOutput(bool state)
{
    ntxm_stereo_output = state;
}
