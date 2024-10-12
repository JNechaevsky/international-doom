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
//	Rendering main loop and setup functions,
//	 utility functions (BSP, geometry, trigonometry).
//	See tables.c, too.
//





#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>


#include "doomdef.h"
#include "doomstat.h"   // villsa [STRIFE]
#include "d_main.h"

#include "m_bbox.h"
#include "m_menu.h"

#include "r_local.h"
#include "r_sky.h"
#include "st_stuff.h" // [crispy]
#include "f_wipe.h"





// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW		2048	



int			viewangleoffset;

// increment every time a check is made
int			validcount = 1;		


lighttable_t*		fixedcolormap;


int			centerx;
int			centery;

fixed_t			centerxfrac;
fixed_t			centeryfrac;
fixed_t			projection;

// just for profiling purposes
int			framecount;	

int			sscount;
int			linecount;
int			loopcount;

fixed_t			viewx;
fixed_t			viewy;
fixed_t			viewz;

int                     viewpitch;  // villsa [STRIFE]

angle_t			viewangle;
localview_t		localview; // [crispy]

fixed_t			viewcos;
fixed_t			viewsin;

player_t*		viewplayer;

// 0 = high, 1 = low
int			detailshift;	

//
// precalculated math tables
//
angle_t			clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X. 
int			viewangletox[FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t			xtoviewangle[MAXWIDTH+1];

// [crispy] calculate the linear sky angle component here
angle_t			linearskyangle[MAXWIDTH+1];

// [crispy] parameterized for smooth diminishing lighting
lighttable_t***		scalelight = NULL;
lighttable_t**		scalelightfixed = NULL;
lighttable_t***		zlight = NULL;

// bumped light from gun blasts
int			extralight;			

// [JN] FOV from DOOM Retro, Woof! and Nugget Doom
static fixed_t fovscale;	
float  fovdiff;   // [Nugget] Used for some corrections
int    max_project_slope = 4;  // [Woof!]

// [crispy] parameterized for smooth diminishing lighting
int NUMCOLORMAPS;
int LIGHTLEVELS;
int LIGHTSEGSHIFT;
int LIGHTBRIGHT;
int MAXLIGHTSCALE;
int LIGHTSCALESHIFT;
int MAXLIGHTZ;
int LIGHTZSHIFT;


void (*colfunc) (void);
void (*basecolfunc) (void);
void (*fuzzcolfunc) (void);
void (*transcolfunc) (void);
void (*spanfunc) (void);



//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
void
R_AddPointToBox
( int		x,
  int		y,
  fixed_t*	box )
{
    if (x< box[BOXLEFT])
	box[BOXLEFT] = x;
    if (x> box[BOXRIGHT])
	box[BOXRIGHT] = x;
    if (y< box[BOXBOTTOM])
	box[BOXBOTTOM] = y;
    if (y> box[BOXTOP])
	box[BOXTOP] = y;
}


// -----------------------------------------------------------------------------
// R_PointOnSide
// Traverse BSP (sub) tree, check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// [JN] killough 5/2/98: reformatted
// -----------------------------------------------------------------------------

int R_PointOnSide (fixed_t x, fixed_t y, const node_t *node)
{
    if (!node->dx)
    {
        return x <= node->x ? node->dy > 0 : node->dy < 0;
    }

    if (!node->dy)
    {
        return y <= node->y ? node->dx < 0 : node->dx > 0;
    }

    x -= node->x;
    y -= node->y;

    // Try to quickly decide by looking at sign bits.
    if ((node->dy ^ node->dx ^ x ^ y) < 0)
    {
        return (node->dy ^ x) < 0;  // (left is negative)
    }

    return FixedMul(y, node->dx>>FRACBITS) >= FixedMul(node->dy>>FRACBITS, x);		
}


int
R_PointOnSegSide
( fixed_t	x,
  fixed_t	y,
  seg_t*	line )
{
    fixed_t	lx;
    fixed_t	ly;
    fixed_t	ldx;
    fixed_t	ldy;
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
    lx = line->v1->x;
    ly = line->v1->y;
	
    ldx = line->v2->x - lx;
    ldy = line->v2->y - ly;
	
    if (!ldx)
    {
	if (x <= lx)
	    return ldy > 0;
	
	return ldy < 0;
    }
    if (!ldy)
    {
	if (y <= ly)
	    return ldx < 0;
	
	return ldx > 0;
    }
	
    dx = (x - lx);
    dy = (y - ly);
	
    // Try to quickly decide by looking at sign bits.
    if ( (ldy ^ ldx ^ dx ^ dy)&0x80000000 )
    {
	if  ( (ldy ^ dx) & 0x80000000 )
	{
	    // (left is negative)
	    return 1;
	}
	return 0;
    }

    left = FixedMul ( ldy>>FRACBITS , dx );
    right = FixedMul ( dy , ldx>>FRACBITS );
	
    if (right < left)
    {
	// front side
	return 0;
    }
    // back side
    return 1;			
}


//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table.

//




// [crispy] turned into a general R_PointToAngle() flavor
// called with either slope_div = SlopeDivCrispy() from R_PointToAngleCrispy()
// or slope_div = SlopeDiv() else
angle_t
R_PointToAngleSlope
( fixed_t	x,
  fixed_t	y,
  int (*slope_div) (unsigned int num, unsigned int den))
{	
    x -= viewx;
    y -= viewy;
    
    if ( (!x) && (!y) )
	return 0;

    if (x>= 0)
    {
	// x >=0
	if (y>= 0)
	{
	    // y>= 0

	    if (x>y)
	    {
		// octant 0
		return tantoangle[slope_div(y,x)];
	    }
	    else
	    {
		// octant 1
		return ANG90-1-tantoangle[slope_div(x,y)];
	    }
	}
	else
	{
	    // y<0
	    y = -y;

	    if (x>y)
	    {
		// octant 8
		return -tantoangle[slope_div(y,x)];
	    }
	    else
	    {
		// octant 7
		return ANG270+tantoangle[slope_div(x,y)];
	    }
	}
    }
    else
    {
	// x<0
	x = -x;

	if (y>= 0)
	{
	    // y>= 0
	    if (x>y)
	    {
		// octant 3
		return ANG180-1-tantoangle[slope_div(y,x)];
	    }
	    else
	    {
		// octant 2
		return ANG90+ tantoangle[slope_div(x,y)];
	    }
	}
	else
	{
	    // y<0
	    y = -y;

	    if (x>y)
	    {
		// octant 4
		return ANG180+tantoangle[slope_div(y,x)];
	    }
	    else
	    {
		 // octant 5
		return ANG270-1-tantoangle[slope_div(x,y)];
	    }
	}
    }
    return 0;
}

angle_t
R_PointToAngle
( fixed_t	x,
  fixed_t	y )
{
    return R_PointToAngleSlope (x, y, SlopeDiv);
}

// [crispy] overflow-safe R_PointToAngle() flavor
// called only from R_CheckBBox(), R_AddLine() and P_SegLengths()
angle_t
R_PointToAngleCrispy
( fixed_t	x,
  fixed_t	y )
{
    // [crispy] fix overflows for very long distances
    int64_t y_viewy = (int64_t)y - viewy;
    int64_t x_viewx = (int64_t)x - viewx;

    // [crispy] the worst that could happen is e.g. INT_MIN-INT_MAX = 2*INT_MIN
    if (x_viewx < INT_MIN || x_viewx > INT_MAX ||
        y_viewy < INT_MIN || y_viewy > INT_MAX)
    {
	// [crispy] preserving the angle by halfing the distance in both directions
	x = x_viewx / 2 + viewx;
	y = y_viewy / 2 + viewy;
    }

    return R_PointToAngleSlope (x, y, SlopeDivCrispy);
}


angle_t
R_PointToAngle2
( fixed_t	x1,
  fixed_t	y1,
  fixed_t	x2,
  fixed_t	y2 )
{	
    viewx = x1;
    viewy = y1;
    
    // [crispy] R_PointToAngle2() is never called during rendering
    return R_PointToAngleSlope (x2, y2, SlopeDiv);
}


fixed_t
R_PointToDist
( fixed_t	x,
  fixed_t	y )
{
    int		angle;
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	temp;
    fixed_t	dist;
    fixed_t     frac;
	
    dx = abs(x - viewx);
    dy = abs(y - viewy);
	
    if (dy>dx)
    {
	temp = dx;
	dx = dy;
	dy = temp;
    }

    // Fix crashes in udm1.wad

    if (dx != 0)
    {
        frac = FixedDiv(dy, dx);
    }
    else
    {
	frac = 0;
    }
	
    angle = (tantoangle[frac>>DBITS]+ANG90) >> ANGLETOFINESHIFT;

    // use as cosine
    dist = FixedDiv (dx, finesine[angle] );	
	
    return dist;
}


// [crispy] WiggleFix: move R_ScaleFromGlobalAngle function to r_segs.c,
// above R_StoreWallRange
#if 0
//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale
//  for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//
fixed_t R_ScaleFromGlobalAngle (angle_t visangle)
{
    fixed_t		scale;
    angle_t		anglea;
    angle_t		angleb;
    int			sinea;
    int			sineb;
    fixed_t		num;
    int			den;

    // UNUSED
#if 0
{
    fixed_t		dist;
    fixed_t		z;
    fixed_t		sinv;
    fixed_t		cosv;
	
    sinv = finesine[(visangle-rw_normalangle)>>ANGLETOFINESHIFT];	
    dist = FixedDiv (rw_distance, sinv);
    cosv = finecosine[(viewangle-visangle)>>ANGLETOFINESHIFT];
    z = abs(FixedMul (dist, cosv));
    scale = FixedDiv(projection, z);
    return scale;
}
#endif

    anglea = ANG90 + (visangle-viewangle);
    angleb = ANG90 + (visangle-rw_normalangle);

    // both sines are allways positive
    sinea = finesine[anglea>>ANGLETOFINESHIFT];	
    sineb = finesine[angleb>>ANGLETOFINESHIFT];
    num = FixedMul(projection,sineb)<<detailshift;
    den = FixedMul(rw_distance,sinea);

    if (den > num>>FRACBITS)
    {
	scale = FixedDiv (num, den);

	if (scale > 64*FRACUNIT)
	    scale = 64*FRACUNIT;
	else if (scale < 256)
	    scale = 256;
    }
    else
	scale = 64*FRACUNIT;
	
    return scale;
}
#endif


// [AM] Interpolate between two angles.
angle_t R_InterpolateAngle(angle_t oangle, angle_t nangle, fixed_t scale)
{
    if (nangle == oangle)
        return nangle;
    else if (nangle > oangle)
    {
        if (nangle - oangle < ANG270)
            return oangle + (angle_t)((nangle - oangle) * FIXED2DOUBLE(scale));
        else // Wrapped around
            return oangle - (angle_t)((oangle - nangle) * FIXED2DOUBLE(scale));
    }
    else // nangle < oangle
    {
        if (oangle - nangle < ANG270)
            return oangle - (angle_t)((oangle - nangle) * FIXED2DOUBLE(scale));
        else // Wrapped around
            return oangle + (angle_t)((nangle - oangle) * FIXED2DOUBLE(scale));
    }
}



// [crispy] in widescreen mode, make sure the same number of horizontal
// pixels shows the same part of the game scene as in regular rendering mode
static int scaledviewwidth_nonwide, viewwidth_nonwide;
static fixed_t centerxfrac_nonwide;

//
// CalcMaxProjectSlope
// [Woof!] Calculate the minimum divider needed to provide at least 45 degrees
// of FOV padding. For fast rejection during sprite/voxel projection.
//

static void CalcMaxProjectSlope (int fov)
{
    max_project_slope = 16;

    for (int i = 1; i < 16; i++)
    {
        if (atan(i) * FINEANGLES / M_PI - fov >= FINEANGLES / 8)
        {
            max_project_slope = i;
            break;
        }
    }
}

//
// R_InitTextureMapping
//
void R_InitTextureMapping (void)
{
    int			i;
    int			x;
    int			t;
    fixed_t		focallength;
    angle_t		fov; // [Woof!]
    
    // Use tangent table to generate viewangletox:
    //  viewangletox will give the next greatest x
    //  after the view angle.
    //
    // Calc focallength
    //  so FIELDOFVIEW angles covers SCREENWIDTH.
    focallength = FixedDiv (centerxfrac, fovscale);
	
    if (vid_fov == 90 && centerxfrac == centerxfrac_nonwide)
    {
        fov = FIELDOFVIEW;
    }
    else
    {
        const double slope = (tan(vid_fov * M_PI / 360.0) *
                              centerxfrac / centerxfrac_nonwide);
        fov = atan(slope) * FINEANGLES / M_PI;
    }

    for (i=0 ; i<FINEANGLES/2 ; i++)
    {
	if (finetangent[i] > fovscale)
	    t = -1;
	else if (finetangent[i] < -fovscale)
	    t = viewwidth+1;
	else
	{
	    t = FixedMul (finetangent[i], focallength);
	    t = (centerxfrac - t+FRACUNIT-1)>>FRACBITS;

	    if (t < -1)
		t = -1;
	    else if (t>viewwidth+1)
		t = viewwidth+1;
	}
	viewangletox[i] = t;
    }
    
    // Scan viewangletox[] to generate xtoviewangle[]:
    //  xtoviewangle will give the smallest view angle
    //  that maps to x.	
    for (x=0;x<=viewwidth;x++)
    {
	i = 0;
	while (viewangletox[i]>x)
	    i++;
	xtoviewangle[x] = (i<<ANGLETOFINESHIFT)-ANG90;
	// [crispy] calculate sky angle for drawing horizontally linear skies.
	// Taken from GZDoom and refactored for integer math.
	linearskyangle[x] = ((viewwidth / 2 - x) * ((SCREENWIDTH<<6) / viewwidth)) 
	                                         * (ANG90 / (NONWIDEWIDTH<<6)) / fovdiff;
    }
    
    // Take out the fencepost cases from viewangletox.
    for (i=0 ; i<FINEANGLES/2 ; i++)
    {
	t = FixedMul (finetangent[i], focallength);
	t = centerx - t;
	
	if (viewangletox[i] == -1)
	    viewangletox[i] = 0;
	else if (viewangletox[i] == viewwidth+1)
	    viewangletox[i]  = viewwidth;
    }
	
    clipangle = xtoviewangle[0];
    CalcMaxProjectSlope(fov);
}



//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//
#define DISTMAP		2

void R_InitLightTables (void)
{
    int		i;
    int		j;
    int		level;
    int		startmap; 	
    int		scale;
    
    if (scalelight)
    {
        for (i = 0; i < LIGHTLEVELS; i++)
        {
            free(scalelight[i]);
        }
        free(scalelight);
    }

    if (scalelightfixed)
    {
        free(scalelightfixed);
    }

    if (zlight)
    {
        for (i = 0; i < LIGHTLEVELS; i++)
        {
            free(zlight[i]);
        }
        free(zlight);
    }

    // [crispy] smooth diminishing lighting
    if (vis_smooth_light)
    {
#ifdef CRISPY_TRUECOLOR
    if (vid_truecolor)
    {
	    // [crispy] if in TrueColor mode, use smoothest diminished lighting
	    LIGHTLEVELS =      16 << 4;
	    LIGHTSEGSHIFT =     4 -  4;
	    LIGHTBRIGHT =       1 << 4;
	    MAXLIGHTSCALE =    48 << 3;
	    LIGHTSCALESHIFT =  12 -  3;
	    MAXLIGHTZ =       128 << 6;
	    LIGHTZSHIFT =      20 -  6;
    }
    else
#endif
    {
	    // [crispy] else, use paletted approach
	    LIGHTLEVELS =      16 << 1;
	    LIGHTSEGSHIFT =     4 -  1;
	    LIGHTBRIGHT =       1 << 1;
	    MAXLIGHTSCALE =    48 << 0;
	    LIGHTSCALESHIFT =  12 -  0;
	    MAXLIGHTZ =       128 << 3;
	    LIGHTZSHIFT =      20 -  3;
    }
    }
    else
    {
        LIGHTLEVELS = 16;
        LIGHTSEGSHIFT = 4;
        LIGHTBRIGHT = 1;
        MAXLIGHTSCALE = 48;
        LIGHTSCALESHIFT = 12;
        MAXLIGHTZ = 128;
        LIGHTZSHIFT = 20;
    }

    scalelight = malloc(LIGHTLEVELS * sizeof(*scalelight));
    scalelightfixed = malloc(MAXLIGHTSCALE * sizeof(*scalelightfixed));
    zlight = malloc(LIGHTLEVELS * sizeof(*zlight));

    // Calculate the light levels to use
    //  for each level / distance combination.
    for (i=0 ; i< LIGHTLEVELS ; i++)
    {
	scalelight[i] = malloc(MAXLIGHTSCALE * sizeof(**scalelight));
	zlight[i] = malloc(MAXLIGHTZ * sizeof(**zlight));

	startmap = ((LIGHTLEVELS-LIGHTBRIGHT-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
	for (j=0 ; j<MAXLIGHTZ ; j++)
	{
	    scale = FixedDiv ((ORIGWIDTH/2*FRACUNIT), (j+1)<<LIGHTZSHIFT);
	    scale >>= LIGHTSCALESHIFT;
	    level = startmap - scale/DISTMAP;
	    
	    if (level < 0)
		level = 0;

	    if (level >= NUMCOLORMAPS)
		level = NUMCOLORMAPS-1;

	    zlight[i][j] = colormaps + level*256;
	}
    }
}



//
// R_SetViewSize
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
boolean		setsizeneeded;
int		setblocks;
int		setdetail;

// [crispy] lookup table for horizontal screen coordinates
int		flipscreenwidth[MAXWIDTH];
int		*flipviewwidth;

void
R_SetViewSize
( int		blocks,
  int		detail )
{
    setsizeneeded = true;
    setblocks = MIN(blocks, 11); // [crispy] Crispy HUD
    setdetail = detail;
}


//
// R_ExecuteSetViewSize
//
void R_ExecuteSetViewSize (void)
{
    fixed_t	cosadj;
    fixed_t	dy;
    int		i;
    int		j;
    int		level;
    int		startmap; 	
    double	WIDEFOVDELTA;  // [JN] FOV from DOOM Retro and Nugget Doom

    setsizeneeded = false;

    if (setblocks >= 11) // [crispy] Crispy HUD
    {
	scaledviewwidth_nonwide = NONWIDEWIDTH;
	scaledviewwidth = SCREENWIDTH;
	viewheight = SCREENHEIGHT;
    }
    // [crispy] hard-code to SCREENWIDTH and SCREENHEIGHT minus status bar height
    else if (setblocks == 10)
    {
	scaledviewwidth_nonwide = NONWIDEWIDTH;
	scaledviewwidth = SCREENWIDTH;
	viewheight = SCREENHEIGHT-(ST_HEIGHT*vid_resolution);
    }
    else
    {
	scaledviewwidth_nonwide = (setblocks*32)*vid_resolution;
	viewheight = ((setblocks*168/10)&~7)*vid_resolution;

	// [crispy] regular viewwidth in non-widescreen mode
	if (vid_widescreen)
	{
		const int widescreen_edge_aligner = 8 * vid_resolution;

		scaledviewwidth = viewheight*SCREENWIDTH/(SCREENHEIGHT-(ST_HEIGHT*vid_resolution));
		// [crispy] make sure scaledviewwidth is an integer multiple of the bezel patch width
		scaledviewwidth = (scaledviewwidth / widescreen_edge_aligner) * widescreen_edge_aligner;
		scaledviewwidth = MIN(scaledviewwidth, SCREENWIDTH);
	}
	else
	{
		scaledviewwidth = scaledviewwidth_nonwide;
	}
    }
    
    detailshift = setdetail;
    viewwidth = scaledviewwidth>>detailshift;
    viewwidth_nonwide = scaledviewwidth_nonwide>>detailshift;
	
    // [JN] FOV from DOOM Retro and Nugget Doom
    fovdiff = (float) 90 / vid_fov;
    if (vid_widescreen) 
    {
        // fov * 0.82 is vertical FOV for 4:3 aspect ratio
        WIDEFOVDELTA = (atan(SCREENWIDTH / ((SCREENHEIGHT * 1.2)
                     / tan(vid_fov * 0.82 * M_PI / 360.0))) * 360.0 / M_PI) - vid_fov;
    }
    else
    {
        WIDEFOVDELTA = 0;
    }

    // villsa [STRIFE] calculate centery from player's pitch
    centery = (setblocks*(players[consoleplayer].pitch * vid_resolution));
    centery = (unsigned int)(centery/10)+viewheight/2;

    centerx = viewwidth/2;
    centerxfrac = centerx<<FRACBITS;
    centeryfrac = centery<<FRACBITS;
    centerxfrac_nonwide = (viewwidth_nonwide/2)<<FRACBITS;
    // [JN] FOV from DOOM Retro and Nugget Doom
    fovscale = finetangent[(int)(FINEANGLES / 4 + (vid_fov + WIDEFOVDELTA) * FINEANGLES / 360 / 2)];
    projection = FixedDiv(centerxfrac, fovscale);

    //if (!detailshift) // villsa [STRIFE]
    {
	colfunc = basecolfunc = R_DrawColumn;
	fuzzcolfunc = R_DrawTLColumn;   // villsa [STRIFE]
	transcolfunc = R_DrawTranslatedColumn;
	spanfunc = R_DrawSpan;
    }
    // villsa [STRIFE] unused detail stuff
    /*else
    {
	colfunc = basecolfunc = R_DrawColumnLow;
	fuzzcolfunc = R_DrawFuzzColumnLow;
	transcolfunc = R_DrawTranslatedColumnLow;
	spanfunc = R_DrawSpanLow;
    }*/

    R_InitBuffer (scaledviewwidth, viewheight);
	
    R_InitTextureMapping ();
    
    // psprite scales
    pspritescale = FRACUNIT*viewwidth_nonwide/ORIGWIDTH;
    pspriteiscale = FRACUNIT*ORIGWIDTH/viewwidth_nonwide;
    
    // thing clipping
    for (i=0 ; i<viewwidth ; i++)
	screenheightarray[i] = viewheight;
    
    // planes
    for (i=0 ; i<viewheight ; i++)
    {
	// [crispy] re-generate lookup-table for yslope[] (free look)
	// whenever "detailshift" or "screenblocks" change
	// [JN] FOV from DOOM Retro and Nugget Doom
	const fixed_t num = FixedMul(FixedDiv(FRACUNIT, fovscale), (viewwidth<<detailshift)*FRACUNIT/2);
	for (j = 0; j < LOOKDIRS; j++)
	{
	dy = ((i-(viewheight/2 + ((j-LOOKDIRMIN) * (1 * vid_resolution)) * (dp_screen_size < 11 ? dp_screen_size : 11) / 10))<<FRACBITS)+FRACUNIT/2;
	dy = abs(dy);
	yslopes[j][i] = FixedDiv (num, dy);
	}
    }
    yslope = yslopes[LOOKDIRMIN];
	
    for (i=0 ; i<viewwidth ; i++)
    {
	cosadj = abs(finecosine[xtoviewangle[i]>>ANGLETOFINESHIFT]);
	distscale[i] = FixedDiv (FRACUNIT,cosadj);
    }
    
    // Calculate the light levels to use
    //  for each level / scale combination.
    for (i=0 ; i< LIGHTLEVELS ; i++)
    {

	startmap = ((LIGHTLEVELS-LIGHTBRIGHT-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
	for (j=0 ; j<MAXLIGHTSCALE ; j++)
	{
	    level = startmap - j*NONWIDEWIDTH/(viewwidth_nonwide<<detailshift)/DISTMAP;
	    
	    if (level < 0)
		level = 0;

	    if (level >= NUMCOLORMAPS)
		level = NUMCOLORMAPS-1;

	    scalelight[i][j] = colormaps + level*256;
	}
    }

    // [crispy] lookup table for horizontal screen coordinates
    for (i = 0, j = SCREENWIDTH - 1; i < SCREENWIDTH; i++, j--)
    {
	flipscreenwidth[i] = gp_flip_levels ? j : i;
    }

    flipviewwidth = flipscreenwidth + (gp_flip_levels ? (SCREENWIDTH - scaledviewwidth) : 0);

    pspr_interp = false; // [crispy] interpolate weapon bobbing
}



//
// R_Init
//



void R_Init (void)
{
    R_InitData ();
    printf (".");
    // viewwidth / viewheight / dp_detail_level are set by the defaults
    R_SetViewSize (dp_screen_size, dp_detail_level);
    R_InitPlanes ();
    printf (".");
    R_InitLightTables ();
    printf (".");
    if(devparm)
        printf (".");
    else
        D_IntroTick();
    
    R_InitSkyMap ();
    if(!devparm)
        D_IntroTick();

    R_InitTranslationTables ();
    if(devparm)
        printf (".");
    else
        D_IntroTick();

    framecount = 0;
}


//
// R_PointInSubsector
//
subsector_t*
R_PointInSubsector
( fixed_t	x,
  fixed_t	y )
{
    node_t*	node;
    int		side;
    int		nodenum;

    // single subsector is a special case
    if (!numnodes)				
	return subsectors;
		
    nodenum = numnodes-1;

    while (! (nodenum & NF_SUBSECTOR) )
    {
	node = &nodes[nodenum];
	side = R_PointOnSide (x, y, node);
	nodenum = node->children[side];
    }
	
    return &subsectors[nodenum & ~NF_SUBSECTOR];
}

//
// R_SetupPitch
// villsa [STRIFE] new function
// Calculate centery/centeryfrac for player viewpitch
//

void R_SetupPitch(int pitch)
{
    int pitchfrac;

    if (viewpitch != pitch)
    {
        viewpitch   = pitch;
        pitchfrac   = (setblocks * (pitch * vid_resolution)) / 10;
        centery     = pitchfrac + viewheight / 2;
        centeryfrac = centery << FRACBITS;

        // [crispy] apply lookup table
        yslope = yslopes[LOOKDIRMIN + pitch];
    }
}

// [crispy]
static inline boolean CheckLocalView(const player_t *player)
{
  return (
    // Don't use localview if the player is spying.
    player == &players[consoleplayer] &&
    // Don't use localview if the player is dead.
    player->playerstate != PST_DEAD &&
    // Don't use localview if the player just teleported.
    !player->mo->reactiontime &&
    // Don't use localview if a demo is playing or recording.
    !demoplayback && !demorecording &&
    // Don't use localview during a netgame (single-player only).
    !netgame
  );
}

//
// R_SetupFrame
//
void R_SetupFrame (player_t* player)
{		
    int		i;
    int		pitch;
    
    viewplayer = player;

    // [AM] Interpolate the player camera if the feature is enabled.
    if (vid_uncapped_fps && leveltime > 1 && player->mo->interp == true &&
        leveltime > oldleveltime && !do_wipe)
    {
        const boolean use_localview = CheckLocalView(player);

        viewx = LerpFixed(player->mo->oldx, player->mo->x);
        viewy = LerpFixed(player->mo->oldy, player->mo->y);
        viewz = LerpFixed(player->oldviewz, player->viewz);
        if (use_localview)
        {
            viewangle = (player->mo->angle + localview.angle -
                        localview.ticangle + LerpAngle(localview.oldticangle,
                                                       localview.ticangle)) + viewangleoffset;
        }
        else
        {
            viewangle = LerpAngle(player->mo->oldangle, player->mo->angle) + viewangleoffset;
        }

        pitch = LerpInt(player->oldpitch, player->pitch);
    }
    else
    {
        viewx = player->mo->x;
        viewy = player->mo->y;
        viewz = player->viewz;
        viewangle = player->mo->angle + viewangleoffset;
        pitch = player->pitch;
    }

    R_SetupPitch(pitch);  // villsa [STRIFE]

    extralight = player->extralight;
    
    viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
    viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];
	
    sscount = 0;
	
    if (player->fixedcolormap)
    {
	fixedcolormap =
	    colormaps
	    // [crispy] sizeof(lighttable_t) not needed in paletted render
	    // and breaks Sigil weapon effects in true color render
	    + player->fixedcolormap*(NUMCOLORMAPS / 32)*256/**sizeof(lighttable_t)*/;  // [crispy] smooth diminishing lighting
	
	walllights = scalelightfixed;

	for (i=0 ; i<MAXLIGHTSCALE ; i++)
	    scalelightfixed[i] = fixedcolormap;
    }
    else
	fixedcolormap = 0;
		
    framecount++;
    validcount++;
}



//
// R_RenderView
//
void R_RenderPlayerView (player_t* player)
{	
    R_SetupFrame (player);

    // Clear buffers.
    R_ClearClipSegs ();
    R_ClearDrawSegs ();
    R_ClearPlanes ();
    R_ClearSprites ();
    // [crispy]
    if (automapactive && !automap_overlay)
    {
        R_RenderBSPNode (numnodes-1);
        return;
    }
    
    // check for new console commands.
    NetUpdate ();

    // [crispy] smooth texture scrolling
    R_InterpolateTextureOffsets();

    // The head node is the last node output.
    R_RenderBSPNode (numnodes-1);
    
    // Check for new console commands.
    NetUpdate ();
    
    R_DrawPlanes ();
    
    // Check for new console commands.
    NetUpdate ();
    
    R_DrawMasked ();

    // Check for new console commands.
    NetUpdate ();				
}
