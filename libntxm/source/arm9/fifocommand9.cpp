/*
 * fifocommand9.cpp
 *
 *  Created on: Mar 27, 2010
 *      Author: tob
 */

#include <nds/ndstypes.h>
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

void CommandRecvHandler(int bytes, void *user_data) {
    NTXMFifoMessage msg;

    fifoGetDatamsg(FIFO_NTXM, bytes, (u8*)&msg);

    switch(msg.commandType) {
#ifdef DEBUG
        case DBG_OUT: // TODO it's not safe to do this in an interrupt handler
            ntxm_dprintf(msg.dbgOut.msg);
            break;
#endif

        case UPDATE_ROW:
            RecvCommandUpdateRow(&msg.updateRow);
            break;

        case UPDATE_POTPOS:
            RecvCommandUpdatePotPos(&msg.updatePotPos);
            break;

        case NOTIFY_STOP:
            RecvCommandNotifyStop();
            break;

        case SAMPLE_FINISH:
            RecvCommandSampleFinish();
            break;

        default:
            break;
    }
}

bool CommandInit() {
    fifoSetDatamsgHandler(FIFO_NTXM, CommandRecvHandler, 0);
    //fifoSetValue32Handler(FIFO_NTXM, CommandRecvHandler, 0);
    return true;
}

void CommandExit() {

}

void CommandPlaySample(Sample *sample, u8 note, u8 volume, u8 channel)
{
    NTXMFifoMessage command;
    PlaySampleCommand* ps = &command.playSample;

    command.commandType = PLAY_SAMPLE;

    ps->sample = sample;
    ps->note = note;
    ps->volume = volume;
    ps->channel = channel;


    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandStopSample(int channel)
{
    NTXMFifoMessage command;
    StopSampleSoundCommand *ss = &command.stopSample;

    command.commandType = STOP_SAMPLE;
    ss->channel = channel;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandStartRecording(u16* buffer, int length)
{
    NTXMFifoMessage command;
    StartRecordingCommand* sr = &command.startRecording;

    command.commandType = START_RECORDING;
    sr->buffer = buffer;
    sr->length = length;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

int CommandStopRecording(void)
{
    NTXMFifoMessage command;
    command.commandType = STOP_RECORDING;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);

    fifoWaitValue32(FIFO_NTXM);

    return (int)fifoGetValue32(FIFO_NTXM);
}

void CommandSetSong(void *song)
{
    NTXMFifoMessage command;
    SetSongCommand* c = &command.setSong;

    command.commandType = SET_SONG;
    c->ptr = song;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandStartPlay(u8 potpos, u16 row, bool loop)
{
    NTXMFifoMessage command;
    StartPlayCommand* c = &command.startPlay;

    command.commandType = START_PLAY;
    c->potpos = potpos;
    c->row = row;
    c->loop = loop;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandStopPlay(void) {

    NTXMFifoMessage command;
    command.commandType = STOP_PLAY;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandPlayInst(u8 inst, u8 note, u8 volume, u8 channel)
{
    NTXMFifoMessage command;
    command.commandType = PLAY_INST;

    PlayInstCommand* c = &command.playInst;

    c->inst    = inst;
    c->note    = note;
    c->volume  = volume;
    c->channel = channel;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandStopInst(u8 channel)
{
    NTXMFifoMessage command;
    command.commandType = STOP_INST;

    StopInstCommand* c = &command.stopInst;

    c->channel = channel;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandStopMatchingInst(u8 inst, u8 note)
{
    NTXMFifoMessage command;
    command.commandType = STOP_MATCHING_INST;

    StopMatchingInstCommand* c = &command.stopMatchingInst;

    c->note = note;
    c->inst = inst;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandPlayNoteAuto(u8 inst, u8 note, u8 volume, u16 tag)
{
    NTXMFifoMessage command;
    command.commandType = PLAY_NOTE_AUTO;

    PlayNoteAutoCommand* c = &command.playNoteAuto;

    c->inst    = inst;
    c->note    = note;
    c->volume  = volume;
    c->tag  = tag;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandStopNoteAuto(u16 tag)
{
    NTXMFifoMessage command;
    command.commandType = STOP_NOTE_AUTO;

    StopNoteAutoCommand* c = &command.stopNoteAuto;

    c->tag = tag;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandMicOn(void)
{
    NTXMFifoMessage command;
    command.commandType = MIC_ON;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandMicOff(void)
{
    NTXMFifoMessage command;
    command.commandType = MIC_OFF;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandOnSongSpeedChanged(void)
{
    NTXMFifoMessage command;
    command.commandType = ON_SONG_SPEED_CHANGED;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandSetPatternLoop(bool state)
{
    NTXMFifoMessage command;
    command.commandType = PATTERN_LOOP;

    PatternLoopCommand* c = &command.ptnLoop;
    c->state = state;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandSetStereoOutput(bool state)
{
    NTXMFifoMessage command;
    command.commandType = SET_STEREO_OUTPUT;

    SetStereoOutputCommand* c = &command.setStereoOutput;
    c->state = state;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}
