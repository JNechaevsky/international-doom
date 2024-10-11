//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
//	Refresh, visplane stuff (floor, ceilings).
//


#ifndef __R_PLANE__
#define __R_PLANE__


#include "r_data.h"



// Visplane related.
extern size_t  maxopenings;         // [JN] 32-bit integer maths
extern int    *lastopening;
extern int    *openings;


typedef void (*planefunction_t) (int top, int bottom);

extern planefunction_t	floorfunc;
extern planefunction_t	ceilingfunc_t;

extern int		floorclip[MAXWIDTH]; // [crispy] 32-bit integer math
extern int		ceilingclip[MAXWIDTH]; // [crispy] 32-bit integer math

extern fixed_t*	yslope;
extern fixed_t		yslopes[LOOKDIRS][MAXHEIGHT]; // [crispy] lookup table
extern fixed_t		distscale[MAXWIDTH];

void R_InitPlanes (void);
void R_ClearPlanes (void);

void R_DrawPlanes (void);

visplane_t*
R_FindPlane
( fixed_t	height,
  int		picnum,
  int		lightlevel );

visplane_t*
R_CheckPlane
( visplane_t*	pl,
  int		start,
  int		stop );
  
extern visplane_t *R_DupPlane (const visplane_t *pl, int start, int stop);



#endif
