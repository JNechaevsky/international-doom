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


#define _USE_MATH_DEFINES
#include <math.h>
#include "f_wipe.h"
#include "m_random.h"
#include "h2def.h"
#include "m_bbox.h"
#include "p_local.h"
#include "r_local.h"
#include "v_video.h"

#include "id_vars.h"
#include "id_func.h"


int viewangleoffset;

// haleyjd: removed WATCOMC

int validcount = 1;             // increment every time a check is made

lighttable_t *fixedcolormap;

int centerx, centery;
fixed_t centerxfrac, centeryfrac;
fixed_t projection;

fixed_t viewx, viewy, viewz;
angle_t viewangle;
fixed_t viewcos, viewsin;
player_t *viewplayer;
localview_t localview; // [crispy]

int detailshift;                // 0 = high, 1 = low

//
// precalculated math tables
//
angle_t clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup maps the visible view
// angles  to screen X coordinates, flattening the arc to a flat projection
// plane.  There will be many angles mapped to the same X.
int viewangletox[FINEANGLES / 2];

// The xtoviewangleangle[] table maps a screen pixel to the lowest viewangle
// that maps back to x ranges from clipangle to -clipangle
angle_t xtoviewangle[MAXWIDTH + 1];

// [crispy] calculate the linear sky angle component here
angle_t linearskyangle[MAXWIDTH+1];

// [crispy] lookup table for horizontal screen coordinates
int  flipscreenwidth[MAXWIDTH];
int *flipviewwidth;

// [crispy] parameterized for smooth diminishing lighting
lighttable_t***		scalelight = NULL;
lighttable_t**		scalelightfixed = NULL;
lighttable_t***		zlight = NULL;

int extralight;                 // bumped light from gun blasts

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
void (*tlcolfunc) (void);
void (*tladdcolfunc) (void);
void (*transcolfunc) (void);
void (*extratlcolfunc) (void);
void (*spanfunc) (void);

/*
===================
=
= R_AddPointToBox
=
===================
*/

/*
void R_AddPointToBox (int x, int y, fixed_t *box)
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
*/


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

// -----------------------------------------------------------------------------
// R_PointOnSegSide
// [PN] killough 5/2/98: reformatted
// -----------------------------------------------------------------------------

int R_PointOnSegSide (fixed_t x, fixed_t y, const seg_t *line)
{
    const fixed_t lx = line->v1->x;
    const fixed_t ly = line->v1->y;
    const fixed_t ldx = line->v2->x - lx;
    const fixed_t ldy = line->v2->y - ly;
    
    if (!ldx)
    {
        return x <= lx ? ldy > 0 : ldy < 0;
    }

    if (!ldy)
    {
        return y <= ly ? ldx < 0 : ldx > 0;
    }

    x -= lx;
    y -= ly;

    // Try to quickly decide by looking at sign bits.
    if ((ldy ^ ldx ^ x ^ y) < 0)
        return (ldy ^ x) < 0; // (left is negative)
            return FixedMul(y, ldx>>FRACBITS) >= FixedMul(ldy>>FRACBITS, x);
}


// [crispy] turned into a general R_PointToAngle() flavor
// called with either slope_div = SlopeDivCrispy() from R_PointToAngleCrispy()
// or slope_div = SlopeDiv() else
// [PN] Reformatted for readability and reduced nesting
/*
===============================================================================
=
= R_PointToAngleSlope
=
===============================================================================
*/

#define	DBITS		(FRACBITS-SLOPEBITS)

