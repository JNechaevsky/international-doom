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


#define _USE_MATH_DEFINES
#include <math.h>
#include "doomstat.h"
#include "p_local.h"
#include "v_video.h"
#include "st_bar.h"

#include "id_vars.h"
#include "id_func.h"


// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW 2048

// [JN] Will be false if modified PLAYPAL lump is loaded.
boolean original_playpal = true;
// [JN] Will be false if modified COLORMAP lump is loaded.
boolean original_colormap = true;

int viewangleoffset;

// increment every time a check is made
int validcount = 1;

lighttable_t *fixedcolormap;

int     centerx;
int     centery;
fixed_t centerxfrac;
fixed_t centeryfrac;
fixed_t projection;

fixed_t viewx;
fixed_t viewy;
fixed_t viewz;

angle_t     viewangle;
localview_t localview; // [crispy]

fixed_t viewcos;
fixed_t viewsin;

player_t *viewplayer;

// 0 = high, 1 = low
int detailshift;

//
// precalculated math tables
//
angle_t clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X. 
int viewangletox[FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t xtoviewangle[MAXWIDTH+1];

// [crispy] calculate the linear sky angle component here
angle_t linearskyangle[MAXWIDTH+1];

// [crispy] lookup table for horizontal screen coordinates
int  flipscreenwidth[MAXWIDTH];
int *flipviewwidth;

// [crispy] parameterized for smooth diminishing lighting
lighttable_t ***scalelight = NULL;
lighttable_t  **scalelightfixed = NULL;
lighttable_t ***zlight = NULL;

// bumped light from gun blasts
int extralight;

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
// [JN] Maximum diminished lighting index for half-brights.
int BMAPMAXDIMINDEX;
// [JN] Shifring value for glowing and flickering brightmaps.
int BMAPANIMSHIFT;


void (*colfunc) (void);
void (*basecolfunc) (void);
void (*fuzzcolfunc) (void);
void (*fuzztlcolfunc) (void);
void (*fuzzbwcolfunc) (void);
void (*transcolfunc) (void);
void (*tlcolfunc) (void);
void (*tladdcolfunc) (void);
void (*transtlfuzzcolfunc) (void);
void (*spanfunc) (void);


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

// -----------------------------------------------------------------------------
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
// [PN] Reformatted for readability and reduced nesting
// -----------------------------------------------------------------------------

static angle_t R_PointToAngleSlope (fixed_t x, fixed_t y,
                                    int(*slope_div)(unsigned int num, unsigned int den))
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

angle_t R_PointToAngle (fixed_t x, fixed_t y)
{
    return R_PointToAngleSlope (x, y, SlopeDiv);
}

// [crispy] overflow-safe R_PointToAngle() flavor
// called only from R_CheckBBox(), R_AddLine() and P_SegLengths()
angle_t R_PointToAngleCrispy (fixed_t x, fixed_t y)
{
    // [crispy] fix overflows for very long distances
    const int64_t y_viewy = (int64_t)y - viewy;
    const int64_t x_viewx = (int64_t)x - viewx;

    // [crispy] the worst that could happen is e.g. INT_MIN-INT_MAX = 2*INT_MIN
    if (x_viewx < INT_MIN || x_viewx > INT_MAX
    ||  y_viewy < INT_MIN || y_viewy > INT_MAX)
    {
        // [crispy] preserving the angle by halfing the distance in both directions
        x = x_viewx / 2 + viewx;
        y = y_viewy / 2 + viewy;
    }

    return R_PointToAngleSlope(x, y, SlopeDivCrispy);
}

angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    viewx = x1;
    viewy = y1;
    
    // [crispy] R_PointToAngle2() is never called during rendering
    return R_PointToAngleSlope (x2, y2, SlopeDiv);
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

// -----------------------------------------------------------------------------
// R_InitTextureMapping
// [JN] Replaced slow linear search with binary search, merged loops,
// precomputed values, and tightened variable scopes - boosting speed
// while keeping identical behavior.
// -----------------------------------------------------------------------------

static void R_InitTextureMapping (void)
{
    // Calc focallength 
    const fixed_t focallength = FixedDiv(centerxfrac, fovscale);
    
    // Calculate FOV
    angle_t fov;
    if (vid_fov == 90 && centerxfrac == centerxfrac_nonwide)
    {
        fov = FIELDOFVIEW;
    }
    else
    {
        const double slope = (tan(vid_fov * M_PI / 360.0)
                           * centerxfrac / centerxfrac_nonwide);
        fov = atan(slope) * FINEANGLES / M_PI;
    }

    // First pass: fill viewangletox
    const int max_x = viewwidth + 1;
    const int min_x = -1;
    const fixed_t centerxfrac_adj = centerxfrac + FRACUNIT - 1;
    
    for (int i = 0; i < FINEANGLES/2; i++)
    {
        const fixed_t tangent = finetangent[i];
        
        if (tangent > fovscale)
        {
            viewangletox[i] = 0;
        }
        else if (tangent < -fovscale)
        {
            viewangletox[i] = viewwidth;
        }
        else
        {
            const int t = (centerxfrac_adj - FixedMul(tangent, focallength)) >> FRACBITS;
            viewangletox[i] = (t < min_x) ? min_x : (t > max_x) ? max_x : t;
        }
    }

    // Second pass: build xtoviewangle using binary search
    const int linear_factor = (((SCREENWIDTH << 6) / viewwidth)
                            * (ANG90 / (NONWIDEWIDTH << 6))) / fovdiff;
    const int width_shift = viewwidth / 2;
    const int fineangles_half = FINEANGLES / 2;
    
    for (int x = 0; x <= viewwidth; x++)
    {
        int low = 0;
        int high = fineangles_half - 1;
        
        while (low <= high)
        {
            const int mid = (low + high) >> 1;
            if (viewangletox[mid] > x)
            {
                low = mid + 1;
            }
            else
            {
                high = mid - 1;
            }
        }
        
        xtoviewangle[x] = (low << ANGLETOFINESHIFT) - ANG90;
        // [crispy] calculate sky angle for drawing horizontally linear skies.
        // Taken from GZDoom and refactored for integer math.
        linearskyangle[x] = (width_shift - x) * linear_factor;
    }
    
    // Final adjustments
    clipangle = xtoviewangle[0];
    CalcMaxProjectSlope(fov);
}

// -----------------------------------------------------------------------------
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
// -----------------------------------------------------------------------------

void R_InitLightTables (void)
{
    int i;

    if (scalelight)
    {
        for (i = 0; i < LIGHTLEVELS; i++)
            free(scalelight[i]);
        free(scalelight);
    }

    if (scalelightfixed)
    {
        free(scalelightfixed);
    }

    if (zlight)
    {
        for (i = 0; i < LIGHTLEVELS; i++)
            free(zlight[i]);
        free(zlight);
    }

    // [crispy] smooth diminishing lighting
    if (vis_smooth_light)
    {
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
            BMAPMAXDIMINDEX =  47 << 3;
            BMAPANIMSHIFT  =    0 +  3;
        }
        else
        {
            // [crispy] else, use paletted approach
            LIGHTLEVELS =      16 << 1;
            LIGHTSEGSHIFT =     4 -  1;
            LIGHTBRIGHT =       1 << 1;
            MAXLIGHTSCALE =    48 << 0;
            LIGHTSCALESHIFT =  12 -  0;
            MAXLIGHTZ =       128 << 3;
            LIGHTZSHIFT =      20 -  3;
            BMAPMAXDIMINDEX =  47 << 0;
            BMAPANIMSHIFT =     0 +  0;
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
        BMAPMAXDIMINDEX =  47;
        BMAPANIMSHIFT =     0;
    }

    scalelight = malloc(LIGHTLEVELS * sizeof(*scalelight));
    scalelightfixed = malloc(MAXLIGHTSCALE * sizeof(*scalelightfixed));
    zlight = malloc(LIGHTLEVELS * sizeof(*zlight));

    // [PN] Precalculate lighting scale table before generating zlight[][]
    int *scale_table = malloc(MAXLIGHTZ * sizeof(*scale_table));
    {
        const int fracwidth = (ORIGWIDTH >> 1) * FRACUNIT;
        for (int j = 0; j < MAXLIGHTZ; ++j)
            scale_table[j] = (FixedDiv(fracwidth, (j + 1) << LIGHTZSHIFT)) >> LIGHTSCALESHIFT;
    }

    // Calculate the light levels to use for each level / distance combination.
    for (i = 0 ; i < LIGHTLEVELS ; i++)
    {
        scalelight[i] = malloc(MAXLIGHTSCALE * sizeof(**scalelight));
        zlight[i] = malloc(MAXLIGHTZ * sizeof(**zlight));
        const int start_map = ((LIGHTLEVELS - LIGHTBRIGHT - i) << 1) * NUMCOLORMAPS / LIGHTLEVELS;

        for (int j = 0; j < MAXLIGHTZ; j++)
        {
            const int scale = scale_table[j];
            int level = start_map - (scale >> 1);
    
            if (level < 0)
                level = 0;
            else if (level >= NUMCOLORMAPS)
                level = NUMCOLORMAPS - 1;
    
            zlight[i][j] = colormaps + (level << 8);
        }
    }

    // [PN] Free after zlight[][] is built
    free(scale_table);
}

// -----------------------------------------------------------------------------
// R_SetViewSize
// Do not really change anything here, because it might be in the middle
// of a refresh. The change will take effect next refresh.
// -----------------------------------------------------------------------------

boolean setsizeneeded;
int     setblocks;
int     setdetail;

void R_SetViewSize (int blocks, int detail)
{
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
}

// -----------------------------------------------------------------------------
// R_ExecuteSetViewSize
// -----------------------------------------------------------------------------

void R_ExecuteSetViewSize (void)
{
    int	    i;
    int	    j;
    fixed_t cosadj;
    double  WIDEFOVDELTA;  // [JN] FOV from DOOM Retro and Nugget Doom

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
        scaledviewwidth_nonwide = (setblocks * 32) * vid_resolution;
        viewheight = ((setblocks * 168 / 10) & ~7) * vid_resolution;

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
        fuzzcolfunc = R_DrawFuzzColumn;
        fuzztlcolfunc = R_DrawFuzzTLColumn;
        fuzzbwcolfunc = R_DrawFuzzBWColumn;
        transcolfunc = R_DrawTranslatedColumn;
        if (vid_truecolor)
        {
            tlcolfunc = R_DrawTLColumn;
            tladdcolfunc = R_DrawTLAddColumn;
            transtlfuzzcolfunc = R_DrawTransTLFuzzColumn;
        }
        else
        {
            tlcolfunc = R_DrawTLColumn_8;
            tladdcolfunc = R_DrawTLAddColumn_8;
            transtlfuzzcolfunc = R_DrawTransTLFuzzColumn_8;
        }
        spanfunc = R_DrawSpan;
    }
    else
    {
        colfunc = basecolfunc = R_DrawColumnLow;
        fuzzcolfunc = R_DrawFuzzColumnLow;
        fuzztlcolfunc = R_DrawFuzzTLColumnLow;
        fuzzbwcolfunc = R_DrawFuzzBWColumnLow;
        transcolfunc = R_DrawTranslatedColumnLow;
        if (vid_truecolor)
        {
            tlcolfunc = R_DrawTLColumnLow;
            tladdcolfunc = R_DrawTLAddColumnLow;
            transtlfuzzcolfunc = R_DrawTransTLFuzzColumnLow;
        }
        else
        {
            tlcolfunc = R_DrawTLColumnLow_8;
            tladdcolfunc = R_DrawTLAddColumnLow_8;
            transtlfuzzcolfunc = R_DrawTransTLFuzzColumnLow_8;
        }
        spanfunc = R_DrawSpanLow;
    }

    R_InitBuffer(scaledviewwidth, viewheight);

    R_InitTextureMapping();

    // psprite scales
    pspritescale = FRACUNIT * viewwidth_nonwide / ORIGWIDTH;
    pspriteiscale = FRACUNIT * ORIGWIDTH / viewwidth_nonwide;

    // thing clipping
    for (i = 0 ; i < viewwidth ; i++)
        screenheightarray[i] = viewheight;

    // planes
    {
        // [crispy] re-generate lookup-table for yslope[] (free look)
        // whenever "dp_detail_level" or "dp_screen_size" change
        // [JN] FOV from DOOM Retro and Nugget Doom
        // [PN] Optimized: moved invariant calculations out of loops for better performance
        const fixed_t half_fracunit = FRACUNIT >> 1;
        const fixed_t half_viewheight = viewheight >> 1;
        const fixed_t num = FixedMul(FixedDiv(FRACUNIT, fovscale), (viewwidth << detailshift) * half_fracunit);
        const fixed_t step = (dp_screen_size < 11 ? dp_screen_size : 11);

        for (i = 0; i < viewheight; i++)
        {
            for (j = 0; j < LOOKDIRS; j++)
            {
                const fixed_t dy = abs((i - (half_viewheight + ((j - LOOKDIRMIN) * vid_resolution) * step / 10)) << FRACBITS) + half_fracunit;
                yslopes[j][i] = FixedDiv(num, dy);
            }
        }
    }
    yslope = yslopes[LOOKDIRMIN];

    for (i = 0 ; i < viewwidth ; i++)
    {
        cosadj = abs(finecosine[xtoviewangle[i] >> ANGLETOFINESHIFT]);
        distscale[i] = FixedDiv(FRACUNIT, cosadj);
    }

    // [PN] Precalculate lighting scale table before generating scalelight[][]
    int *scale_table = malloc(MAXLIGHTSCALE * sizeof(*scale_table));
    {
        const int divisor = viewwidth_nonwide << detailshift;
        for (j = 0; j < MAXLIGHTSCALE; ++j)
            scale_table[j] = (j * NONWIDEWIDTH / divisor) >> 1;
    }

    // Calculate the light levels to use
    //  for each level / scale combination.
    for (i = 0 ; i < LIGHTLEVELS ; i++)
    {
        const int start_map = ((LIGHTLEVELS - LIGHTBRIGHT - i) << 1) * NUMCOLORMAPS / LIGHTLEVELS;

        for (j = 0 ; j < MAXLIGHTSCALE ; j++)
        {
            int level = start_map - scale_table[j];

            if (level < 0)
                level = 0;
            else if (level >= NUMCOLORMAPS)
                level = NUMCOLORMAPS - 1;

            scalelight[i][j] = colormaps + (level << 8);
        }
    }

    // [PN] Free after scalelight[][] is built
    free(scale_table);

    // [crispy] lookup table for horizontal screen coordinates
    for (i = 0, j = SCREENWIDTH - 1; i < SCREENWIDTH; i++, j--)
    {
        flipscreenwidth[i] = gp_flip_levels ? j : i;
    }

    flipviewwidth = flipscreenwidth + (gp_flip_levels ? (SCREENWIDTH - scaledviewwidth) : 0);

    // Erase old menu stuff
    R_FillBackScreen();

    // [crispy] Redraw status bar, needed for widescreen HUD
    st_fullupdate = true;
}

