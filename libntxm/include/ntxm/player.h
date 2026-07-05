/*
 * libNTXM - XM Player Library for the Nintendo DS
 */

#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "instrument.h"
#include "song.h"

typedef struct {
    int16_t songPos, pattNr, pattPos, pattLen;
    uint16_t speed, globVol, timer;
    uint8_t pattDelTime, pattDelTime2, pBreakPos;
    bool pBreakFlag, posJumpFlag;
} PlayerState;

typedef struct stmTyp_t
{
    int32_t smpStartPos;
	Instrument *instrSeg;

	int16_t envVIPValue, envPIPValue;
	uint16_t outPeriod, realPeriod, finalPeriod, finalVol, tonTyp, wantPeriod, portaSpeed;
	uint16_t envVCnt, envVAmp, envPCnt, envPAmp, eVibAmp, eVibSweep;
	uint16_t fadeOutAmp, fadeOutSpeed;

	volatile uint8_t status;
	int8_t relTonNr, fineTune;
	uint8_t sampleNr, instrNr, effTyp, eff, smpOffset, tremorSave, tremorPos;
	uint8_t globVolSlideSpeed, panningSlideSpeed, mute, waveCtrl, portaDir;
	uint8_t glissFunk, vibPos, tremPos, vibSpeed, vibDepth, tremSpeed, tremDepth;
	uint8_t pattPos, loopCnt, volSlideSpeed, fVolSlideUpSpeed, fVolSlideDownSpeed;
	uint8_t fPortaUpSpeed, fPortaDownSpeed, ePortaUpSpeed, ePortaDownSpeed;
	uint8_t portaUpSpeed, portaDownSpeed, retrigSpeed, retrigCnt, retrigVol;
	uint8_t volKolVol, tonNr, envPPos, eVibPos, envVPos, realVol, oldVol, outVol;
	uint8_t oldPan, outPan, finalPan;
	bool envSustainActive;

	uint8_t ntxmTag;
	uint16_t ntxmCurVol; ///< Interpolates between start and end as needed
	uint16_t ntxmStartVol; ///< Previous volume set to the audio channel (before fading began)
	uint16_t ntxmEndVol; ///< Latest volume set to the audio channel, or sometimes zero in the case of ramping before a new note.
	uint16_t ntxmRampTimer;
	uint16_t ntxmRampDuration;
	bool ntxmEarlyRamp;  ///< Indicates that this channel is currently ramping out due to a new upcoming note on the next row.
} stmTyp;

class Player {
public:
    Player(void (*_playTimerListener)(void) = nullptr);

    void play(int potpos, int row, bool repeat);
    void stop();
    void playNote(int note, int volume, int channel, int instidx);
    void stopAllNotes(int note, int instidx);
    void playSample(Sample *sample, int note, int volume, int channel);
    void stopChannel(int channel);
    void playNoteAuto(int instidx, int note, int volume, int tag);
    void stopNoteAuto(int tag);

    u32 getMsPerTick() const;
    inline int getPlayTimerFrequency() const { return 1000; }

    void setPatternLoop(bool repeat);
    void setSong(Song* _song);

#ifdef NT_PLATFORM_NDS
    void playTimerHandler();
#endif
    void update(int msDelta);

private:
    PlayerState state;
    u32 currMs;
    u32 nextPlayerMs;
    u32 nextFadeMs;
    bool playing;
    bool songLoop;
    bool patternLoop;

    Song* song;
    void (*playTimerListener)(void);

    int getChannelForTag(u16 tag);
    void tryEarlyVolumeRamps(void);

    // ft2play routines
    uint8_t PMPTmpActiveChannel;
    Sample *PMPSampleOverride;
    bool PMPIgnoreMute;

    stmTyp stm[MAX_CHANNELS];

    void startSongChannel(int c, stmTyp *ch, Sample *s, int smpOffset);
    void setPos(int32_t pos, int32_t row);
    void resetVoice(stmTyp *ch);
    void stopVoices(void);