angle_t R_PointToAngleSlope(fixed_t x, fixed_t y,
            int (*slope_div) (unsigned int num, unsigned int den))
{
    // [PN] Shift to local player coordinates
    x -= viewx;
    y -= viewy;

    // [PN] If the point matches the player's position
    if (!x && !y)
        return 0;

    if (x >= 0)
    {
        if (y >= 0)
        {
            // [PN] First quadrant → octants 0 or 1
            return (x > y) 
                 ? tantoangle[slope_div(y, x)]
                 : (ANG90 - 1 - tantoangle[slope_div(x, y)]);
        }
        else
        {
            // [PN] Fourth quadrant → octants 8 or 7
            return (x > -y) 
                 ? (0 - tantoangle[slope_div(-y, x)]) 
                 : (ANG270 + tantoangle[slope_div(x, -y)]);
        }
    }
    else
    {
        if (y >= 0)
        {
            // [PN] Second quadrant → octants 3 or 2
            return (-x > y)
                 ? (ANG180 - 1 - tantoangle[slope_div(y, -x)])
                 : (ANG90 + tantoangle[slope_div(-x, y)]);
        }
        else
        {
            // [PN] Third quadrant → octants 4 or 5
            return (-x > -y)
                 ? (ANG180 + tantoangle[slope_div(-y, -x)])
                 : (ANG270 - 1 - tantoangle[slope_div(-x, -y)]);
        }
    }
}


angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
    return R_PointToAngleSlope(x, y, SlopeDiv);
}

// [crispy] overflow-safe R_PointToAngle() flavor
// called only from R_CheckBBox(), R_AddLine() and P_SegLengths()
angle_t R_PointToAngleCrispy(fixed_t x, fixed_t y)
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

    return R_PointToAngleSlope(x, y, SlopeDivCrispy);
}

angle_t R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    viewx = x1;
    viewy = y1;
    // [crispy] R_PointToAngle2() is never called during rendering
    return R_PointToAngleSlope(x2, y2, SlopeDiv);
}


//=============================================================================

// [crispy] WiggleFix: move R_ScaleFromGlobalAngle function to r_segs.c, above
// R_StoreWallRange
#if 0
/*
================
=
= R_ScaleFromGlobalAngle
=
= Returns the texture mapping scale for the current line at the given angle
= rw_distance must be calculated first
================
*/

