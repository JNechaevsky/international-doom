//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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


#pragma once


typedef struct
{
    int64_t x,y;
} mpoint_t;

extern int followplayer;
extern int ravmap_cheating;

extern mpoint_t *markpoints; 
extern int markpointnum;
extern int markpointnum_max;

extern vertex_t KeyPoints[];
extern const char *LevelNames[];

extern void AM_ClearMarks (void);
extern void AM_Init (void);
extern void AM_initOverlayMode (void);
extern void AM_initVariables (void);
extern void AM_LevelInit (boolean reinit);
extern void AM_LevelNameDrawer (void);
extern void AM_Start (void);
extern void AM_Stop (void);


//
// Automap colors:
//

// Common walls
#define WALLCOLORS      96
#define FDWALLCOLORS    112
#define CDWALLCOLORS    80
#define SECRETCOLORS    175
#define FSECRETCOLORS   206

// Hidden lines
#define MLDONTDRAW1     40
#define MLDONTDRAW2     43

// Locked doors and keys
#define YELLOWKEY       144
#define GREENKEY        220
#define BLUEKEY         197

// Teleporters
#define TELEPORTERS     156

// Exits
#define EXITS           182

// Players (no antialiasing)
#define PL_WHITE        32
#define PL_GREEN        221
#define PL_YELLOW       241
#define PL_RED          160
#define PL_BLUE         198

// Grid (no antialiasing)
#define GRIDCOLORS      39

// Crosshair
#define XHAIRCOLORS	    28

// IDDT triangles
#define IDDT_GREEN      222
#define IDDT_YELLOW     140
#define IDDT_RED        150
#define IDDT_GRAY       9