// -----------------------------------------------------------------------------
// R_Init
// -----------------------------------------------------------------------------

void R_Init (void)
{
    // [JN] Check for modified PLAYPAL lump.
    if (W_CheckMultipleLumps("PLAYPAL") > 1)
    {
        original_playpal = false;
    }

    // [JN] Check for modified COLORMAP lump.
    if (W_CheckMultipleLumps("COLORMAP") > 1)
    {
        original_colormap = false;
    }

    R_InitData();
    printf(".");
    // viewwidth / viewheight / dp_detail_level are set by the defaults
    R_SetViewSize(dp_screen_size, dp_detail_level);
    printf(".");
    R_InitLightTables();
    printf(".");
    R_InitSkyMap();
    printf(".");
    R_InitTranslationTables();
    printf(".");
    printf ("]");
}

// -----------------------------------------------------------------------------
// R_PointInSubsector
// -----------------------------------------------------------------------------

subsector_t *R_PointInSubsector (fixed_t x, fixed_t y)
{
    node_t *node;
    int     side;
    int     nodenum;

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

// -----------------------------------------------------------------------------
// R_SetupFrame
// -----------------------------------------------------------------------------

static void R_SetupFrame (player_t *const player)
{
    int tempCentery;
    int pitch; // [crispy]
    int tableAngle;

    viewplayer = player;

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
        // [AM] Interpolate the player camera if the feature is enabled.
        if (vid_uncapped_fps &&
            // Don't interpolate on the first tic of a level,
            // otherwise oldviewz might be garbage.
            leveltime > 1 &&
            // Don't interpolate if the player did something
            // that would necessitate turning it off for a tic.
            player->mo->interp == true &&
            // Don't interpolate during a paused state
            realleveltime > oldleveltime)
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

            pitch = LerpInt(player->oldlookdir, player->lookdir) / MLOOKUNIT;
        }
        else
        {
            viewx = player->mo->x;
            viewy = player->mo->y;
            viewz = player->viewz;
            viewangle = player->mo->angle + viewangleoffset;
            pitch = player->lookdir / MLOOKUNIT; // [crispy] pitch is actual lookdir and weapon pitch
        }
    }

    tableAngle = viewangle >> ANGLETOFINESHIFT;

    extralight = player->extralight;
    extralight += dp_level_brightness;  // [JN] Level Brightness feature.

    // RestlessRodent -- Just report it
    CRL_ReportPosition(viewx, viewy, viewz, viewangle);
    
    if (pitch > LOOKDIRMAX)
        pitch = LOOKDIRMAX;
    else
    if (pitch < -LOOKDIRMIN)
        pitch = -LOOKDIRMIN;

    // [crispy] apply new yslope[] whenever "lookdir", "detailshift" or
    // "dp_screen_size" change
    tempCentery = viewheight / 2 + (pitch * (1 * vid_resolution))
                * (dp_screen_size < 11 ? dp_screen_size : 11) / 10;
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
        fixedcolormap = colormaps
                      + player->fixedcolormap * (NUMCOLORMAPS / 32) * 256; // [crispy] smooth diminishing lighting

        walllights = scalelightfixed;

        for (int i = 0 ; i < MAXLIGHTSCALE ; i++)
            scalelightfixed[i] = fixedcolormap;
    }
    else
    {
        fixedcolormap = 0;
    }

    validcount++;
}

