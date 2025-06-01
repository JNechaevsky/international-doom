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
//
// DESCRIPTION:  the automap code
//


#include <stdio.h>
#include "deh_main.h"
#include "z_zone.h"
#include "st_bar.h"
#include "p_local.h"
#include "w_wad.h"
#include "m_controls.h"
#include "m_misc.h"
#include "i_system.h"
#include "v_video.h"
#include "doomstat.h"
#include "dstrings.h"
#include "m_menu.h"
#include "am_map.h"
#include "ct_chat.h"
#include "g_game.h"
#include "v_trans.h"

#include "id_vars.h"


//
// [JN] Make wall colors palette-independent.
//

// Common colors / original color scheme:
static int doom_0;
static int doom_64;
static int doom_96;
static int doom_99;
static int doom_104;
static int doom_112;
static int doom_176;
static int doom_184;
static int doom_209;
static int doom_231;

// BOOM color scheme:
static int boom_23;
static int boom_55;
static int boom_88;
static int boom_119;
static int boom_175;
static int boom_204;
static int boom_208;
static int boom_215;

// Remaster color scheme:
static int remaster_72;
static int remaster_81;
static int remaster_120;
static int remaster_160;
static int remaster_200;

// Jaguar color scheme:
static int jaguar_32;
static int jaguar_75;
static int jaguar_120;
static int jaguar_163;
static int jaguar_176;
static int jaguar_200;
static int jaguar_228;
static int jaguar_254;

static int exitcolors;
static int secretwallcolors;
static int foundsecretwallcolors;

// drawing stuff
#define AM_NUMMARKPOINTS 10

// [JN] FRACTOMAPBITS: overflow-safe coordinate system.
// Written by Andrey Budko (entryway), adapted from prboom-plus/src/am_map.*
#define MAPBITS 12
#define FRACTOMAPBITS (FRACBITS-MAPBITS)

// scale on entry
#define INITSCALEMTOF (.2*FRACUNIT)

// [JN] How much the automap moves window per tic in frame-buffer coordinates.
static int f_paninc;
#define F_PANINC_SLOW 4  // 140 map units in 1 second.
#define F_PANINC_FAST 8  // 280 map units in 1 second.
static int f_paninc_zoom;
#define F_PANINC_ZOOM_SLOW 8   // 280 map units in 1 second.
#define F_PANINC_ZOOM_FAST 16  // 560 map units in 1 second.

// [JN] How much zoom-in per tic goes to 2x in 1 second.
static int m_zoomin;
#define M_ZOOMIN_SLOW ((int) (1.04*FRACUNIT))
#define M_ZOOMIN_FAST ((int) (1.08*FRACUNIT))

// [JN] How much zoom-out per tic pulls out to 0.5x in 1 second.
static int m_zoomout;
#define M_ZOOMOUT_SLOW ((int) (FRACUNIT/1.04))
#define M_ZOOMOUT_FAST ((int) (FRACUNIT/1.08))

// [crispy] zoom faster with the mouse wheel
#define M2_ZOOMIN_SLOW  ((int) (1.08*FRACUNIT))
#define M2_ZOOMOUT_SLOW ((int) (FRACUNIT/1.08))
#define M2_ZOOMIN_FAST  ((int) (1.5*FRACUNIT))
#define M2_ZOOMOUT_FAST ((int) (FRACUNIT/1.5))
static int m_zoomin_mouse;
static int m_zoomout_mouse;
static boolean mousewheelzoom;

// translates between frame-buffer and map distances
#define FTOM(x) (((int64_t)((x)<<16) * scale_ftom) >> FRACBITS)
#define MTOF(x) ((((int64_t)(x) * scale_mtof) >> FRACBITS)>>16)

// translates between frame-buffer and map coordinates
#define CXMTOF(x) (f_x + MTOF((x)-m_x))
#define CYMTOF(y) (f_y + (f_h - MTOF((y)-m_y)))

// [JN] ReMood-inspired IDDT monster coloring, slightly optimized
// for uncapped framerate and uses different coloring logics:
// Active monsters: up-up-up-up
// Inactive monsters: up-down-up-down
#define IDDT_REDS_RANGE (10)
#define IDDT_REDS_MIN   (176)
#define IDDT_REDS_MAX   (176+ IDDT_REDS_RANGE)
static  int     iddt_reds_active;
static  int     iddt_reds_inactive = 176;
static  boolean iddt_reds_direction = false;
// [JN] Pulse player arrow in Spectator mode.
#define ARROW_WHITE_RANGE (10)
#define ARROW_WHITE_MIN   (80)
#define ARROW_WHITE_MAX   (96)
static  int     arrow_color = 80;
static  boolean arrow_color_direction = false;

typedef struct
{
    int x, y;
} fpoint_t;

typedef struct
{
    fpoint_t a, b;
} fline_t;

typedef struct
{
    mpoint_t a, b;
} mline_t;

#define M_ARRAY_INIT_CAPACITY 500
#include "m_array.h"

typedef struct
{
    mline_t l;
    int color;
} am_line_t;

static am_line_t *lines_1S = NULL;

// -----------------------------------------------------------------------------
// The vector graphics for the automap.
// A line drawing of the player pointing right, starting from the middle.
// -----------------------------------------------------------------------------

#define R ((8*FRACUNIT)/7)
static mline_t player_arrow[] = {
    { { -R+R/8,   0 }, {  R,      0   } }, // -----
    { {  R,       0 }, {  R-R/2,  R/4 } }, // ----->
    { {  R,       0 }, {  R-R/2, -R/4 } },
    { { -R+R/8,   0 }, { -R-R/8,  R/4 } }, // >---->
    { { -R+R/8,   0 }, { -R-R/8, -R/4 } },
    { { -R+3*R/8, 0 }, { -R+R/8,  R/4 } }, // >>--->
    { { -R+3*R/8, 0 }, { -R+R/8, -R/4 } }
};

static mline_t cheat_player_arrow[] = {
    { { -R+R/8,     0        }, {  R,           0 } }, // -----
    { {  R,         0        }, {  R-R/2,     R/4 } }, // ----->
    { {  R,         0        }, {  R-R/2,    -R/4 } },
    { { -R+R/8,     0        }, { -R-R/8,     R/4 } }, // >----->
    { { -R+R/8,     0        }, { -R-R/8,    -R/4 } },
    { { -R+3*R/8,   0        }, { -R+R/8,     R/4 } }, // >>----->
    { { -R+3*R/8,   0        }, { -R+R/8,    -R/4 } },
    { { -R/2,       0        }, { -R/2,      -R/6 } }, // >>-d--->
    { { -R/2,      -R/6      }, { -R/2+R/6,  -R/6 } },
    { { -R/2+R/6,  -R/6      }, { -R/2+R/6,   R/4 } },
    { { -R/6,       0        }, { -R/6,      -R/6 } }, // >>-dd-->
    { { -R/6,      -R/6      }, {  0,        -R/6 } },
    { {  0,        -R/6      }, {  0,         R/4 } },
    { {  R/6,       R/4      }, {  R/6,      -R/7 } }, // >>-ddt->
    { {  R/6,      -R/7      }, {  R/6+R/32, -R/7-R/32 } },
    { {  R/6+R/32, -R/7-R/32 }, {  R/6+R/10, -R/7 } }
};
#undef R

#define R (FRACUNIT)
static mline_t thintriangle_guy[] = {
    { { (fixed_t)(-.5*R), (fixed_t)(-.7*R) }, { (fixed_t)(R    ), (fixed_t)(0    ) } },
    { { (fixed_t)(R    ), (fixed_t)(0    ) }, { (fixed_t)(-.5*R), (fixed_t)(.7*R ) } },
    { { (fixed_t)(-.5*R), (fixed_t)(.7*R ) }, { (fixed_t)(-.5*R), (fixed_t)(-.7*R) } }
};
#undef R


boolean automapactive = false;

int iddt_cheating = 0;
static boolean grid = false;

// location of window on screen
static int  f_x;
static int  f_y;

// size of window on screen
static int  f_w;
static int  f_h;

#define fb I_VideoBuffer // [crispy] simplify

static mpoint_t m_paninc;     // how far the window pans each tic (map coords)
static fixed_t  mtof_zoommul; // how far the window zooms in each tic (map coords)
static fixed_t  ftom_zoommul; // how far the window zooms in each tic (fb coords)
static fixed_t  curr_mtof_zoommul; // [JN] Zooming interpolation.

static int64_t  m_x, m_y;     // LL x,y where the window is on the map (map coords)
static int64_t  m_x2, m_y2;   // UR x,y where the window is on the map (map coords)

// width/height of window on map (map coords)
static int64_t  m_w;
static int64_t  m_h;

// based on level size
static fixed_t  min_x;
static fixed_t  min_y; 
static fixed_t  max_x;
static fixed_t  max_y;

static fixed_t  max_w; // max_x-min_x,
static fixed_t  max_h; // max_y-min_y

static fixed_t  min_scale_mtof; // used to tell when to stop zooming out
static fixed_t  max_scale_mtof; // used to tell when to stop zooming in

// old stuff for recovery later
static int64_t old_m_w, old_m_h;
static int64_t old_m_x, old_m_y;

// used by MTOF to scale from map-to-frame-buffer coords
static fixed_t scale_mtof = (fixed_t)INITSCALEMTOF;
static fixed_t prev_scale_mtof = (fixed_t)INITSCALEMTOF; // [JN] Panning interpolation.
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static fixed_t scale_ftom;

static player_t *plr; // the player represented by an arrow

static patch_t *marknums[10]; // numbers used for marking by the automap
// [JN] killough 2/22/98: Remove limit on automap marks,
// and make variables external for use in savegames.
mpoint_t *markpoints = NULL;     // where the points are
int       markpointnum = 0;      // next point to be assigned (also number of points now)
int       markpointnum_max = 0;  // killough 2/22/98

static int followplayer = 1; // specifies whether to follow the player around

static boolean stopped = true;

// [crispy] Antialiased lines from Heretic with more colors
#define NUMSHADES 8
#define NUMSHADES_BITS 3 // log2(NUMSHADES)
static pixel_t color_shades[NUMSHADES * 256];

// Forward declare for AM_LevelInit
static void AM_drawFline_Vanilla(fline_t* fl, int color);
static void AM_drawFline_Smooth(fline_t* fl, int color);
// Indirect through this to avoid having to test crispy->smoothmap for every line
void (*AM_drawFline)(fline_t*, int) = AM_drawFline_Vanilla;