fixed_t R_ScaleFromGlobalAngle(angle_t visangle)
{
    fixed_t scale;
    int anglea, angleb;
    int sinea, sineb;
    fixed_t num, den;

#if 0
    {
        fixed_t dist, z;
        fixed_t sinv, cosv;

        sinv = finesine[(visangle - rw_normalangle) >> ANGLETOFINESHIFT];
        dist = FixedDiv(rw_distance, sinv);
        cosv = finecosine[(viewangle - visangle) >> ANGLETOFINESHIFT];
        z = abs(FixedMul(dist, cosv));
        scale = FixedDiv(projection, z);
        return scale;
    }
#endif

    anglea = ANG90 + (visangle - viewangle);
    angleb = ANG90 + (visangle - rw_normalangle);
// bothe sines are allways positive
    sinea = finesine[anglea >> ANGLETOFINESHIFT];
    sineb = finesine[angleb >> ANGLETOFINESHIFT];
    num = FixedMul(projection, sineb) << detailshift;
    den = FixedMul(rw_distance, sinea);
    if (den > num >> 16)
    {
        scale = FixedDiv(num, den);
        if (scale > 64 * FRACUNIT)
            scale = 64 * FRACUNIT;
        else if (scale < 256)
            scale = 256;
    }
    else
        scale = 64 * FRACUNIT;

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

/*
=================
=
= R_InitTextureMapping
=
=================
*/

void R_InitTextureMapping(void)
{
    int i;
    int x;
    int t;
    fixed_t focallength;
    angle_t fov; // [Woof!]


//
// use tangent table to generate viewangletox
// viewangletox will give the next greatest x after the view angle
//
    // calc focallength so FIELDOFVIEW angles covers SCREENWIDTH
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

    for (i = 0; i < FINEANGLES / 2; i++)
    {
        if (finetangent[i] > fovscale)
            t = -1;
        else if (finetangent[i] < -fovscale)
            t = viewwidth + 1;
        else
        {
            t = FixedMul(finetangent[i], focallength);
            t = (centerxfrac - t + FRACUNIT - 1) >> FRACBITS;
            if (t < -1)
                t = -1;
            else if (t > viewwidth + 1)
                t = viewwidth + 1;
        }
        viewangletox[i] = t;
    }

//
// scan viewangletox[] to generate xtoviewangleangle[]
//
// xtoviewangle will give the smallest view angle that maps to x

    // [JN] Precalculate linearskyangle[] multipler.
    const int linear_factor = (((SCREENWIDTH << 6) / viewwidth)
                            * (ANG90 / (NONWIDEWIDTH << 6))) / fovdiff;

    for (x = 0; x <= viewwidth; x++)
    {
        i = 0;
        while (viewangletox[i] > x)
            i++;
        xtoviewangle[x] = (i << ANGLETOFINESHIFT) - ANG90;
	    // [crispy] calculate sky angle for drawing horizontally linear skies.
	    // Taken from GZDoom and refactored for integer math.
	    linearskyangle[x] = (viewwidth / 2 - x) * linear_factor;
    }

//
// take out the fencepost cases from viewangletox
//
    for (i = 0; i < FINEANGLES / 2; i++)
    {
        if (viewangletox[i] == -1)
            viewangletox[i] = 0;
        else if (viewangletox[i] == viewwidth + 1)
            viewangletox[i] = viewwidth;
    }

    clipangle = xtoviewangle[0];
    CalcMaxProjectSlope(fov);
}

//=============================================================================

/*
====================
=
= R_InitLightTables
=
= Only inits the zlight table, because the scalelight table changes
= with view size
=
====================
*/


void R_InitLightTables(void)
{
    int i, j, level, startmap;
    int scale;
    // [PN] Pre-calculate for slight optimization
    const int fracwidth = (ORIGWIDTH / 2 * FRACUNIT);

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
        LIGHTLEVELS =      16;
        LIGHTSEGSHIFT =     4;
        LIGHTBRIGHT =       1;
        MAXLIGHTSCALE =    48;
        LIGHTSCALESHIFT =  12;
        MAXLIGHTZ =       128;
        LIGHTZSHIFT =      20;
    }

    scalelight = malloc(LIGHTLEVELS * sizeof(*scalelight));
    scalelightfixed = malloc(MAXLIGHTSCALE * sizeof(*scalelightfixed));
    zlight = malloc(LIGHTLEVELS * sizeof(*zlight));

//
// Calculate the light levels to use for each level / distance combination
//
    for (i = 0; i < LIGHTLEVELS; i++)
    {
        scalelight[i] = malloc(MAXLIGHTSCALE * sizeof(**scalelight));
        zlight[i] = malloc(MAXLIGHTZ * sizeof(**zlight));
	
        // [PN] Use bit shifting for faster handling
        startmap = ((LIGHTLEVELS - LIGHTBRIGHT - i) << 1) * NUMCOLORMAPS / LIGHTLEVELS;
        for (j = 0; j < MAXLIGHTZ; j++)
        {
            // [PN] Use precalculated fracwidth value
            scale = (FixedDiv(fracwidth, (j + 1) << LIGHTZSHIFT)) >> LIGHTSCALESHIFT;
            // [PN] Use bit shifting for faster handling
            level = startmap - (scale >> 1);
            // [PN] Clamp light level values
            level = BETWEEN(0, NUMCOLORMAPS - 1, level);

            zlight[i][j] = colormaps + level * 256;
        }
    }
}


/*
==============
=
= R_SetViewSize
=
= Don't really change anything here, because i might be in the middle of
= a refresh.  The change will take effect next refresh.
=
==============
*/

boolean setsizeneeded;
int setblocks, setdetail;

void R_SetViewSize(int blocks, int detail)
{
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
}

/*
==============
=
= R_ExecuteSetViewSize
=
==============
*/

