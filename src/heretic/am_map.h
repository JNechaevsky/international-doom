//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2024 Julia Nechaevskaya
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
#ifndef __AMMAP_H__
#define __AMMAP_H__

// For use if I do walls with outsides/insides
#define REDS		12*8
#define REDRANGE	1       //16
#define BLUES		(256-4*16+8)
#define BLUERANGE	1       //8
#define GREENS		(33*8)
#define GREENRANGE	1       //16
#define GRAYS		(5*8)
#define GRAYSRANGE	1       //16
#define BROWNS		(14*8)
#define BROWNRANGE	1       //16
#define YELLOWS		10*8
#define YELLOWRANGE	1
#define BLACK		0
#define WHITE		4*8
#define PARCH		13*8-1
#define BLOODRED  150

// Automap colors
#define BACKGROUND	PARCH
#define YOURCOLORS	WHITE
#define YOURRANGE	0
#define WALLRANGE	REDRANGE
#define THINGCOLORS	GREENS
#define THINGRANGE	GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS	39// (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE	0






// Common walls
#define WALLCOLORS      96
#define FDWALLCOLORS    112
#define CDWALLCOLORS    80
#define SECRETCOLORS    175

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

// IDDT triangles
#define IDDT_GREEN      222
#define IDDT_YELLOW     140

// Crosshair
#define XHAIRCOLORS	    28

typedef struct
{
    int64_t x,y;
} mpoint_t;

extern int ravmap_cheating;

extern mpoint_t *markpoints; 
extern int markpointnum;
extern int markpointnum_max;

extern vertex_t KeyPoints[];
extern const char *LevelNames[];

void AM_Stop(void);

extern void AM_Init (void);
extern void AM_Start (void);
extern void AM_LevelInit (boolean reinit);
extern void AM_LevelNameDrawer (void);
extern void AM_clearMarks (void);

#endif