// [crispy/Woof!] automap rotate mode and square aspect ratio need these early on
#define ADJUST_ASPECT_RATIO (vid_aspect_ratio_correct == 1 && automap_square)
static void AM_rotate (int64_t *x, int64_t *y, angle_t a);
static void AM_transformPoint (mpoint_t *pt);
static mpoint_t mapcenter;
static angle_t mapangle;

// -----------------------------------------------------------------------------
// AM_Init
// [JN] Predefine some variables at program startup.
// -----------------------------------------------------------------------------

void AM_Init (void)
{
    // [crispy] Precalculate color lookup tables for antialiased line drawing using COLORMAP
    unsigned char *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);

    for (int color = 0; color < 256; ++color)
    {
#define REINDEX(I) (color + I * 256)
        // Pick a range of shades for a steep gradient to keep lines thin
        const int shade_index[NUMSHADES] =
        {
            REINDEX(0), REINDEX(1), REINDEX(2), REINDEX(3), REINDEX(7), REINDEX(15), REINDEX(23), REINDEX(31),
        };
#undef REINDEX
        for (int shade = 0; shade < NUMSHADES; ++shade)
        {
            color_shades[color * NUMSHADES + shade] = colormaps[shade_index[shade]];
        }
    }

    // [JN] Make wall colors palette-independent.
    if (original_playpal)
    {
        // Common colors / original color scheme
        doom_0   = 0;
        doom_64  = 64;
        doom_96  = 96;
        doom_99  = 99;
        doom_104 = 104;
        doom_112 = 112;
        doom_176 = 176;
        doom_184 = 184;
        doom_209 = 209;
        doom_231 = 231;

        // BOOM color scheme
        boom_23  = 23;
        boom_55  = 55;
        boom_88  = 88;
        boom_119 = 119;
        boom_175 = 175;
        boom_204 = 204;
        boom_208 = 208;
        boom_215 = 215;

        // Remaster color scheme
        remaster_72  = 72;
        remaster_81  = 81;
        remaster_120 = 120;
        remaster_160 = 160;
        remaster_200 = 200;

        // Jaguar color scheme
        jaguar_32 = 32;
        jaguar_75 = 75;
        jaguar_120 = 120;
        jaguar_163 = 163;
        jaguar_176 = 176;
        jaguar_200 = 200;
        jaguar_228 = 228;
        jaguar_254 = 254;

        exitcolors = 195;
        secretwallcolors = 252;
        foundsecretwallcolors = 112;
    }
    else
    {
        // Common colors / original color scheme
        doom_0   = V_GetPaletteIndex(playpal,   1,   1,   1);
        doom_64  = V_GetPaletteIndex(playpal, 191, 123,  75);
        doom_96  = V_GetPaletteIndex(playpal, 131, 131, 131);
        doom_99  = V_GetPaletteIndex(playpal, 111, 111, 111);
        doom_104 = V_GetPaletteIndex(playpal,  79,  79,  79);
        doom_112 = V_GetPaletteIndex(playpal, 119, 255, 111);
        doom_176 = V_GetPaletteIndex(playpal, 255,   1,   1);
        doom_184 = V_GetPaletteIndex(playpal, 155,   1,   1);
        doom_209 = V_GetPaletteIndex(playpal, 255, 235, 219);
        doom_231 = V_GetPaletteIndex(playpal, 255, 255,   1);

        // BOOM color scheme
        boom_23  = V_GetPaletteIndex(playpal, 211, 115, 115);
        boom_55  = V_GetPaletteIndex(playpal, 255, 187, 147);
        boom_88  = V_GetPaletteIndex(playpal, 183, 183, 183);
        
        boom_119 = V_GetPaletteIndex(playpal,  67, 147,  55);
        boom_175 = V_GetPaletteIndex(playpal, 255,  31,  31);
        boom_204 = V_GetPaletteIndex(playpal,   1,   1, 155);
        boom_208 = V_GetPaletteIndex(playpal, 255, 255, 255);
        boom_215 = V_GetPaletteIndex(playpal, 255, 127,  27);

        // Remaster color scheme
        remaster_72  = V_GetPaletteIndex(playpal, 119,  79,  43);
        remaster_81  = V_GetPaletteIndex(playpal, 231, 231, 231);
        remaster_120 = V_GetPaletteIndex(playpal,  63, 131,  47);
        remaster_160 = V_GetPaletteIndex(playpal, 255, 255, 115);
        remaster_200 = V_GetPaletteIndex(playpal,   1,   1, 255);

        // Jaguar color scheme
        jaguar_32  = V_GetPaletteIndex(playpal, 155,  51,  51);
        jaguar_75  = V_GetPaletteIndex(playpal,  83,  63,  31);
        jaguar_120 = V_GetPaletteIndex(playpal,  63, 131,  47);
        jaguar_163 = V_GetPaletteIndex(playpal, 195, 155,  47);
        jaguar_176 = V_GetPaletteIndex(playpal, 255,   0,   0);
        jaguar_200 = V_GetPaletteIndex(playpal,   0,   0, 255);
        jaguar_228 = V_GetPaletteIndex(playpal, 255, 255, 107);
        jaguar_254 = V_GetPaletteIndex(playpal, 111,   1, 107);

        exitcolors = V_GetPaletteIndex(playpal, 143, 143, 255);
        secretwallcolors = V_GetPaletteIndex(playpal, 255, 1, 255);
        foundsecretwallcolors = V_GetPaletteIndex(playpal, 119, 255, 111);
    }

    W_ReleaseLumpName("PLAYPAL");
}

// -----------------------------------------------------------------------------
// AM_activateNewScale
// Changes the map scale after zooming or translating.
// -----------------------------------------------------------------------------