void R_ExecuteSetViewSize(void)
{
    fixed_t cosadj, dy;
    int i, j, level, startmap;
    double WIDEFOVDELTA;  // [JN] FOV from DOOM Retro and Nugget Doom

    setsizeneeded = false;

    if (setblocks >= 11)
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
	viewheight = SCREENHEIGHT-SBARHEIGHT;
    }
    else
    {
        scaledviewwidth_nonwide = (setblocks * 32) * vid_resolution;
        viewheight = (setblocks * 161 / 10) * vid_resolution;

	// [crispy] regular viewwidth in non-widescreen mode
	if (vid_widescreen)
	{
		const int widescreen_edge_aligner = (16 * vid_resolution) - 1;

		scaledviewwidth = viewheight*SCREENWIDTH/(SCREENHEIGHT-SBARHEIGHT);
		// [crispy] make sure scaledviewwidth is an integer multiple of the bezel patch width
		scaledviewwidth = (scaledviewwidth + widescreen_edge_aligner) & (int)~widescreen_edge_aligner;
		scaledviewwidth = MIN(scaledviewwidth, SCREENWIDTH);
	}
	else
	{
		scaledviewwidth = scaledviewwidth_nonwide;
	}
    }

    detailshift = setdetail;
    viewwidth = scaledviewwidth >> detailshift;
    viewwidth_nonwide = scaledviewwidth_nonwide >> detailshift;

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

    centery = viewheight / 2;
    centerx = viewwidth / 2;
    centerxfrac = centerx << FRACBITS;
    centeryfrac = centery << FRACBITS;
    centerxfrac_nonwide = (viewwidth_nonwide / 2) << FRACBITS;
    // [JN] FOV from DOOM Retro and Nugget Doom
    fovscale = finetangent[(int)(FINEANGLES / 4 + (vid_fov + WIDEFOVDELTA) * FINEANGLES / 360 / 2)];
    projection = FixedDiv(centerxfrac, fovscale);

    if (!detailshift)
    {
        colfunc = basecolfunc = R_DrawColumn;
        tlcolfunc = R_DrawTLColumn;
        tladdcolfunc = R_DrawTLAddColumn;
        transcolfunc = R_DrawTranslatedColumn;
        extratlcolfunc = R_DrawExtraTLColumn;
        spanfunc = R_DrawSpan;
    }
    else
    {
        colfunc = basecolfunc = R_DrawColumnLow;
        tlcolfunc = R_DrawTLColumn;
        tladdcolfunc = R_DrawTLAddColumnLow;
        transcolfunc = R_DrawTranslatedColumn;
        extratlcolfunc = R_DrawExtraTLColumnLow;
        spanfunc = R_DrawSpanLow;
    }

    R_InitBuffer(scaledviewwidth, viewheight);

    R_InitTextureMapping();

//
// psprite scales
//
    pspritescale = FRACUNIT * viewwidth_nonwide / ORIGWIDTH;
    pspriteiscale = FRACUNIT * ORIGWIDTH / viewwidth_nonwide;

//
// thing clipping
//
    for (i = 0; i < viewwidth; i++)
        screenheightarray[i] = viewheight;

//
// planes
//
    for (i = 0; i < viewheight; i++)
    {
        // [crispy] re-generate lookup-table for yslope[] (free look)
        // whenever "detailshift" or "dp_screen_size" change
        // [JN] FOV from DOOM Retro and Nugget Doom
        const fixed_t num = FixedMul(FixedDiv(FRACUNIT, fovscale), (viewwidth<<detailshift)*FRACUNIT/2);
        for (j = 0; j < LOOKDIRS; j++)
        {
        dy = ((i - (viewheight / 2 + ((j - LOOKDIRMIN) * (1 * vid_resolution)) *
                (dp_screen_size < 11 ? dp_screen_size : 11) / 10)) << FRACBITS) +
                FRACUNIT / 2;
        dy = abs(dy);
        yslopes[j][i] = FixedDiv(num, dy);
        }
    }
    yslope = yslopes[LOOKDIRMIN];

    for (i = 0; i < viewwidth; i++)
    {
        cosadj = abs(finecosine[xtoviewangle[i] >> ANGLETOFINESHIFT]);
        distscale[i] = FixedDiv(FRACUNIT, cosadj);
    }