// -----------------------------------------------------------------------------
// R_RenderView
// -----------------------------------------------------------------------------

void R_RenderPlayerView (player_t *player)
{
    // [JN] Reset render counters.
    memset(&IDRender, 0, sizeof(IDRender));

    // Start frame
    R_SetupFrame(player);

    // [JN] Fill view buffer with black color to prevent
    // overbrighting from post-processing effects and 
    // forcefully update status bar if any effect is active.
    if (V_PProc_EffectsActive())
    {
        V_DrawFilledBox(viewwindowx, viewwindowy, scaledviewwidth, viewheight, 0);
        st_fullupdate = true;
    }

    // Clear buffers.
    R_ClearClipSegs();
    R_ClearDrawSegs();
    R_ClearPlanes();
    R_ClearSprites();

    if (automapactive && !automap_overlay)
    {
        R_RenderBSPNode(numnodes - 1);
        return;
    }

    // check for new console commands.
    NetUpdate();

    // [crispy] smooth texture scrolling
    if (!crl_freeze)
    {
        R_InterpolateTextureOffsets();
    }

    // The head node is the last node output.
    R_RenderBSPNode(numnodes - 1);

    // Check for new console commands.
    NetUpdate();

    R_DrawPlanes();

    // Check for new console commands.
    NetUpdate();

    // [crispy] draw fuzz effect independent of rendering frame rate
    R_SetFuzzPosDraw();

    R_DrawMasked();

    // Check for new console commands.
    NetUpdate();

    // [JN] Apply post-processing effects.
    V_PProc_PlayerView();
}