static void AM_activateNewScale (void)
{
    m_x += m_w/2;
    m_y += m_h/2;
    m_w  = FTOM(f_w);
    m_h  = FTOM(f_h);
    m_x -= m_w/2;
    m_y -= m_h/2;
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

// -----------------------------------------------------------------------------
// AM_saveScaleAndLoc
// Saves the current center and zoom.
// Affects the variables that remember old scale and loc.
// -----------------------------------------------------------------------------

static void AM_saveScaleAndLoc (void)
{
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;
}

// -----------------------------------------------------------------------------
// AM_restoreScaleAndLoc
// Restores the center and zoom from locally saved values.
// Affects global variables for location and scale.
// -----------------------------------------------------------------------------

static void AM_restoreScaleAndLoc (void)
{
    m_w = old_m_w;
    m_h = old_m_h;

    if (!followplayer)
    {
        m_x = old_m_x;
        m_y = old_m_y;
    }
    else 
    {
        m_x = (plr->mo->x >> FRACTOMAPBITS) - m_w/2;
        m_y = (plr->mo->y >> FRACTOMAPBITS) - m_h/2;
    }

    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;

    // Change the scaling multipliers
    scale_mtof = FixedDiv(f_w<<FRACBITS, m_w);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

// -----------------------------------------------------------------------------
// AM_addMark
// Adds a marker at the current location.
// -----------------------------------------------------------------------------

static void AM_addMark (void)
{
    // [JN] killough 2/22/98: remove limit on automap marks
    if (markpointnum >= markpointnum_max)
    {
        markpoints = I_Realloc(markpoints,
                              (markpointnum_max = markpointnum_max ? 
                               markpointnum_max*2 : 16) * sizeof(*markpoints));
    }

    // [crispy] keep the map static in overlay mode if not following the player
    if (!followplayer)
    {
        markpoints[markpointnum].x = m_x + m_w/2;
        markpoints[markpointnum].y = m_y + m_h/2;
    }
    else
    {
        markpoints[markpointnum].x = plr->mo->x >> FRACTOMAPBITS;
        markpoints[markpointnum].y = plr->mo->y >> FRACTOMAPBITS;
    }
    markpointnum++;
}

// -----------------------------------------------------------------------------
// AM_findMinMaxBoundaries
// Determines bounding box of all vertices, 
// sets global variables controlling zoom range.
// -----------------------------------------------------------------------------

static void AM_findMinMaxBoundaries (void)
{
    int     i;
    fixed_t a, b;

    min_x = min_y =  INT_MAX;
    max_x = max_y = -INT_MAX;

    for (i = 0 ; i < numvertexes ; i++)
    {
        if (vertexes[i].x < min_x)
        {
            min_x = vertexes[i].x;
        }
        else if (vertexes[i].x > max_x)
        {
            max_x = vertexes[i].x;
        }

        if (vertexes[i].y < min_y)
        {
            min_y = vertexes[i].y;
        }
        else if (vertexes[i].y > max_y)
        {
            max_y = vertexes[i].y;
        }
    }

    // [crispy] cope with huge level dimensions which span the entire INT range
    max_w = (max_x >>= FRACTOMAPBITS) - (min_x >>= FRACTOMAPBITS);
    max_h = (max_y >>= FRACTOMAPBITS) - (min_y >>= FRACTOMAPBITS);

    a = FixedDiv(f_w<<FRACBITS, max_w);
    b = FixedDiv(f_h<<FRACBITS, max_h);

    min_scale_mtof = a < b ? a : b;
    max_scale_mtof = FixedDiv(f_h<<FRACBITS, 2*FRACUNIT);
}

// -----------------------------------------------------------------------------
// AM_changeWindowLoc
// Moves the map window by the global variables m_paninc.x, m_paninc.y
// -----------------------------------------------------------------------------

static void AM_changeWindowLoc (void)
{
    int64_t incx, incy;

    if (m_paninc.x || m_paninc.y)
    {
        followplayer = 0;
    }

    if (vid_uncapped_fps && realleveltime > oldleveltime)
    {
        // [PN] Accumulator for FPS‑independent panning
        static fixed_t prev_frac = 0;

        // [PN] Accumulate delta between frames using `fractionaltic`,
        // so pan speed stays consistent regardless of frame rate.
        // Delta may wrap around after a tic; we correct for that.
        fixed_t delta = fractionaltic - prev_frac;

        if (delta < 0)
            delta += FRACUNIT;

        incx = FixedMul(m_paninc.x, delta);
        incy = FixedMul(m_paninc.y, delta);

        prev_frac = fractionaltic;
    }
    else
    {
        incx = m_paninc.x;
        incy = m_paninc.y;
    }

    if (automap_rotate)
    {
        AM_rotate(&incx, &incy, 0 - mapangle);
    }

    // [PN] Apply frame-scaled pan delta to already-zoomed coordinates.
    // Keeps direction stable when zooming and panning simultaneously.
    m_x += incx;
    m_y += incy;

    if (m_x + m_w/2 > max_x)
    {
        m_x = max_x - m_w/2;
    }
    else if (m_x + m_w/2 < min_x)
    {
        m_x = min_x - m_w/2;
    }

    if (m_y + m_h/2 > max_y)
    {
        m_y = max_y - m_h/2;
    }
    else if (m_y + m_h/2 < min_y)
    {
        m_y = min_y - m_h/2;
    }

    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

// -----------------------------------------------------------------------------
// AM_initVariables
// -----------------------------------------------------------------------------

void AM_initVariables (void)
{
    automapactive = true;

    m_paninc.x = m_paninc.y = 0;
    ftom_zoommul = FRACUNIT;
    mtof_zoommul = FRACUNIT;
    mousewheelzoom = false; // [crispy]

    m_w = FTOM(f_w);
    m_h = FTOM(f_h);

    // [JN] Find player to center.
    plr = &players[displayplayer];

    m_x = (plr->mo->x >> FRACTOMAPBITS) - m_w/2;
    m_y = (plr->mo->y >> FRACTOMAPBITS) - m_h/2;

    AM_Ticker();
    AM_changeWindowLoc();

    // for saving & restoring
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;
}

// -----------------------------------------------------------------------------
// AM_loadPics
// -----------------------------------------------------------------------------

static void AM_loadPics (void)
{
    char namebuf[9];
  
    for (int i = 0 ; i < 10 ; i++)
    {
        DEH_snprintf(namebuf, 9, "AMMNUM%d", i);
        marknums[i] = W_CacheLumpName(namebuf, PU_STATIC);
    }

}

// -----------------------------------------------------------------------------
// AM_unloadPics
// -----------------------------------------------------------------------------

static void AM_unloadPics (void)
{
    char namebuf[9];

    for (int i = 0 ; i < 10 ; i++)
    {
        DEH_snprintf(namebuf, 9, "AMMNUM%d", i);
        W_ReleaseLumpName(namebuf);
    }
}

// -----------------------------------------------------------------------------
// AM_clearMarks
// -----------------------------------------------------------------------------

void AM_clearMarks (void)
{
    markpointnum = 0;
}

// -----------------------------------------------------------------------------
// AM_SetdrawFline 
// -----------------------------------------------------------------------------

void AM_SetdrawFline (void)
{
    AM_drawFline = automap_smooth ? AM_drawFline_Smooth : AM_drawFline_Vanilla;
}    

// -----------------------------------------------------------------------------
// AM_LevelInit
// Should be called at the start of every level.
// Right now, i figure it out myself.
// -----------------------------------------------------------------------------

void AM_LevelInit (boolean reinit)
{
    static int f_h_old;

    f_x = f_y = 0;
    f_w = SCREENWIDTH;
    f_h = SCREENHEIGHT - (ST_HEIGHT * vid_resolution);

    AM_SetdrawFline();

    AM_findMinMaxBoundaries();

    // [crispy] preserve map scale when re-initializing
    if (reinit && f_h_old)
    {
        scale_mtof = scale_mtof * f_h / f_h_old;
    }
    else
    {
        scale_mtof = FixedDiv(min_scale_mtof, (int) (0.7*FRACUNIT));
    }

    if (scale_mtof > max_scale_mtof)
    {
        scale_mtof = min_scale_mtof;
    }

    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

    f_h_old = f_h;

    // [JN] If running Deathmatch mode, mark all automap lines as mapped
    // so they will appear initially. DM mode is not about map reveal.
    if (deathmatch)
    {
        for (int i = 0 ; i < numlines ; i++)
        {
            lines[i].flags |= ML_MAPPED;
        }
    }
}

// -----------------------------------------------------------------------------
// AM_Stop
// -----------------------------------------------------------------------------

void AM_Stop (void)
{
    AM_unloadPics();
    automapactive = false;
    stopped = true;
}

// -----------------------------------------------------------------------------
// AM_Start
// -----------------------------------------------------------------------------

void AM_Start (void)
{
    static int lastlevel = -1, lastepisode = -1;

    if (!stopped)
    {
        AM_Stop();
    }

    stopped = false;

    if (lastlevel != gamemap || lastepisode != gameepisode)
    {
        AM_LevelInit(false);
        lastlevel = gamemap;
        lastepisode = gameepisode;
    }

    AM_initVariables();
    AM_loadPics();
}

// -----------------------------------------------------------------------------
// AM_minOutWindowScale
// Set the window scale to the maximum size.
// -----------------------------------------------------------------------------

static void AM_minOutWindowScale (void)
{
    scale_mtof = min_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    AM_activateNewScale();
}

// -----------------------------------------------------------------------------
// AM_maxOutWindowScale
// Set the window scale to the minimum size.
// -----------------------------------------------------------------------------

static void AM_maxOutWindowScale (void)
{
    scale_mtof = max_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    AM_activateNewScale();
}

// -----------------------------------------------------------------------------
// AM_Responder
// Handle events (user inputs) in automap mode.
// -----------------------------------------------------------------------------

boolean AM_Responder (event_t *ev)
{
    int         rc;
    static int  bigstate=0;
    static char buffer[20];
    int         key;

    // [JN] If run button is hold, pan/zoom Automap faster.    
    if (speedkeydown())
    {
        f_paninc = F_PANINC_FAST;
        f_paninc_zoom = F_PANINC_ZOOM_FAST;
        m_zoomin = M_ZOOMIN_FAST;
        m_zoomout = M_ZOOMOUT_FAST;
        m_zoomin_mouse = M2_ZOOMIN_FAST;
        m_zoomout_mouse = M2_ZOOMOUT_FAST;
    }
    else
    {
        f_paninc = F_PANINC_SLOW;
        f_paninc_zoom = F_PANINC_ZOOM_SLOW;
        m_zoomin = M_ZOOMIN_SLOW;
        m_zoomout = M_ZOOMOUT_SLOW;
        m_zoomin_mouse = M2_ZOOMIN_SLOW;
        m_zoomout_mouse = M2_ZOOMOUT_SLOW;
    }

    rc = false;

    if (ev->type == ev_joystick && joybautomap >= 0
    && (ev->data1 & (1 << joybautomap)) != 0)
    {
        joywait = I_GetTime() + 5;

        if (!automapactive)
        {
            AM_Start ();
            if (!automap_overlay)
            {
                // [JN] Redraw status bar background.
                st_fullupdate = true;
            }
        }
        else
        {
            bigstate = 0;
            AM_Stop ();
        }

        return true;
    }

    if (!automapactive)
    {
        if (ev->type == ev_keydown && ev->data1 == key_map_toggle)
        {
            AM_Start ();
            if (!automap_overlay)
            {
                // [JN] Redraw status bar background.
                st_fullupdate = true;
            }
            rc = true;
        }
    }
    // [crispy] zoom Automap with the mouse wheel
    // [JN] Mouse wheel "buttons" hardcoded.
    else if (ev->type == ev_mouse && !menuactive)
    {
        if (/*mousebmapzoomout >= 0 &&*/ ev->data1 & (1 << 4 /*mousebmapzoomout*/))
        {
            mtof_zoommul = m_zoomout_mouse;
            ftom_zoommul = m_zoomin_mouse;
            curr_mtof_zoommul = mtof_zoommul;
            mousewheelzoom = true;
            rc = true;
        }
        else
        if (/*mousebmapzoomin >= 0 &&*/ ev->data1 & (1 << 3 /*mousebmapzoomin*/))
        {
            mtof_zoommul = m_zoomin_mouse;
            ftom_zoommul = m_zoomout_mouse;
            curr_mtof_zoommul = mtof_zoommul;
            mousewheelzoom = true;
            rc = true;
        }
    }
    else if (ev->type == ev_keydown)
    {
        rc = true;
        key = ev->data1;

        if (key == key_map_east)          // pan right
        {
            if (!followplayer)
            {
                m_paninc.x = gp_flip_levels ?
                             -FTOM(f_paninc * vid_resolution) : FTOM(f_paninc * vid_resolution);
            }
            else
            {
                rc = false;
            }
        }
        else if (key == key_map_west)     // pan left
        {
            if (!followplayer)
            {
                m_paninc.x = gp_flip_levels ?
                             FTOM(f_paninc * vid_resolution) : -FTOM(f_paninc * vid_resolution);
            }
            else
            {
                rc = false;
            }
        }
        else if (key == key_map_north)    // pan up
        {
            if (!followplayer)
            {
                m_paninc.y = FTOM(f_paninc * vid_resolution);
            }
            else
            {
                rc = false;
            }
        }
        else if (key == key_map_south)    // pan down
        {
            if (!followplayer)
            {
                m_paninc.y = -FTOM(f_paninc * vid_resolution);
            }
            else
            {
                rc = false;
            }
        }
        else if (key == key_map_zoomout)  // zoom out
        {
            mtof_zoommul = m_zoomout;
            ftom_zoommul = m_zoomin;
            curr_mtof_zoommul = mtof_zoommul;
        }
        else if (key == key_map_zoomin)   // zoom in
        {
            mtof_zoommul = m_zoomin;
            ftom_zoommul = m_zoomout;
            curr_mtof_zoommul = mtof_zoommul;
        }
        else if (key == key_map_toggle)   // toggle map (tab)
        {
            bigstate = 0;
            AM_Stop ();
        }
        else if (key == key_map_maxzoom)
        {
            bigstate = !bigstate;

            if (bigstate)
            {
                AM_saveScaleAndLoc();
                AM_minOutWindowScale();
            }
            else
            {
                AM_restoreScaleAndLoc();
            }
        }
        else if (key == key_map_follow)
        {
            followplayer = !followplayer;

            CT_SetMessage(plr, DEH_String(followplayer ?
                          AMSTR_FOLLOWON : AMSTR_FOLLOWOFF), false, NULL);
        }
        else if (key == key_map_grid)
        {
            grid = !grid;

            CT_SetMessage(plr, DEH_String(grid ?
                          AMSTR_GRIDON : AMSTR_GRIDOFF), false, NULL);
        }
        else if (key == key_map_mark)
        {
            M_snprintf(buffer, sizeof(buffer), "%s %d",
                       DEH_String(AMSTR_MARKEDSPOT), markpointnum);
            CT_SetMessage(plr, buffer, false, NULL);
            AM_addMark();
        }
        else if (key == key_map_clearmark && markpointnum > 0)
        {
            // [JN] Clear all mark by holding "run" button and pressing "clear mark".
            if (speedkeydown())
            {
                AM_clearMarks();
                CT_SetMessage(plr, DEH_String(AMSTR_MARKSCLEARED), false, NULL);
            }
            else
            {
                markpointnum--;
                M_snprintf(buffer, sizeof(buffer), "%s %d",
                        DEH_String(AMSTR_MARKCLEARED), markpointnum);
                CT_SetMessage(plr, buffer, false, NULL);
            }
        }
        else if (key == key_map_rotate)
        {
            // [JN] Automap rotate mode.
            automap_rotate = !automap_rotate;
            CT_SetMessage(plr, DEH_String(automap_rotate ?
                          ID_AUTOMAPROTATE_ON : ID_AUTOMAPROTATE_OFF), false, NULL);
        }
        else if (key == key_map_overlay)
        {
            // [JN] Automap overlay mode.
            automap_overlay = !automap_overlay;
            if (automap_overlay)
            {
                CT_SetMessage(plr, DEH_String(ID_AUTOMAPOVERLAY_ON), false, NULL);
            }
            else
            {
                CT_SetMessage(plr, DEH_String(ID_AUTOMAPOVERLAY_OFF), false, NULL);
                // [JN] Redraw status bar background.
                st_fullupdate = true;
            }
        }
        else
        {
            rc = false;
        }
    }
    else if (ev->type == ev_keyup)
    {
        rc = false;
        key = ev->data1;

        if (key == key_map_east)
        {
            if (!followplayer)
            {
                m_paninc.x = 0;
            }
        }
        else if (key == key_map_west)
        {
            if (!followplayer)
            {
                m_paninc.x = 0;
            }
        }
        else if (key == key_map_north)
        {
            if (!followplayer)
            {
                m_paninc.y = 0;
            }
        }
        else if (key == key_map_south)
        {
            if (!followplayer)
            {
                m_paninc.y = 0;
            }
        }
        else if (key == key_map_zoomout || key == key_map_zoomin)
        {
            mtof_zoommul = FRACUNIT;
            ftom_zoommul = FRACUNIT;
        }
    }

    return rc;
}

// -----------------------------------------------------------------------------
// AM_changeWindowScale
// Adjusts automap zoom level smoothly based on user input or zoom state.
// -----------------------------------------------------------------------------

static void AM_changeWindowScale (void)
{
    if (vid_uncapped_fps && realleveltime > oldleveltime)
    {
        float f_paninc_smooth = (float)f_paninc_zoom / (float)FRACUNIT * (float)fractionaltic;

        if (f_paninc_smooth < 0.01f)
        {
            f_paninc_smooth = 0.01f;
        }
    
        scale_mtof = prev_scale_mtof;

        if (curr_mtof_zoommul == m_zoomin)
        {
            mtof_zoommul = ((int) ((float)FRACUNIT * (1.00f + f_paninc_smooth / 200.0f)));
            ftom_zoommul = ((int) ((float)FRACUNIT / (1.00f + f_paninc_smooth / 200.0f)));
        }
        if (curr_mtof_zoommul == m_zoomout)
        {
            mtof_zoommul = ((int) ((float)FRACUNIT / (1.00f + f_paninc_smooth / 200.0f)));
            ftom_zoommul = ((int) ((float)FRACUNIT * (1.00f + f_paninc_smooth / 200.0f)));
        }
    }

    // Change the scaling multipliers
    scale_mtof = FixedMul(scale_mtof, mtof_zoommul);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

    // [crispy] reset after zooming with the mouse wheel
    if (mousewheelzoom)
    {
        mtof_zoommul = FRACUNIT;
        ftom_zoommul = FRACUNIT;
        mousewheelzoom = false;
    }

    if (scale_mtof < min_scale_mtof)
    {
        AM_minOutWindowScale();
    }
    else if (scale_mtof > max_scale_mtof)
    {
        AM_maxOutWindowScale();
    }
    else
    {
        AM_activateNewScale();
    }
}

// -----------------------------------------------------------------------------
// AM_doFollowPlayer
// Turn on follow mode - the map scrolls opposite to player motion.
// -----------------------------------------------------------------------------

static void AM_doFollowPlayer (void)
{
    // [JN] Use interpolated player coords for smooth
    // scrolling and static player arrow position.
    // [crispy] FTOM(MTOF()) is needed to fix map line jitter in follow mode.
    if (vid_resolution > 1)
    {
        m_x = (viewx >> FRACTOMAPBITS) - m_w/2;
        m_y = (viewy >> FRACTOMAPBITS) - m_h/2;
    }
    else
    {
        m_x = FTOM(MTOF(viewx >> FRACTOMAPBITS)) - m_w/2;
        m_y = FTOM(MTOF(viewy >> FRACTOMAPBITS)) - m_h/2;
    }

    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

// -----------------------------------------------------------------------------
// AM_Ticker
// Updates automap states for each game tick, handling zoom, player arrow color
// pulsation, and IDDT mode animations.
// [PN] Optimized by reducing redundant conditions and consolidating
// color animations
// -----------------------------------------------------------------------------

void AM_Ticker (void)
{
    if (!automapactive)
    {
        return;
    }

    prev_scale_mtof = scale_mtof;

    // [JN/PN] Animate IDDT monster colors (inactive and active states):
    if (gametic & 1)
    {
        iddt_reds_inactive += iddt_reds_direction ? -1 : 1;

        if (iddt_reds_inactive == IDDT_REDS_MAX || iddt_reds_inactive == IDDT_REDS_MIN)
        {
            iddt_reds_direction = !iddt_reds_direction;
        }
    }

    iddt_reds_active = 172 + ((gametic >> 1) % IDDT_REDS_RANGE);

    // [JN/PN] Pulse player arrow in Spectator mode:
    arrow_color += arrow_color_direction ? -1 : 1;
    if (arrow_color == ARROW_WHITE_MAX || arrow_color == ARROW_WHITE_MIN)
    {
        arrow_color_direction = !arrow_color_direction;
    }
}

// -----------------------------------------------------------------------------
// AM_clearFB
// Clear automap frame buffer.
// -----------------------------------------------------------------------------

static void AM_clearFB (void)
{
    memset(I_VideoBuffer, doom_0, f_w*f_h*sizeof(*I_VideoBuffer));
}

// -----------------------------------------------------------------------------
// AM_shadeBackground
//  [JN] Shade background in overlay mode.
// -----------------------------------------------------------------------------

static void AM_shadeBackground (void)
{
    pixel_t *dest = I_VideoBuffer;
    const int shade = automap_shading;
    const int scr = (dp_screen_size > 10)
                  ? SCREENAREA
                  : SCREENWIDTH * (SCREENHEIGHT - ST_HEIGHT * vid_resolution);

    for (int i = 0; i < scr; i++)
    {
#ifndef CRISPY_TRUECOLOR
        *dest = colormaps[((automap_shading + 3) * 2) * 256 + I_VideoBuffer[y]];
#else
        *dest = I_BlendDark(*dest, I_ShadeFactor[shade]);
#endif
        ++dest;
    }
}

// -----------------------------------------------------------------------------
// AM_clipMline
// Automap clipping of lines.
//
// Based on Cohen-Sutherland clipping algorithm but with a slightly
// faster reject and precalculated slopes.  If the speed is needed,
// use a hash algorithm to handle  the common cases.
//
// [PN] Optimized by consolidating boundary checks, refining clip logic for
// edge cases, and improving readability in the clipping loop.
// -----------------------------------------------------------------------------

static boolean AM_clipMline (mline_t *ml, fline_t *fl)
{
    enum { LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8 };

    int outcode1 = 0, outcode2 = 0, outside;
    int dx, dy;
    fpoint_t tmp = { 0, 0 };

#define DOOUTCODE(oc, mx, my) \
    (oc) = 0; \
    if ((my) < f_y) (oc) |= TOP; \
    else if ((my) >= f_y + f_h) (oc) |= BOTTOM; \
    if ((mx) < f_x) (oc) |= LEFT; \
    else if ((mx) >= f_x + f_w) (oc) |= RIGHT;

    // [PN] Perform trivial rejects
    if (ml->a.y < m_y && ml->b.y < m_y) return false;
    if (ml->a.y > m_y2 && ml->b.y > m_y2) return false;
    if (ml->a.x < m_x && ml->b.x < m_x) return false;
    if (ml->a.x > m_x2 && ml->b.x > m_x2) return false;

    // [PN] Transform coordinates to frame-buffer
    fl->a.x = CXMTOF(ml->a.x);
    fl->a.y = CYMTOF(ml->a.y);
    fl->b.x = CXMTOF(ml->b.x);
    fl->b.y = CYMTOF(ml->b.y);

    DOOUTCODE(outcode1, fl->a.x, fl->a.y);
    DOOUTCODE(outcode2, fl->b.x, fl->b.y);

    // [PN] Check for full outside overlap
    if (outcode1 & outcode2) return false;

    // [PN] Clip loop
    while (outcode1 | outcode2)
    {
        outside = outcode1 ? outcode1 : outcode2;
        dx = fl->b.x - fl->a.x;
        dy = fl->b.y - fl->a.y;

        if (outside & TOP)
        {
            tmp.x = fl->a.x + (fixed_t)(((int64_t)dx * (f_y - fl->a.y)) / dy);
            tmp.y = f_y;
        }
        else if (outside & BOTTOM)
        {
            tmp.x = fl->a.x + (fixed_t)(((int64_t)dx * (f_y + f_h - 1 - fl->a.y)) / dy);
            tmp.y = f_y + f_h - 1;
        }
        else if (outside & RIGHT)
        {
            tmp.y = fl->a.y + (fixed_t)(((int64_t)dy * (f_x + f_w - 1 - fl->a.x)) / dx);
            tmp.x = f_x + f_w - 1;
        }
        else if (outside & LEFT)
        {
            tmp.y = fl->a.y + (fixed_t)(((int64_t)dy * (f_x - fl->a.x)) / dx);
            tmp.x = f_x;
        }

        if (outside == outcode1)
        {
            fl->a = tmp;
            DOOUTCODE(outcode1, fl->a.x, fl->a.y);
        }
        else
        {
            fl->b = tmp;
            DOOUTCODE(outcode2, fl->b.x, fl->b.y);
        }

        if (outcode1 & outcode2) return false;
    }

    return true;
}
#undef DOOUTCODE


#define PUTDOT_RAW(xx,yy,cc) fb[(yy) * f_w + flipscreenwidth[(xx)]] = (cc)
#ifndef CRISPY_TRUECOLOR
#define PUTDOT(xx,yy,cc) PUTDOT_RAW(xx,yy,cc)
#else
#define PUTDOT(xx,yy,cc) PUTDOT_RAW(xx,yy,pal_color[(cc)])
#endif

// -----------------------------------------------------------------------------
// PUTDOT_THICK
// [PN] Draws a "thick" pixel by filling an area around the target pixel.
// Takes the current resolution into account to determine the thickness.
// Includes boundary checks to prevent out-of-bounds access.
// [JN] With support for "user-defined" (1x...6x) and "auto" thickness.
// -----------------------------------------------------------------------------

static inline void PUTDOT_THICK (int x, int y, pixel_t color)
{
    // Cache the global setting for smooth rendering locally to avoid
    // repeated access during the loop iterations.
    const int smooth = automap_smooth;

    // If the line thickness feature is disabled, draw the dot directly
    // without performing any additional boundary checks.
    if (!automap_thick)
    {
        // Choose between smooth and regular drawing.
        if (smooth) PUTDOT_RAW(x, y, color);
        else        PUTDOT(x, y, color);
        return;
    }

    // Determine the line thickness. Auto mode uses half of the resolution.
    const int thickness = (automap_thick == 6) 
        ? vid_resolution / 2   // Auto thickness
        : automap_thick;       // User-defined thickness

    // Precalculate drawing boundaries to reduce per-pixel checks.
    int minx = x - thickness; if (minx < 0)    minx = 0;
    int maxx = x + thickness; if (maxx >= f_w) maxx = f_w - 1;
    int miny = y - thickness; if (miny < 0)    miny = 0;
    int maxy = y + thickness; if (maxy >= f_h) maxy = f_h - 1;

    // Calculate the squared thickness for distance checks.
    const int thick_sq = thickness * thickness;

    // Use a macro to handle smooth or regular drawing.
    #define PUT_PIXEL(nx, ny, c) do {         \
        if (smooth) PUTDOT_RAW(nx, ny, c);    \
        else        PUTDOT(nx, ny, c);        \
    } while (0)

    // Iterate over the bounding box and draw pixels within the circle.
    for (int nx = minx; nx <= maxx; nx++)
    {
        const int dx = nx - x;
        for (int ny = miny; ny <= maxy; ny++)
        {
            const int dy = ny - y;
            const int dist2 = dx * dx + dy * dy;

            // Draw only if the pixel lies within the desired radius.
            if (dist2 <= thick_sq)
                PUT_PIXEL(nx, ny, color);
        }
    }
    // Clean up the macro definition.
    #undef PUT_PIXEL
}

// -----------------------------------------------------------------------------
// AM_drawFline
// Classic Bresenham w/ whatever optimizations needed for speed.
// [PN] Simplified control flow, combined variable assignments, and minimized
// redundant checks for better performance and code clarity.
// -----------------------------------------------------------------------------

static void AM_drawFline_Vanilla (fline_t *fl, int color)
{
    int d;
    int x = fl->a.x, y = fl->a.y;
    int dx = fl->b.x - fl->a.x, dy = fl->b.y - fl->a.y;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    // [PN] Calculate abs(dx) and abs(dy) in one step
    int ax = sx * dx * 2, ay = sy * dy * 2;

    static int yuck = 0;

    // [PN] Debug check to exit if out of bounds
    if (x < 0 || x >= f_w || y < 0 || y >= f_h || fl->b.x < 0 || fl->b.x >= f_w || fl->b.y < 0 || fl->b.y >= f_h)
    {
        DEH_fprintf(stderr, "yuck %d\n", yuck++);
        return;
    }

    // [PN] Main loop for Bresenham's line algorithm
    if (ax > ay) // X-major case
    {
        d = ay - ax / 2;
        while (x != fl->b.x)
        {
            PUTDOT_THICK(x, y, color);
            if (d >= 0) { y += sy; d -= ax; }
            x += sx;
            d += ay;
        }
    }
    else // Y-major case
    {
        d = ax - ay / 2;
        while (y != fl->b.y)
        {
            PUTDOT_THICK(x, y, color);
            if (d >= 0) { x += sx; d -= ay; }
            y += sy;
            d += ax;
        }
    }

    PUTDOT_THICK(x, y, color); // Final point
}

// -----------------------------------------------------------------------------
// AM_drawFline_Smooth
// [crispy] Adapted from Heretic's DrawWuLine
// [PN] Optimized for readability and performance by simplifying control flow,
// reducing redundant calculations, and consolidating variable usage.
// -----------------------------------------------------------------------------

static void AM_drawFline_Smooth(fline_t* fl, int color)
{
    int X0 = fl->a.x, Y0 = fl->a.y, X1 = fl->b.x, Y1 = fl->b.y;
    const pixel_t* BaseColor = &color_shades[color * NUMSHADES];
    unsigned short ErrorAcc = 0, ErrorAdj;
    unsigned short Weighting, WeightingComplementMask = NUMSHADES - 1;
    // [PN] Declared IntensityShift with other variables
    short DeltaX, DeltaY, XDir, IntensityShift = 16 - NUMSHADES_BITS;

    /* Ensure the line runs top to bottom */
    if (Y0 > Y1)
    {
        int temp = Y0; Y0 = Y1; Y1 = temp;
        temp = X0; X0 = X1; X1 = temp;
    }

    /* Draw the initial pixel */
    PUTDOT_THICK(X0, Y0, BaseColor[0]);

    DeltaX = X1 - X0;
    DeltaY = Y1 - Y0;
    XDir = (DeltaX >= 0) ? 1 : -1;
    DeltaX = (DeltaX >= 0) ? DeltaX : -DeltaX; // [PN] Make DeltaX positive

    /* Horizontal line */
    if (DeltaY == 0)
    {
        while (DeltaX--)
        {
            X0 += XDir;
            PUTDOT_THICK(X0, Y0, BaseColor[0]);
        }
        return;
    }

    /* Vertical line */
    if (DeltaX == 0)
    {
        while (DeltaY--)
        {
            Y0++;
            PUTDOT_THICK(X0, Y0, BaseColor[0]);
        }
        return;
    }

    /* Diagonal line */
    if (DeltaX == DeltaY)
    {
        while (DeltaY--)
        {
            X0 += XDir;
            Y0++;
            PUTDOT_THICK(X0, Y0, BaseColor[0]);
        }
        return;
    }

    /* Y-major line */
    if (DeltaY > DeltaX)
    {
        // [PN] Moved ErrorAdj calculation closer to its usage
        ErrorAdj = ((unsigned int)DeltaX << 16) / (unsigned int)DeltaY;

        while (--DeltaY)
        {
            // [PN] Added ErrorAccTemp inside the loop
            unsigned short ErrorAccTemp = ErrorAcc;
            ErrorAcc += ErrorAdj;
            if (ErrorAcc <= ErrorAccTemp)
            {
                X0 += XDir;
            }
            Y0++;
            Weighting = ErrorAcc >> IntensityShift;
            PUTDOT_THICK(X0, Y0, BaseColor[Weighting]);
            PUTDOT_THICK(X0 + XDir, Y0, BaseColor[Weighting ^ WeightingComplementMask]);
        }
    }
    /* X-major line */
    else
    {
        // [PN] Moved ErrorAdj calculation closer to its usage
        ErrorAdj = ((unsigned int)DeltaY << 16) / (unsigned int)DeltaX;

        while (--DeltaX)
        {
            // [PN] Added ErrorAccTemp inside the loop
            unsigned short ErrorAccTemp = ErrorAcc;

            ErrorAcc += ErrorAdj;
            if (ErrorAcc <= ErrorAccTemp)
            {
                Y0++;
            }
            X0 += XDir;
            Weighting = ErrorAcc >> IntensityShift;
            PUTDOT_THICK(X0, Y0, BaseColor[Weighting]);
            PUTDOT_THICK(X0, Y0 + 1, BaseColor[Weighting ^ WeightingComplementMask]);
        }
    }

    /* Draw the final pixel */
    PUTDOT_THICK(X1, Y1, BaseColor[0]);
}

// -----------------------------------------------------------------------------
// AM_drawMline
// Clip lines, draw visible parts of lines.
// -----------------------------------------------------------------------------

static void AM_drawMline (mline_t *ml, int color)
{
    static fline_t fl;

    if (AM_clipMline(ml, &fl))
    {
        // draws it on frame buffer using fb coords
        AM_drawFline(&fl, color);
    }
}

// -----------------------------------------------------------------------------
// AM_drawGrid
// Draws flat (floor/ceiling tile) aligned grid lines.
// [PN] Optimized by reducing redundant calculations, consolidating boundary 
// adjustments, and minimizing conditionals in the loop.
// -----------------------------------------------------------------------------

static void AM_drawGrid (void)
{
    int64_t x, y;
    int64_t start, end;
    const fixed_t gridsize = MAPBLOCKUNITS << MAPBITS;
    mline_t ml;
    // [PN] Precomputed for boundary adjustments
    int half_w = m_w / 2;
    int half_h = m_h / 2;

    // Determine starting position for vertical lines
    start = m_x - (automap_rotate ? half_h : 0);
    start -= (start - (bmaporgx >> FRACTOMAPBITS)) % gridsize;

    end = m_x + m_w + (automap_rotate ? half_h : 0);

    // Draw vertical grid lines
    for (x = start; x < end; x += gridsize)
    {
        ml.a.x = x;
        ml.b.x = x;
        // [PN] Adjust for rotation or aspect
        ml.a.y = m_y - (automap_rotate || ADJUST_ASPECT_RATIO ? half_w : 0);
        ml.b.y = m_y + m_h + (automap_rotate || ADJUST_ASPECT_RATIO ? half_w : 0);
        
        AM_transformPoint(&ml.a);
        AM_transformPoint(&ml.b);
        AM_drawMline(&ml, doom_104);
    }

    // Determine starting position for horizontal lines
    // [PN] Adjust for rotation or aspect
    start = m_y - (automap_rotate || ADJUST_ASPECT_RATIO ? half_w : 0);
    start -= (start - (bmaporgy >> FRACTOMAPBITS)) % gridsize;

    // [PN] Adjust end for rotation or aspect
    end = m_y + m_h + (automap_rotate || ADJUST_ASPECT_RATIO ? half_w : 0);

    // Draw horizontal grid lines
    for (y = start; y < end; y += gridsize)
    {
        ml.a.y = y;
        ml.b.y = y;
        // [PN] Adjust for rotation
        ml.a.x = m_x - (automap_rotate ? half_h : 0);
        ml.b.x = m_x + m_w + (automap_rotate ? half_h : 0);
        
        AM_transformPoint(&ml.a);
        AM_transformPoint(&ml.b);
        AM_drawMline(&ml, doom_104);
    }
}

// -----------------------------------------------------------------------------
// AM_drawWalls
// Determines visible lines, draws them. 
// This is LineDef based, not LineSeg based.
// -----------------------------------------------------------------------------

static void AM_drawWalls (void)
{
    static mline_t l;

    for (int i = 0 ; i < numlines ; i++)
    {
        l.a.x = lines[i].v1->x >> FRACTOMAPBITS;
        l.a.y = lines[i].v1->y >> FRACTOMAPBITS;
        l.b.x = lines[i].v2->x >> FRACTOMAPBITS;
        l.b.y = lines[i].v2->y >> FRACTOMAPBITS;
        AM_transformPoint(&l.a);
        AM_transformPoint(&l.b);

        if (iddt_cheating || (lines[i].flags & ML_MAPPED))
        {
            if ((lines[i].flags & ML_DONTDRAW) && !iddt_cheating)
            {
                continue;
            }

            // [JN] Automap color scheme:
            switch (automap_scheme)
            {
                // BOOM
                case 1:
                {
                    if (iddt_cheating || (lines[i].flags & ML_MAPPED))
                    {
                        if (!lines[i].backsector)
                        {
                            // [JN] Highlight secret sectors
                            if (automap_secrets > 1 && lines[i].frontsector->special == 9)
                            {
                                array_push(lines_1S, ((am_line_t){l, secretwallcolors}));
                            }
                            // [plums] show revealed secrets
                            else if (automap_secrets && lines[i].frontsector->oldspecial == 9)
                            {
                                array_push(lines_1S, ((am_line_t){l, foundsecretwallcolors}));
                            }
                            else
                            {
                                array_push(lines_1S, ((am_line_t){l, boom_23}));
                            }
                        }
                        else
                        {
                            // Various teleporters
                            if (lines[i].special == 39  || lines[i].special == 97
                            ||  lines[i].special == 125 || lines[i].special == 126)
                            {
                                AM_drawMline(&l, boom_119);
                            }
                            // Secret door
                            else if (lines[i].flags & ML_SECRET)
                            {
                                AM_drawMline(&l, boom_23);      // wall color
                            }
                            // [JN] Highlight secret sectors
                            else if (automap_secrets > 1
                            && (lines[i].frontsector->special == 9
                            ||  lines[i].backsector->special == 9))
                            {
                                AM_drawMline(&l, secretwallcolors);
                            }
                            // [plums] show revealed secrets
                            else if (automap_secrets
                            && (lines[i].frontsector->oldspecial == 9
                            ||  lines[i].backsector->oldspecial == 9))
                            {
                                AM_drawMline(&l, foundsecretwallcolors);
                            }
                            // BLUE locked doors
                            else
                            if (lines[i].special == 26 || lines[i].special == 32
                            ||  lines[i].special == 99 || lines[i].special == 133)
                            {
                                AM_drawMline(&l, boom_204);
                            }
                            // RED locked doors
                            else
                            if (lines[i].special == 28  || lines[i].special == 33
                            ||  lines[i].special == 134 || lines[i].special == 135)
                            {
                                AM_drawMline(&l, boom_175);
                            }
                            // YELLOW locked doors
                            else
                            if (lines[i].special == 27  || lines[i].special == 34
                            ||  lines[i].special == 136 || lines[i].special == 137)
                            {
                                AM_drawMline(&l, doom_231);
                            }
                            // non-secret closed door
                            else
                            if (!(lines[i].flags & ML_SECRET) &&
                            ((lines[i].backsector->floorheight == lines[i].backsector->ceilingheight) ||
                            (lines[i].frontsector->floorheight == lines[i].frontsector->ceilingheight)))
                            {
                                AM_drawMline(&l, boom_208);      // non-secret closed door
                            } //jff 1/6/98 show secret sector 2S lines
                            // floor level change
                            else
                            if (lines[i].backsector->floorheight != lines[i].frontsector->floorheight)
                            {
                                AM_drawMline(&l, boom_55);
                            }
                            // ceiling level change
                            else
                            if (lines[i].backsector->ceilingheight != lines[i].frontsector->ceilingheight)
                            {
                                AM_drawMline(&l, boom_215);
                            }
                            //2S lines that appear only in IDDT
                            else if (iddt_cheating)
                            {
                                AM_drawMline(&l, boom_88);
                            }
                        }
                        // [JN] Exit (can be one-sided or two-sided)
                        if (lines[i].special == 11 || lines[i].special == 51
                        ||  lines[i].special == 52 || lines[i].special == 124)
                        {
                            array_push(lines_1S, ((am_line_t){l, exitcolors}));
                        }
                    }
                    // computermap visible lines
                    else if (plr->powers[pw_allmap])
                    {
                        // invisible flag lines do not show
                        if (!(lines[i].flags & ML_DONTDRAW))
                        {
                            if (!lines[i].backsector 
                            || lines[i].backsector->floorheight != lines[i].frontsector->floorheight
                            || lines[i].backsector->ceilingheight != lines[i].frontsector->ceilingheight)
                            {
                                AM_drawMline(&l, doom_104);
                            }
                        }
                    }
                }
                break;

                // Remaster
                case 2:
                {
                    if (iddt_cheating || (lines[i].flags & ML_MAPPED))
                    {
                        // [JN] One sided wall
                        if (!lines[i].backsector)
                        {
                            // [JN] Highlight secret sectors
                            if (automap_secrets > 1 && lines[i].frontsector->special == 9)
                            {    
                                array_push(lines_1S, ((am_line_t){l, secretwallcolors}));
                            }
                            // [plums] show revealed secrets
                            else if (automap_secrets && lines[i].frontsector->oldspecial == 9)
                            {
                                array_push(lines_1S, ((am_line_t){l, foundsecretwallcolors}));
                            }
                            else
                            {
                                array_push(lines_1S, ((am_line_t){l, doom_184}));
                            }
                        }
                        else
                        {
                            // [JN] Secret door
                            if (lines[i].flags & ML_SECRET)
                            {
                                AM_drawMline(&l, doom_184);
                            }
                            // [JN] Highlight secret sectors
                            else if (automap_secrets > 1
                            && (lines[i].frontsector->special == 9
                            ||  lines[i].backsector->special == 9))
                            {
                                AM_drawMline(&l, secretwallcolors);
                            }
                            // [plums] show revealed secrets
                            else if (automap_secrets
                            && (lines[i].frontsector->oldspecial == 9
                            ||  lines[i].backsector->oldspecial == 9))
                            {
                                AM_drawMline(&l, foundsecretwallcolors);
                            }
                            // [JN] Various Doors
                            else
                            if (lines[i].special == 1   || lines[i].special == 31
                            ||  lines[i].special == 117 || lines[i].special == 118)
                            {
                                AM_drawMline(&l, remaster_81);
                            }
                            // [JN] Various teleporters
                            else
                            if (lines[i].special == 39  || lines[i].special == 97
                            ||  lines[i].special == 125 || lines[i].special == 126)
                            {
                                AM_drawMline(&l, remaster_120);
                            }
                            // [JN] BLUE locked doors
                            else
                            if (lines[i].special == 26 || lines[i].special == 32
                            ||  lines[i].special == 99 || lines[i].special == 133)
                            {
                                AM_drawMline(&l, remaster_200);
                            }
                            // [JN] RED locked doors
                            else
                            if (lines[i].special == 28  || lines[i].special == 33
                            ||  lines[i].special == 134 || lines[i].special == 135)
                            {
                                AM_drawMline(&l, doom_176);
                            }
                            // [JN] YELLOW locked doors
                            else
                            if (lines[i].special == 27  || lines[i].special == 34
                            ||  lines[i].special == 136 || lines[i].special == 137)
                            {
                                AM_drawMline(&l, remaster_160);
                            }
                            // [JN] Floor level change
                            else if (lines[i].backsector->floorheight != lines[i].frontsector->floorheight) 
                            {
                                AM_drawMline(&l, remaster_72);
                            }
                            // [JN] Ceiling level change
                            else if (lines[i].backsector->ceilingheight != lines[i].frontsector->ceilingheight) 
                            {
                                AM_drawMline(&l, doom_64);
                            }
                            // [JN] IDDT visible lines
                            else if (iddt_cheating)
                            {
                                AM_drawMline(&l, doom_96);
                            }
                        }
                        // [JN] Exit (can be one-sided or two-sided)
                        if (lines[i].special == 11 || lines[i].special == 51
                        ||  lines[i].special == 52 || lines[i].special == 124)
                        {
                            array_push(lines_1S, ((am_line_t){l, exitcolors}));
                        }
                    }
                    // [JN] Computermap visible lines
                    else if (plr->powers[pw_allmap])
                    {
                        if (!(lines[i].flags & ML_DONTDRAW)) AM_drawMline(&l, doom_104);
                    }
                }
                break;

                // Jaguar
                case 3:
                {
                    if (iddt_cheating || (lines[i].flags & ML_MAPPED))
                    {
                        if (!lines[i].backsector)
                        {
                            // [JN] Highlight secret sectors
                            if (automap_secrets > 1 && lines[i].frontsector->special == 9)
                            {
                                array_push(lines_1S, ((am_line_t){l, secretwallcolors}));
                            }
                            // [plums] show revealed secrets
                            else if (automap_secrets && lines[i].frontsector->oldspecial == 9)
                            {
                                array_push(lines_1S, ((am_line_t){l, foundsecretwallcolors}));
                            }
                            else
                            {
                                array_push(lines_1S, ((am_line_t){l, jaguar_32}));
                            }
                        }
                        else
                        {
                            // Teleport line
                            if (lines[i].special == 39 || lines[i].special == 97)
                            {
                                AM_drawMline(&l, jaguar_120);
                            }
                            // Secret door
                            else if (lines[i].flags & ML_SECRET)
                            {
                                AM_drawMline(&l, jaguar_32);
                            }

                            // [JN] RED Key-locked doors
                            else
                            if (lines[i].special == 28  || lines[i].special == 33
                            ||  lines[i].special == 134 || lines[i].special == 135)
                            {
                                AM_drawMline(&l, jaguar_176);
                            }
                            // [JN] BLUE Key-locked doors
                            else
                            if (lines[i].special == 26  || lines[i].special == 32
                            ||  lines[i].special == 99  || lines[i].special == 133)
                            {
                                AM_drawMline(&l, jaguar_200);
                            }
                            // [JN] YELLOW Key-locked doors
                            else
                            if (lines[i].special == 27  || lines[i].special == 34
                            ||  lines[i].special == 136 || lines[i].special == 137)
                            {
                                AM_drawMline(&l, jaguar_228);
                            }
                            // [JN] Highlight secret sectors
                            else if (automap_secrets > 1
                            && (lines[i].frontsector->special == 9
                            ||  lines[i].backsector->special == 9))
                            {
                                AM_drawMline(&l, secretwallcolors);
                            }
                            // [plums] show revealed secrets
                            else if (automap_secrets
                            && (lines[i].frontsector->oldspecial == 9
                            ||  lines[i].backsector->oldspecial == 9))
                            {
                                AM_drawMline(&l, foundsecretwallcolors);
                            }
                            // Any special linedef
                            else if (lines[i].special)
                            {
                                AM_drawMline(&l, jaguar_254);
                            }
                            // Floor level change
                            else if (lines[i].backsector->floorheight != lines[i].frontsector->floorheight)
                            {
                                AM_drawMline(&l, jaguar_163);
                            }
                            // Ceiling level change
                            else if (lines[i].backsector->ceilingheight != lines[i].frontsector->ceilingheight)
                            {
                                AM_drawMline(&l, jaguar_75);
                            }
                            // Hidden gray walls
                            else if (iddt_cheating)
                            {
                                AM_drawMline(&l, doom_96);
                            }
                        }
                        // [JN] Exit (can be one-sided or two-sided)
                        if (lines[i].special == 11 || lines[i].special == 51
                        ||  lines[i].special == 52 || lines[i].special == 124)
                        {
                            array_push(lines_1S, ((am_line_t){l, exitcolors}));
                        }
                    }
                    else if (plr->powers[pw_allmap])
                    {
                        if (!(lines[i].flags & ML_DONTDRAW)) AM_drawMline(&l, doom_99);
                    }
                }
                break;

                // Original
                default:
                {
                    if (!lines[i].backsector)
                    {
                        // [JN] Mark secret sectors.
                        if (automap_secrets > 1 && lines[i].frontsector->special == 9)
                        {
                            array_push(lines_1S, ((am_line_t){l, secretwallcolors}));
                        }
                        // [plums] show revealed secrets
                        else if (automap_secrets && lines[i].frontsector->oldspecial == 9)
                        {
                            array_push(lines_1S, ((am_line_t){l, foundsecretwallcolors}));
                        }
                        else
                        {
                            array_push(lines_1S, ((am_line_t){l, doom_176}));
                        }
                    }
                    else
                    {
                        if (lines[i].special == 39)
                        { // teleporters
                            AM_drawMline(&l, doom_184);
                        }
                        else
                        if (lines[i].flags & ML_SECRET) // secret door
                        {
                            // [JN] Note: this means "don't map as two sided".
                            AM_drawMline(&l, doom_176);
                        }
                        // [JN] Mark secret sectors.
                        else
                        if (automap_secrets > 1
                        && (lines[i].frontsector->special == 9
                        ||  lines[i].backsector->special == 9))
                        {
                            AM_drawMline(&l, secretwallcolors);
                        }
                        // [plums] show revealed secrets
                        else if (automap_secrets
                        && (lines[i].frontsector->oldspecial == 9
                        ||  lines[i].backsector->oldspecial == 9))
                        {
                            AM_drawMline(&l, foundsecretwallcolors);
                        }
                        else
                        if (lines[i].backsector->floorheight
			            !=  lines[i].frontsector->floorheight)
                        {
                            AM_drawMline(&l, doom_64); // floor level change
                        }
                        else
                        if (lines[i].backsector->ceilingheight
                        !=  lines[i].frontsector->ceilingheight)
                        {
                            AM_drawMline(&l, doom_231); // ceiling level change
                        }
                        else
                        if (iddt_cheating)
                        {
                            AM_drawMline(&l, doom_96);
                        }
                    }
                }
                break;
            }
        }
        else if (plr->powers[pw_allmap])
        {
            if (!(lines[i].flags & ML_DONTDRAW))
            {
                AM_drawMline(&l, doom_99);
            }
        }
    }

    for (int i = 0; i < array_size(lines_1S); ++i)
    {
        AM_drawMline(&lines_1S[i].l, lines_1S[i].color);
    }
    array_clear(lines_1S);
}

// -----------------------------------------------------------------------------
// AM_rotate
// Rotation in 2D. Used to rotate player arrow line character.
// -----------------------------------------------------------------------------

static void AM_rotate (int64_t *x, int64_t *y, angle_t a)
{
    int64_t tmpx;

    a >>= ANGLETOFINESHIFT;

    tmpx = FixedMul(*x, finecosine[a])
         - FixedMul(*y, finesine[a]);

    *y = FixedMul(*x, finesine[a])
       + FixedMul(*y, finecosine[a]);

    *x = tmpx;
}

// -----------------------------------------------------------------------------
// AM_transformPoint
// [crispy] rotate point around map center
// adapted from prboom-plus/src/am_map.c:898-920
// [Woof!] Also, scale y coordinate of point for square aspect ratio
// [PN] Optimized by consolidating rotation calculations and simplifying logic.
// -----------------------------------------------------------------------------

static void AM_transformPoint(mpoint_t *pt)
{
    if (automap_rotate)
    {
        int64_t tmpx, tmpy;
        angle_t angle = (followplayer || !automap_overlay ? ANG90 - viewangle : mapangle) >> ANGLETOFINESHIFT;

        pt->x -= mapcenter.x;
        pt->y -= mapcenter.y;

        tmpx = FixedMul(pt->x, finecosine[angle]) - FixedMul(pt->y, finesine[angle]);
        tmpy = FixedMul(pt->x, finesine[angle]) + FixedMul(pt->y, finecosine[angle]);

        pt->x = tmpx + mapcenter.x;
        pt->y = tmpy + mapcenter.y;
    }

    // [PN] Adjust y-coordinate for aspect ratio if needed
    if (ADJUST_ASPECT_RATIO)
    {
        pt->y = mapcenter.y + (5 * (pt->y - mapcenter.y) / 6);
    }
}

// -----------------------------------------------------------------------------
// AM_drawLineCharacter
// Draws a vector graphic according to numerous parameters.
// [PN] Optimized by consolidating scaling, rotation, and translation logic
// for points a and b.
// -----------------------------------------------------------------------------

static void AM_drawLineCharacter (mline_t *lineguy, int lineguylines,
                                  fixed_t scale, angle_t angle, int color,
                                  fixed_t x, fixed_t y)
{
    int     i;
    mline_t l;

    if (automap_rotate)
    {
        angle += mapangle;
    }

    for (i = 0; i < lineguylines; i++)
    {
        // [PN] Copy and transform both points a and b
        l.a = lineguy[i].a;
        l.b = lineguy[i].b;

        // [PN] Apply scaling if specified
        if (scale)
        {
            l.a.x = FixedMul(scale, l.a.x);
            l.a.y = FixedMul(scale, l.a.y);
            l.b.x = FixedMul(scale, l.b.x);
            l.b.y = FixedMul(scale, l.b.y);
        }

        // [PN] Apply rotation if angle is specified
        if (angle)
        {
            AM_rotate(&l.a.x, &l.a.y, angle);
            AM_rotate(&l.b.x, &l.b.y, angle);
        }

        // [PN] Adjust aspect ratio if needed
        if (ADJUST_ASPECT_RATIO)
        {
            l.a.y = 5 * l.a.y / 6;
            l.b.y = 5 * l.b.y / 6;
        }

        // [PN] Translate points to final position
        l.a.x += x;
        l.a.y += y;
        l.b.x += x;
        l.b.y += y;

        // [PN] Draw the line segment
        AM_drawMline(&l, color);
    }
}

// -----------------------------------------------------------------------------
// AM_drawPlayers
// Draws the player arrow in single-player or all player arrows in netgame.
// [PN] Optimized by consolidating interpolation and transformation logic.
// -----------------------------------------------------------------------------

static void AM_drawPlayers (void)
{
    int       i;
    int       their_colors[] = { doom_112, doom_96, doom_64, doom_176 };
    int       color;
    mpoint_t  pt;
    player_t *p;
    angle_t smoothangle;

    if (!netgame)
    {
        // [JN] Smooth player arrow rotation.
        // Keep arrow static in Spectator + rotate mode.
        smoothangle = (crl_spectating && automap_rotate) ? plr->mo->angle : 
                      automap_rotate ? plr->mo->angle : viewangle;

        // [JN] Interpolate player arrow.
        pt.x = viewx >> FRACTOMAPBITS;
        pt.y = viewy >> FRACTOMAPBITS;

        // [JN] Prevent arrow jitter in non-hires mode.
        if (vid_resolution == 1)
        {
            pt.x = FTOM(MTOF(pt.x));
            pt.y = FTOM(MTOF(pt.y));
        }

        AM_transformPoint(&pt);
        AM_drawLineCharacter(cheat_player_arrow, iddt_cheating ?
                             arrlen(cheat_player_arrow) : arrlen(player_arrow),
                             0, smoothangle, crl_spectating ? arrow_color : doom_209, pt.x, pt.y);

        return;
    }

    // [PN] Draw arrows for all players in netgame
    for (i = 0; i < MAXPLAYERS; i++)
    {
        p = &players[i];
        if ((deathmatch && !singledemo && p == plr) || !playeringame[i])
            continue;

        color = p->powers[pw_invisibility] ? 246 : their_colors[i % 4];

        if (vid_uncapped_fps && realleveltime > oldleveltime)
        {
            pt.x = LerpFixed(p->mo->oldx, p->mo->x) >> FRACTOMAPBITS;
            pt.y = LerpFixed(p->mo->oldy, p->mo->y) >> FRACTOMAPBITS;
        }
        else
        {
            pt.x = p->mo->x >> FRACTOMAPBITS;
            pt.y = p->mo->y >> FRACTOMAPBITS;
        }

        // [JN] Prevent arrow jitter in non-hires mode.
        if (vid_resolution == 1)
        {
            pt.x = FTOM(MTOF(pt.x));
            pt.y = FTOM(MTOF(pt.y));
        }

        AM_transformPoint(&pt);
        smoothangle = automap_rotate ? p->mo->angle : LerpAngle(p->mo->oldangle, p->mo->angle);

        AM_drawLineCharacter(player_arrow, arrlen(player_arrow), 0, smoothangle, color, pt.x, pt.y);
    }
}

// -----------------------------------------------------------------------------
// AM_drawThings
// Draws the things on the automap in double IDDT cheat mode.
// -----------------------------------------------------------------------------

static void AM_drawThings (void)
{
    int       i;
    mpoint_t  pt;
    mobj_t   *t;
    angle_t   actualangle;
    // RestlessRodent -- Carbon copy from ReMooD
    int       color = doom_112;

    for (i = 0 ; i < numsectors ; i++)
    {
        t = sectors[i].thinglist;
        while (t)
        {
            // [JN] Use actual radius for things drawing.
            const fixed_t actualradius = t->radius >> FRACTOMAPBITS;
                
            // [crispy] do not draw an extra triangle for the player
            if (t == plr->mo)
            {
                t = t->snext;
                continue;
            }

            // [JN] Interpolate things if possible.
            if (vid_uncapped_fps && realleveltime > oldleveltime)
            {
                pt.x = LerpFixed(t->oldx, t->x) >> FRACTOMAPBITS;
                pt.y = LerpFixed(t->oldy, t->y) >> FRACTOMAPBITS;
                actualangle = LerpAngle(t->oldangle, t->angle);
            }
            else
            {
                pt.x = t->x >> FRACTOMAPBITS;
                pt.y = t->y >> FRACTOMAPBITS;
                actualangle = t->angle;
            }

            // [JN] Keep things static in Spectator + rotate mode.
            if (crl_spectating && automap_rotate)
            {
                actualangle = t->angle - mapangle - viewangle + ANG90;
            }

            AM_transformPoint(&pt);

            // [JN] IDDT extended colors:
            // [crispy] draw blood splats and puffs as small squares
            if (t->type == MT_BLOOD || t->type == MT_PUFF)
            {
                AM_drawLineCharacter(thintriangle_guy, arrlen(thintriangle_guy),
                                     actualradius >> 2, actualangle, doom_96, pt.x, pt.y);
            }
            else
            {
                // [JN] CRL - ReMooD-inspired monsters coloring.
                if (t->target && t->state && t->state->action.acv != (actionf_v)A_Look)
                {
                    color = iddt_reds_active;
                }
                else
                {
                    color = iddt_reds_inactive;
                }

                AM_drawLineCharacter(thintriangle_guy, arrlen(thintriangle_guy), 
                                     actualradius, actualangle, 
                                     // Monsters
                                     t->flags & MF_COUNTKILL ? (t->health > 0 ? color : doom_96) :
                                     // Lost Souls and Explosive barrels (does not have a MF_COUNTKILL flag)
                                     t->type == MT_SKULL || t->type == MT_BARREL ? doom_231 :
                                     // Countable items
                                     t->flags & MF_COUNTITEM ? doom_112 :
                                     // Everything else
                                     doom_96,
                                     pt.x, pt.y);
            }

            t = t->snext;
        }
    }
}

// -----------------------------------------------------------------------------
// AM_drawSpectator
// [JN] Draws player as wtite triangle while in spectator mode.
// [PN] Optimized by consolidating conditions and minimizing redundant checks.
// -----------------------------------------------------------------------------

static void AM_drawSpectator (void)
{
    int       i;
    mpoint_t  pt;
    mobj_t   *t;
    angle_t   actualangle;

    for (i = 0 ; i < numsectors ; i++)
    {
        for (t = sectors[i].thinglist; t; t = t->snext)
        {
            // [JN] Interpolate things if possible.
            if (vid_uncapped_fps && realleveltime > oldleveltime)
            {
                pt.x = LerpFixed(t->oldx, t->x) >> FRACTOMAPBITS;
                pt.y = LerpFixed(t->oldy, t->y) >> FRACTOMAPBITS;
                actualangle = LerpAngle(t->oldangle, t->angle);
            }
            else
            {
                pt.x = t->x >> FRACTOMAPBITS;
                pt.y = t->y >> FRACTOMAPBITS;
                actualangle = t->angle;
            }

            // [JN] Keep things static in Spectator + rotate mode.
            if (crl_spectating && automap_rotate)
            {
                actualangle = t->angle - mapangle - viewangle + ANG90;
            }

            AM_transformPoint(&pt);

            // [crispy] do not draw an extra triangle for the player
            if (t == plr->mo)
            {
                AM_drawLineCharacter(thintriangle_guy, arrlen(thintriangle_guy),
                                     t->radius >> FRACTOMAPBITS, actualangle,
                                     arrow_color, pt.x, pt.y);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// AM_drawMarks
// Draw the marked locations on the automap.
// [PN] Simplified boundary checks, eliminated redundant variables, and
// minimized conditional nesting for improved readability and performance.
// -----------------------------------------------------------------------------

#define MARK_W      (5)
#define MARK_FLIP_1 (1)

static void AM_drawMarks (void)
{
    int i, fx, fy, j, d;
    int fx_flip; // [crispy] support for marks drawing in flipped levels
    mpoint_t pt;

    // [JN] killough 2/22/98: remove automap mark limit
    for (i = 0; i < markpointnum; i++)
    {
        if (markpoints[i].x == -1) 
            continue;

        // [crispy] center marks around player
        pt.x = markpoints[i].x;
        pt.y = markpoints[i].y;
        AM_transformPoint(&pt);
        fx = (CXMTOF(pt.x) / vid_resolution) - 1;
        fy = (CYMTOF(pt.y) / vid_resolution) - 2;
        fx_flip = (flipscreenwidth[CXMTOF(pt.x)] / vid_resolution) - 1;
        j = i;

        do
        {
            d = j % 10;

            // killough 2/22/98: less spacing for '1'
            if (d == 1)
            {
                fx += (MARK_FLIP_1);
            }

            // [PN] Draw if within boundaries
            if (fx >= f_x && fx <= (f_w / vid_resolution) - 5
            &&  fy >= f_y && fy <= (f_h / vid_resolution) - 6)
            {
                V_DrawPatch(fx_flip - WIDESCREENDELTA, fy, marknums[d]);
            }

            // killough 2/22/98: 1 space backwards
            fx_flip -= MARK_W - (MARK_FLIP_1);
    
            j /= 10;
        } while (j > 0);
    }
}

// -----------------------------------------------------------------------------
// AM_drawCrosshair
// -----------------------------------------------------------------------------

static void AM_drawCrosshair (void)
{
    // [JN] Draw the crosshair as a graphical patch to keep it unaffected by
    // map line thickness, following the logic of mark drawing. Coloring via
    // dp_translation is still needed to ensure it won't be affected much
    // by possible custom PLAYPAL palette and modified color index.
    //
    // Patch drawing coordinates are the center of the screen:
    // x: 159 = (ORIGWIDTH  / 2) - 1
    // y:  84 = (ORIGHEIGHT / 2) - 16

    static const byte am_xhair[] =
    {
        3,0,3,0,0,0,0,0,20,0,0,0,26,0,0,0,
        34,0,0,0,1,1,90,90,90,255,0,3,90,90,90,90,
        90,255,1,1,90,90,90,255
    };

    dp_translation = cr[CR_LIGHTGRAY];
    V_DrawPatch(159, 84, (patch_t*)&am_xhair);
    dp_translation = NULL;
}

// -----------------------------------------------------------------------------
// AM_LevelNameDrawer
// -----------------------------------------------------------------------------

void AM_LevelNameDrawer (void)
{
    static char str[128];
    const int left_align = (widget_alignment ==  0) ? -WIDESCREENDELTA :      // left
                           (widget_alignment ==  1) ? 0                :      // status bar
                           (dp_screen_size    > 12  ? -WIDESCREENDELTA : 0);  // auto

    sprintf(str, "%s", level_name);
    M_WriteText(left_align, 160, str,
                // [JN] Woof and DSDA widget color scheme using yellow map name.
                widget_scheme > 2 ? cr[CR_YELLOW] : NULL);
}

// -----------------------------------------------------------------------------
// AM_Drawer
// -----------------------------------------------------------------------------

void AM_Drawer (void)
{
    if (!automapactive)
    {
        return;
    }
    
    // [JN] Moved from AM_Ticker for drawing interpolation.
    if (followplayer)
    {
        AM_doFollowPlayer();
    }

    // Change the zoom if necessary.
    // [JN] Moved from AM_Ticker for zooming interpolation.
    if (ftom_zoommul != FRACUNIT)
    {
        AM_changeWindowScale();
    }

    // Change X and Y location.
    // [JN] Moved from AM_Ticker for paning interpolation.
    if (m_paninc.x || m_paninc.y)
    {
        AM_changeWindowLoc();
    }

    // [crispy/Woof!] required for AM_transformPoint()
    if (automap_rotate || ADJUST_ASPECT_RATIO)
    {
        mapcenter.x = m_x + m_w / 2;
        mapcenter.y = m_y + m_h / 2;
        // [crispy] keep the map static in overlay mode
        // if not following the player
        if (!(!followplayer && automap_overlay))
        {
            mapangle = ANG90 - plr->mo->angle;
        }
    }

	if (!automap_overlay)
    {
		AM_clearFB();
		pspr_interp = false;  // [JN] Supress interpolated weapon bobbing.
    }

    if (automap_shading && automap_overlay)
    {
        AM_shadeBackground();
    }

    if (grid)
    {
        AM_drawGrid();
    }

    AM_drawWalls();

    AM_drawPlayers();

    if (iddt_cheating == 2)
    {
        AM_drawThings();
    }

    // [JN] CRL - draw pulsing triangle for player in Spectator mode.
    if (crl_spectating)
    {
        AM_drawSpectator();
    }

    // [JN] Do not draw in following mode.
    if (!followplayer)
    {
        AM_drawCrosshair();
    }

    AM_drawMarks();

    // [JN] Draw level name only if Level Name widget is set to "automap".
    if (!widget_levelname)
    {
        AM_LevelNameDrawer();
    }

    V_MarkRect(f_x, f_y, f_w, f_h);
}
