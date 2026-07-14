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

#if defined(NT_PLATFORM_NDS)

u16 lastTime;

void demoInit(void)
{
	// 33513982 ticks per second
	// => ~33514 ticks per millisecond
	TIMER0_DATA=(u16) (~33514);
	TIMER1_DATA=0;
	TIMER0_CR=TIMER_DIV_1 | TIMER_IRQ_REQ | TIMER_ENABLE;
	TIMER1_CR=TIMER_CASCADE | TIMER_ENABLE;
	lastTime = 0;
}

u32 getMsDelta(void)
{
	u16 currTime = TIMER1_DATA;
	s32 delta = (s32)currTime - (s32)lastTime;
	lastTime = currTime;
	if (delta < 0) {
		delta += 65536;
	}
	return delta;
}

#endif