//
// Calculate the light levels to use for each level / scale combination
//
    for (i = 0; i < LIGHTLEVELS; i++)
    {
        // [PN] Use bit shifting for faster handling
        startmap = ((LIGHTLEVELS - LIGHTBRIGHT - i) << 1) * NUMCOLORMAPS / LIGHTLEVELS;
        for (j = 0; j < MAXLIGHTSCALE; j++)
        {
            // [PN] Use bit shifting for faster handling
            level = startmap - ((j * NONWIDEWIDTH / (viewwidth_nonwide << detailshift)) >> 1);
            // [PN] Clamp light level values
            level = BETWEEN(0, NUMCOLORMAPS - 1, level);

            scalelight[i][j] = colormaps + level * 256;
        }
    }

    // [crispy] lookup table for horizontal screen coordinates
    for (i = 0, j = SCREENWIDTH - 1; i < SCREENWIDTH; i++, j--)
    {
        flipscreenwidth[i] = gp_flip_levels ? j : i;
    }

    flipviewwidth = flipscreenwidth + (gp_flip_levels ? (SCREENWIDTH - scaledviewwidth) : 0);

//
// draw the border
//
    R_FillBackScreen();         // erase old menu stuff

    // [crispy] Redraw status bar needed for widescreen HUD
    SB_ForceRedraw();

    pspr_interp = false; // [crispy]
}


/*
==============
=
= R_Init
=
==============
*/

int detailLevel;

void R_Init(void)
{
    R_InitData();
    // viewwidth / viewheight / detailLevel are set by the defaults
    R_SetViewSize(dp_screen_size, detailLevel);
    R_InitPlanes();
    R_InitLightTables();
    R_InitSkyMap();
    R_InitTranslationTables();
}

/*
==============
=
= R_PointInSubsector
=
==============
*/

subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
    node_t *node;
    int side, nodenum;

    if (!numnodes)              // single subsector is a special case
        return subsectors;

    nodenum = numnodes - 1;

    while (!(nodenum & NF_SUBSECTOR))
    {
        node = &nodes[nodenum];
        side = R_PointOnSide(x, y, node);
        nodenum = node->children[side];
    }

    return &subsectors[nodenum & ~NF_SUBSECTOR];

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

//----------------------------------------------------------------------------
//
// PROC R_SetupFrame
//
//----------------------------------------------------------------------------

