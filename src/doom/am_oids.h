//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2026 Polina "Aura" N.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// DESCRIPTION:  Asteroids-inspired mini-game inside Doom automap.
//


#ifndef __AMOIDS__
#define __AMOIDS__

#include "d_event.h"
#include "doomtype.h"
#include "doomstat.h"

// [PN] Asteroids mini-game state machine for Doom automap.
void AM_OidsSetBounds(int x, int y, int w, int h);
void AM_OidsStart(void);
void AM_OidsStop(void);
boolean AM_OidsActive(void);
boolean AM_OidsResponder(const event_t *ev);
void AM_OidsTicker(void);
void AM_OidsDrawer(void);

#endif
