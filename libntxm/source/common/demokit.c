/*
 * libNTXM - XM Player Library for the Nintendo DS
 *
 *    Copyright (C) 2005-2008 Tobias Weyand (0xtob)
 *                         me@nitrotracker.tobw.net
 *
 */

/***** BEGIN LICENSE BLOCK *****
 *
 * Version: Noncommercial zLib License / GPL 3.0
 *
 * The contents of this file are subject to the Noncommercial zLib License
 * (the "License"); you may not use this file except in compliance with
 * the License. You should have recieved a copy of the license with this package.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 3 or later (the "GPL"),
 * in which case the provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only under the terms of
 * either the GPL, and not to allow others to use your version of this file under
 * the terms of the Noncommercial zLib License, indicate your decision by
 * deleting the provisions above and replace them with the notice and other
 * provisions required by the GPL. If you do not delete the provisions above,
 * a recipient may use your version of this file under the terms of any one of
 * the GPL or the Noncommercial zLib License.
 *
 ***** END LICENSE BLOCK *****/

#include "ntxm/demokit.h"
#ifdef NT_PLATFORM_SDL3
#include <SDL3/SDL.h>
#endif

inline unsigned int nds_read_timers(unsigned int tlow, unsigned int thigh) {
	return tlow | (thigh<<16);
}

int ticksSpeed;
unsigned int lastTime;

void demoInit(void)
{
	reStartRealTicks();
	reStartTicks();
}

void reStartRealTicks(void)
{
#if defined(NT_PLATFORM_NDS)
	TIMER2_DATA=0;
	TIMER3_DATA=0;
	TIMER2_CR=TIMER_DIV_64 | TIMER_ENABLE;
	TIMER3_CR=TIMER_CASCADE | TIMER_ENABLE;
#endif
}

// NOTE: each of these values can overflow, but the arithmetic works out, as
//       long as we only divide the delta rather than the absolute time.
unsigned int getRealTicks(void)
{
#if defined(NT_PLATFORM_NDS)
	return nds_read_timers(TIMER2_DATA, TIMER3_DATA);
#elif defined(NT_PLATFORM_3DS)
	return svcGetSystemTick() << MS_PRECISION;
#elif defined(NT_PLATFORM_SDL3)
	return SDL_GetTicksNS() << MS_PRECISION;
#else
#error "Unimplemented getRealTicks() for platform!"
#endif
}

void reStartTicks(void)
{
	ticksSpeed = 100;
	lastTime = getRealTicks();
}

unsigned int getMsDelta(void)
{
	unsigned int t = getRealTicks();
	unsigned int dt = ((t - lastTime)*ticksSpeed)/100;
	lastTime = t;
#if defined(NT_PLATFORM_NDS)
	return (dt * 125) >> 7;   // same as * 1000 / 1024
#elif defined(NT_PLATFORM_3DS)
	return dt / CPU_TICKS_PER_MSEC;
#elif defined(NT_PLATFORM_SDL3)
	return dt / 1000000;
#else
#error "Unimplemented getMsDelta() for platform!"
#endif
}

void setTicksSpeed(int percentage)
{
	ticksSpeed = percentage;
}

int getTicksSpeed(void)
{
	return ticksSpeed;
}