void R_SetupFrame(player_t * player)
{
    int i;
    int tableAngle;
    int tempCentery;
    int intensity;
    static int x_quake, y_quake, quaketime; // [crispy]
    int pitch; // [crispy]

    //drawbsp = 1;
    viewplayer = player;
    // haleyjd: removed WATCOMC
    // haleyjd FIXME: viewangleoffset handling?

    if (crl_spectating)
    {
        fixed_t bx, by, bz;
        angle_t ba;

    	// RestlessRodent -- Get camera position
    	CRL_GetCameraPos(&bx, &by, &bz, &ba);
        
        if (vid_uncapped_fps)
        {
            viewx = LerpFixed(CRL_camera_oldx, bx);
            viewy = LerpFixed(CRL_camera_oldy, by);
            viewz = LerpFixed(CRL_camera_oldz, bz);
            viewangle = LerpAngle(CRL_camera_oldang, ba);
        }
        else
        {
            viewx = bx;
            viewy = by;
            viewz = bz;
            viewangle = ba;
        }
        pitch = 0;
    }
    else
    {
    if (vid_uncapped_fps && leveltime > 1 && player->mo->interp == true
    && realleveltime > oldleveltime && !do_wipe)
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

            pitch = LerpInt(player->oldlookdir, player->lookdir);
    }
    else
    {
        viewangle = player->mo->angle + viewangleoffset;
        viewx = player->mo->x;
        viewy = player->mo->y;
        viewz = player->viewz;
        pitch = player->lookdir; // [crispy]
    }
    }

    // [JN] Limit pitch (lookdir amplitude) for higher FOV levels.
    if (vid_fov > 90)
    {
        pitch *= fovdiff*fovdiff;
    }

    if (localQuakeHappening[displayplayer] && !paused && !crl_freeze)
    {
        // [crispy] only get new quake values once every gametic
        if (leveltime > quaketime)
        {
            intensity = localQuakeHappening[displayplayer];
            x_quake = ((M_Random() % (intensity << 2))
                           - (intensity << 1)) << FRACBITS;
            y_quake = ((M_Random() % (intensity << 2))
                           - (intensity << 1)) << FRACBITS;
            quaketime = leveltime;
        }

        if (vid_uncapped_fps)
        {
            viewx += FixedMul(x_quake, fractionaltic);
            viewy += FixedMul(y_quake, fractionaltic);
        }
        else
        {
            viewx += x_quake;
            viewy += y_quake;
        }
    }
    else if (!localQuakeHappening[displayplayer])
    {
        quaketime = 0;
    }

    extralight = player->extralight;
    extralight += dp_level_brightness;  // [JN] Level Brightness feature.

    // RestlessRodent -- Just report it
    CRL_ReportPosition(viewx, viewy, viewz, viewangle);

    tableAngle = viewangle >> ANGLETOFINESHIFT;

    // [crispy] apply new yslope[] whenever "lookdir", "detailshift" or
    // "dp_screen_size" change
    tempCentery = viewheight / 2 + (pitch * (1 * vid_resolution)) *
                    (dp_screen_size < 11 ? dp_screen_size : 11) / 10;
    if (centery != tempCentery)
    {
        centery = tempCentery;
        centeryfrac = centery << FRACBITS;
        yslope = yslopes[LOOKDIRMIN + pitch];
    }
    viewsin = finesine[tableAngle];
    viewcos = finecosine[tableAngle];
    if (player->fixedcolormap)
    {
        fixedcolormap = colormaps + player->fixedcolormap
            * (NUMCOLORMAPS / 32) // [crispy] smooth diminishing lighting
            // [crispy] sizeof(lighttable_t) not needed in paletted render
            // and breaks Torch's fixed colormap indexes in true color render
            * 256 /* * sizeof(lighttable_t)*/;
        walllights = scalelightfixed;
        for (i = 0; i < MAXLIGHTSCALE; i++)
        {
            scalelightfixed[i] = fixedcolormap;
        }
    }
    else
    {
        fixedcolormap = 0;
    }
    validcount++;

#if 0
    {
        static int frame;
        memset(screen, frame, SCREENAREA);
        frame++;
    }
#endif
}

/*
==============
=
= R_RenderView
=
==============
*/

void R_RenderPlayerView(player_t * player)
{
    extern void PO_InterpolatePolyObjects(void);

    // [JN] Reset render counters.
    memset(&IDRender, 0, sizeof(IDRender));

    R_SetupFrame(player);

    // [JN] Fill view buffer with black color to prevent
    // overbrighting from post-processing effects and 
    // forcefully update status bar if any effect is active.
    if (V_PProc_EffectsActive())
    {
        V_DrawFilledBox(viewwindowx, viewwindowy, scaledviewwidth, viewheight, 0);
        SB_state = -1;
    }

    R_ClearClipSegs();
    R_ClearDrawSegs();
    R_ClearPlanes();
    R_ClearSprites();

    if (automapactive && !automap_overlay)
    {
        R_RenderBSPNode(numnodes - 1);
        return;
    }

    NetUpdate();                // check for new console commands
    if (!crl_freeze)
    {
    PO_InterpolatePolyObjects(); // [crispy] Interpolate polyobjects here
    R_InterpolateTextureOffsets(); // [crispy] Smooth texture scrolling
    }

    // Make displayed player invisible locally
    if (localQuakeHappening[displayplayer] && gamestate == GS_LEVEL)
    {
        players[displayplayer].mo->flags2 |= MF2_DONTDRAW;
        R_RenderBSPNode(numnodes - 1);  // head node is the last node output
        players[displayplayer].mo->flags2 &= ~MF2_DONTDRAW;
    }
    else
    {
        R_RenderBSPNode(numnodes - 1);  // head node is the last node output
    }

    NetUpdate();                // check for new console commands
    R_DrawPlanes();
    NetUpdate();                // check for new console commands
    R_DrawMasked();
    NetUpdate();                // check for new console commands

    // [JN] Apply post-processing effects.
    V_PProc_PlayerView();
}