    uint16_t note2Period(uint16_t note);
    void retrigVolume(stmTyp *ch);
    void retrigEnvelopeVibrato(stmTyp *ch);
    void keyOff(stmTyp *ch);
    void startTone(uint8_t ton, uint8_t effTyp, uint8_t eff, stmTyp *ch);
    void finePortaUp(stmTyp *ch, uint8_t param);
    void finePortaDown(stmTyp *ch, uint8_t param);
    void setGlissCtrl(stmTyp *ch, uint8_t param);
    void setVibratoCtrl(stmTyp *ch, uint8_t param);
    void jumpLoop(stmTyp *ch, uint8_t param);
    void setTremoloCtrl(stmTyp *ch, uint8_t param);
    void volFineUp(stmTyp *ch, uint8_t param);
    void volFineDown(stmTyp *ch, uint8_t param);
    void noteCut0(stmTyp *ch, uint8_t param);
    void pattDelay(stmTyp *ch, uint8_t param);
    void E_Effects_TickZero(stmTyp *ch, uint8_t param);
    void posJump(stmTyp *ch, uint8_t param);
    void pattBreak(stmTyp *ch, uint8_t param);
    void setSpeed(stmTyp *ch, uint8_t param);
    void setGlobaVol(stmTyp *ch, uint8_t param);
    void setEnvelopePos(stmTyp *ch, uint8_t param);
    void v_SetVibSpeed(stmTyp *ch, uint8_t *volKol);
    void v_Volume(stmTyp *ch, uint8_t *volKol);
    void v_FineSlideDown(stmTyp *ch, uint8_t *volKol);
    void v_FineSlideUp(stmTyp *ch, uint8_t *volKol);
    void v_SetPan(stmTyp *ch, uint8_t *volKol);
    void v_SlideDown(stmTyp *ch);
    void v_SlideUp(stmTyp *ch);
    void v_Vibrato(stmTyp *ch);
    void v_PanSlideLeft(stmTyp *ch);
    void v_PanSlideRight(stmTyp *ch);
    void v_TonePorta(stmTyp *ch);
    void VJumpTab_TickNonZero(uint8_t efx, stmTyp *ch);
    void VJumpTab_TickZero(uint8_t efx, stmTyp *ch, uint8_t *volKol);
    void setPan(stmTyp *ch, uint8_t param);
    void setVol(stmTyp *ch, uint8_t param);
    void xFinePorta(stmTyp *ch, uint8_t param);
    void doMultiRetrig(stmTyp *ch, uint8_t param);
    void multiRetrig(stmTyp *ch, uint8_t param, uint8_t volumeColumnData);
    void JumpTab_TickZero(stmTyp *ch, uint8_t effTyp, uint8_t eff);
    void checkEffects(stmTyp *ch);
    void fixTonePorta(stmTyp *ch, const Cell *p, uint8_t inst);
    void getNewNote(stmTyp *ch, const Cell *p);
    void fixaEnvelopeVibrato(stmTyp *ch);
    uint16_t relocateTon(uint16_t period, uint8_t arpNote, stmTyp *ch);
    void tonePorta(stmTyp *ch, uint8_t param);
    void vibrato2(stmTyp *ch);
    void arp(stmTyp *ch, uint8_t param);
    void portaUp(stmTyp *ch, uint8_t param);
    void portaDown(stmTyp *ch, uint8_t param);
    void vibrato(stmTyp *ch, uint8_t param);
    void tonePlusVol(stmTyp *ch, uint8_t param);
    void vibratoPlusVol(stmTyp *ch, uint8_t param);
    void tremolo(stmTyp *ch, uint8_t param);
    void volume(stmTyp *ch, uint8_t param);
    void globalVolSlide(stmTyp *ch, uint8_t param);
    void keyOffCmd(stmTyp *ch, uint8_t param);
    void panningSlide(stmTyp *ch, uint8_t param);
    void tremor(stmTyp *ch, uint8_t param);
    void retrigNote(stmTyp *ch, uint8_t param);
    void noteCut(stmTyp *ch, uint8_t param);
    void noteDelay(stmTyp *ch, uint8_t param);
    void E_Effects_TickNonZero(stmTyp *ch, uint8_t param);
    void JumpTab_TickNonZero(stmTyp *ch, uint8_t effTyp, uint8_t eff);
    void doEffects(stmTyp *ch);
    void getNextPos(void);
    void mainPlayer(void);
};

#endif
