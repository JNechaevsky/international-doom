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


#pragma once


typedef struct
{
    int64_t x,y;
} mpoint_t;


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
#define BLOODRED  	177
#define BLUEKEY 	157
#define YELLOWKEY 	137
#define GREENKEY  	198

// Automap colors

#define AM_PLR1_COLOR 157       // Blue
#define AM_PLR2_COLOR 177       // Red
#define AM_PLR3_COLOR 137       // Yellow
#define AM_PLR4_COLOR 198       // Green
#define AM_PLR5_COLOR 215       // Jade
#define AM_PLR6_COLOR 32        // White
#define AM_PLR7_COLOR 106       // Hazel
#define AM_PLR8_COLOR 234       // Purple

#define BACKGROUND	PARCH
#define YOURCOLORS	WHITE
#define YOURRANGE	0
#define WALLCOLORS	REDS
#define WALLRANGE	REDRANGE
#define TSWALLCOLORS	GRAYS
#define TSWALLRANGE	GRAYSRANGE
#define FDWALLCOLORS	BROWNS
#define FDWALLRANGE	BROWNRANGE
#define CDWALLCOLORS	YELLOWS
#define CDWALLRANGE	YELLOWRANGE
#define THINGCOLORS	GREENS
#define THINGRANGE	GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDRANGE	0

// drawing stuff

#define AM_NUMMARKPOINTS 10

#define AM_MSGHEADER (('a'<<24)+('m'<<16))
#define AM_MSGENTERED (AM_MSGHEADER | ('e'<<8))
#define AM_MSGEXITED (AM_MSGHEADER | ('x'<<8))


// the following is crap
#define LINE_NEVERSEE ML_DONTDRAW





extern int cheating;

extern mpoint_t *markpoints; 
extern int markpointnum;
extern int markpointnum_max;

extern void AM_ClearMarks (void);
extern void AM_Init (void);
extern void AM_LevelInit (boolean reinit);
extern void AM_LevelNameDrawer (void);
extern void AM_Start (void);
extern void AM_Stop (void);


//
// Automap colors:
//

// Players (no antialiasing)
#define PL_WHITE        32
#define PL_GREEN        221
#define PL_YELLOW       145
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