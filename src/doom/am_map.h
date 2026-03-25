//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2026 Julia Nechaevskaya
// Copyright(C) 2024-2026 Polina "Aura" N.
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
#include "m_fixed.h" // fixed_t
#include "tables.h"  // angle_t


typedef struct
{
    int64_t x,y;
} mpoint_t;

extern int64_t m_x, m_y;

extern mpoint_t *markpoints;
extern int markpointnum, markpointnum_max;

extern int am_followplayer;
extern int iddt_cheating;
extern int am_grid;
extern int am_oids;

// Used by ST StatusBar stuff.
#define AM_MSGHEADER (('a'<<24)+('m'<<16))
#define AM_MSGENTERED (AM_MSGHEADER | ('e'<<8))
#define AM_MSGEXITED (AM_MSGHEADER | ('x'<<8))


extern void AM_Init (void);
extern void AM_LevelInit (boolean reinit);

extern fixed_t AM_UnArchiveScaleMtof (void);
extern void AM_ArchiveScaleMtof (fixed_t scale);
extern angle_t mapangle;

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
void AM_SetMapCenter (fixed_t x, fixed_t y);

// [JN] Make global, since mark preserved in saved games.
void AM_clearMarks (void);

extern cheatseq_t cheat_amap;

extern void AM_MiniDrawer (void);


#endif
