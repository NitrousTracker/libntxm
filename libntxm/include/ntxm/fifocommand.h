/*
 * fifocommand.h
 *
 *  Created on: Mar 27, 2010
 *      Author: tob
 */

#ifndef FIFOCOMMAND_H_
#define FIFOCOMMAND_H_

#include <stdbool.h>
#include <stdio.h>
#include "ntxm/sample.h"

#define FIFO_NTXM FIFO_USER_01
#define DEBUGSTRSIZE 40

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
    SET_STEREO_OUTPUT
} NTXMFifoMessageType;

struct PlaySampleCommand
{
    Sample *sample;
    u8 note;
    u8 volume;
    u8 channel;
};

/* Command parameters for stopping a sample */
struct StopSampleSoundCommand
{
    u8 channel;
};

/* Command parameters for starting to record from the microphone */
struct StartRecordingCommand
{
    u16* buffer;
    int length;
};

struct SetSongCommand {
    void *ptr;
};

struct StartPlayCommand {
    u16 row;
    u8 potpos;
    bool loop;
};

struct StopPlayCommand {
};

#ifdef DEBUG
struct DbgOutCommand {
    char msg[DEBUGSTRSIZE];
};
#endif

struct UpdateRowCommand {
    u16 row;
};

struct UpdatePotPosCommand {
    u16 potpos;
};

struct PlayInstCommand {
    u8 inst;
    u8 note;
    u8 volume;
    u8 channel;
};

struct StopInstCommand {
    u8 channel;
};

struct StopMatchingInstCommand {
    u8 inst;
    u8 note;
};

struct PlayNoteAutoCommand {
    u8 inst;
    u8 note;
    u8 volume;
    u16 tag;
};

struct StopNoteAutoCommand {
    u16 tag;
};

struct PatternLoopCommand {
    bool state;
};

struct SetStereoOutputCommand {
    bool state;
};

typedef struct NTXMFifoMessage {
    u16 commandType;

    union {
        void *data;
        PlaySampleCommand      playSample;
        StopSampleSoundCommand stopSample;
        StartRecordingCommand  startRecording;
        SetSongCommand         setSong;
        StartPlayCommand       startPlay;
        StopPlayCommand        stopPlay;
#ifdef DEBUG
        DbgOutCommand          dbgOut;
#endif
        UpdateRowCommand       updateRow;
        UpdatePotPosCommand    updatePotPos;
        PlayInstCommand        playInst;
        StopInstCommand        stopInst;
        StopMatchingInstCommand    stopMatchingInst;
        PlayNoteAutoCommand    playNoteAuto;
        StopNoteAutoCommand    stopNoteAuto;
        PatternLoopCommand     ptnLoop;
        SetStereoOutputCommand setStereoOutput;
    };
} NTXMFifoMessage;

bool CommandInit();
void CommandExit();

#if !defined(NT_PLATFORM_NDS) || defined(ARM9)
void CommandPlayOneShotSample(int channel, int frequency, const void* data, int length, int volume, int format, bool loop);
void CommandPlaySample(Sample *sample, u8 note, u8 volume, u8 channel);
void CommandPlaySample(Sample *sample);
void CommandStopSample(int channel);
void CommandStartRecording(u16* buffer, int length);
int CommandStopRecording(void);
void CommandSetSong(void *song);
void CommandStartPlay(u8 potpos, u16 row, bool loop);
void CommandStopPlay(void);
void CommandSetDebugStrPtr(char **arm7debugstrs, u16 debugstrsize, u8 n_debugbufs);
void CommandPlayInst(u8 inst, u8 note, u8 volume, u8 channel);
void CommandStopInst(u8 channel);
void CommandStopMatchingInst(u8 inst, u8 note);
void CommandPlayNoteAuto(u8 inst, u8 note, u8 volume, u16 tag);
void CommandStopNoteAuto(u16 tag);
void CommandMicOn(void);
void CommandMicOff(void);
void CommandSetPatternLoop(bool state);
void CommandSetStereoOutput(bool state);

void RegisterRowCallback(void (*onUpdateRow_)(u16));
void RegisterStopCallback(void (*onStop_)(void));
void RegisterPlaySampleFinishedCallback(void (*onPlaySampleFinished_)(void));
void RegisterPotPosChangeCallback(void (*onPotPosChange_)(u16));
#endif

#if !defined(NT_PLATFORM_NDS) || defined(ARM7)
#ifdef DEBUG
void CommandDbgOut(const char *formatstr, ...); // Print text from the ARM7, syntax like printf
#else
#define CommandDbgOut(...)
#endif
void CommandUpdateRow(u16 row);
void CommandUpdatePotPos(u16 potpos);
void CommandNotifyStop(void);
void CommandSampleFinish(void);
#endif

#endif /* FIFOCOMMAND_H_ */
