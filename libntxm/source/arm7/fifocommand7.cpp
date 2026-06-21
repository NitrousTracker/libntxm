/*
 * fifocommand7.cpp
 *
 *  Created on: Mar 28, 2010
 *      Author: tob
 */

#include <nds.h>
#include <stdarg.h>

#include "ntxm/fifocommand.h"
#include "ntxm/linear_freq_table.h"

#include "ntxm/player.h"
#include "ntxm/ntxm7.h"

extern NTXM7 *ntxm7;
bool ntxm_recording = false;
bool ntxm_mic_recording = false;
bool ntxm_stereo_output = false;
int ntxm_record_buffer_size = 0;
int ntxm_record_max_buffer_size = 0;

static void MicBufSwapCallback(u8 *completedBuffer, int length) {
    if (!ntxm_mic_recording)
        return;

    if (length > 0)
    {
        ntxm_record_buffer_size += length;
        if (ntxm_record_buffer_size >= ntxm_record_max_buffer_size)
        {
            ntxm_mic_recording = false;
            micStopRecording();
        }
    }
}

static void RecvCommandPlaySample(PlaySampleCommand *ps)
{
    ntxm7->playSample(ps->sample, ps->note, ps->volume, ps->channel);
}

static void RecvCommandStopSample(StopSampleSoundCommand* ss) {
    ntxm7->stopChannel(ss->channel);
}

static void RecvCommandMicOn(void)
{
    micOn();
}

static void RecvCommandMicOff(void)
{
    micOff();
}

static void RecvCommandStartRecording(StartRecordingCommand* sr)
{
    ntxm_mic_recording = true;
    ntxm_recording = true;
    ntxm_record_buffer_size = 0;
    ntxm_record_max_buffer_size = sr->length;
    micStartRecording((u8*) sr->buffer, sr->length, 16384, 1, false, MicBufSwapCallback);
}

static void RecvCommandStopRecording()
{
    if (ntxm_mic_recording)
        micStopRecording(); // buffer size in samples
    fifoSendValue32(FIFO_NTXM, ntxm_record_buffer_size);
    ntxm_mic_recording = false;
    ntxm_recording = false;
}

static void RecvCommandSetSong(SetSongCommand *c) {
    ntxm7->setSong((Song*)c->ptr);
}

static void RecvCommandStartPlay(StartPlayCommand *c) {
    ntxm7->play(c->loop, c->potpos, c->row);
}

static void RecvCommandStopPlay(StopPlayCommand *c) {
    ntxm7->stop();
}

static void RecvCommandPlayInst(PlayInstCommand *c) {
    ntxm7->playNote(c->inst, c->note, c->volume, c->channel);
}

static void RecvCommandStopInst(StopInstCommand *c) {
    ntxm7->stopChannel(c->channel);
}

static void RecvCommandPlayNoteAuto(PlayNoteAutoCommand *c) {
    ntxm7->playNoteAuto(c->inst, c->note, c->volume, c->tag);
}

static void RecvCommandStopNoteAuto(StopNoteAutoCommand *c) {
    ntxm7->stopNoteAuto(c->tag);
}

static void RecvCommandStopMatchingInst(StopMatchingInstCommand *c) {
    ntxm7->stopAllNotes(c->note, c->inst);
}

static void RecvCommandPatternLoop(PatternLoopCommand *c) {
    ntxm7->setPatternLoop(c->state);
}

static void RecvCommandSetStereoOutput(SetStereoOutputCommand *c) {
    ntxm_stereo_output = c->state;
}

#ifdef DEBUG
void CommandDbgOut(const char *formatstr, ...)
{
    NTXMFifoMessage command;
    command.commandType = DBG_OUT;

    DbgOutCommand *cmd = &command.dbgOut;

    va_list marker;
    va_start(marker, formatstr);

    char *debugstr = cmd->msg;
#ifdef BLOCKSDS
    vsnprintf(debugstr, DEBUGSTRSIZE-1, formatstr, marker);
#else
    vsniprintf(debugstr, DEBUGSTRSIZE-1, formatstr, marker);
#endif
    debugstr[DEBUGSTRSIZE-1] = 0;

    va_end(marker);

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}
#endif

void CommandUpdateRow(u16 row)
{
    NTXMFifoMessage command;
    command.commandType = UPDATE_ROW;

    UpdateRowCommand *c = &command.updateRow;
    c->row = row;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandUpdatePotPos(u16 potpos)
{
    NTXMFifoMessage command;
    command.commandType = UPDATE_POTPOS;

    UpdatePotPosCommand *c = &command.updatePotPos;
    c->potpos = potpos;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandNotifyStop(void)
{
    NTXMFifoMessage command;
    command.commandType = NOTIFY_STOP;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandSampleFinish(void)
{
    NTXMFifoMessage command;
    command.commandType = SAMPLE_FINISH;

    fifoSendDatamsg(FIFO_NTXM, sizeof(command), (u8*)&command);
}

void CommandRecvHandler(int bytes, void *user_data) {
    NTXMFifoMessage command;

    fifoGetDatamsg(FIFO_NTXM, bytes, (u8*)&command);

    switch(command.commandType) {
        case PLAY_SAMPLE:
            RecvCommandPlaySample(&command.playSample);
            break;
        case STOP_SAMPLE:
            RecvCommandStopSample(&command.stopSample);
            break;
        case START_RECORDING:
            RecvCommandStartRecording(&command.startRecording);
            break;
        case STOP_RECORDING:
            RecvCommandStopRecording();
            break;
        case SET_SONG:
            RecvCommandSetSong(&command.setSong);
            break;
        case START_PLAY:
            RecvCommandStartPlay(&command.startPlay);
            break;
        case STOP_PLAY:
            RecvCommandStopPlay(&command.stopPlay);
            break;
        case PLAY_INST:
            RecvCommandPlayInst(&command.playInst);
            break;
        case STOP_INST:
            RecvCommandStopInst(&command.stopInst);
            break;
        case STOP_MATCHING_INST:
            RecvCommandStopMatchingInst(&command.stopMatchingInst);
            break;
        case PLAY_NOTE_AUTO:
            RecvCommandPlayNoteAuto(&command.playNoteAuto);
            break;
        case STOP_NOTE_AUTO:
            RecvCommandStopNoteAuto(&command.stopNoteAuto);
            break;
        case MIC_ON:
            RecvCommandMicOn();
            break;
        case MIC_OFF:
            RecvCommandMicOff();
            break;
        case PATTERN_LOOP:
            RecvCommandPatternLoop(&command.ptnLoop);
            break;
        case SET_STEREO_OUTPUT:
            RecvCommandSetStereoOutput(&command.setStereoOutput);
            break;
        default:
            break;
    }
}

bool CommandInit(void)
{
    fifoSetDatamsgHandler(FIFO_NTXM, CommandRecvHandler, 0);
    //fifoSetValue32Handler(FIFO_NTXM, CommandRecvHandler, 0);
}

void CommandExit(void)
{

}
