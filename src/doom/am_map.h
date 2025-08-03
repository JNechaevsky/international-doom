//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2025 Julia Nechaevskaya
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
// DESCRIPTION:
//  AutoMap module.
//

#ifndef __AMMAP_H__
#define __AMMAP_H__

#include "d_event.h"
#include "m_cheat.h"


typedef struct
{
    int64_t x,y;
} mpoint_t;

extern mpoint_t *markpoints;
extern int markpointnum, markpointnum_max;

extern int followplayer;
extern int iddt_cheating;

// Used by ST StatusBar stuff.
#define AM_MSGHEADER (('a'<<24)+('m'<<16))
#define AM_MSGENTERED (AM_MSGHEADER | ('e'<<8))
#define AM_MSGEXITED (AM_MSGHEADER | ('x'<<8))


extern void AM_Init (void);
extern void AM_LevelInit (boolean reinit);

extern void AM_SetdrawFline (void);

// Called by main loop.
boolean AM_Responder (const event_t *ev);
void AM_initVariables (void);

// Called by main loop.
void AM_Ticker (void);

// Called by main loop,
// called instead of view drawer if automap active.
void AM_Drawer (void);
extern void AM_LevelNameDrawer (void);

// Called to force the automap to quit
// if the level is completed while it is up.
void AM_Start (void);
void AM_Stop (void);

// [JN] Make global, since mark preserved in saved games.
void AM_clearMarks (void);

extern cheatseq_t cheat_amap;


#endif
