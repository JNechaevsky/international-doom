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
//
// DESCRIPTION:  the automap code
//


#include <stdio.h>

#include "doomdef.h"
#include "deh_str.h"
#include "i_timer.h"
#include "i_video.h"
#include "v_trans.h"
#include "m_controls.h"
#include "i_system.h"
#include "m_misc.h"
#include "p_local.h"
#include "am_map.h"
#include "ct_chat.h"
#include "doomkeys.h"
#include "v_video.h"
#include "p_action.h"

#include "id_vars.h"


const char *LevelNames[] = {
    // EPISODE 1 - THE CITY OF THE DAMNED
    "E1M1:  THE DOCKS",
    "E1M2:  THE DUNGEONS",
    "E1M3:  THE GATEHOUSE",
    "E1M4:  THE GUARD TOWER",
    "E1M5:  THE CITADEL",
    "E1M6:  THE CATHEDRAL",
    "E1M7:  THE CRYPTS",
    "E1M8:  HELL'S MAW",
    "E1M9:  THE GRAVEYARD",
    // EPISODE 2 - HELL'S MAW
    "E2M1:  THE CRATER",
    "E2M2:  THE LAVA PITS",
    "E2M3:  THE RIVER OF FIRE",
    "E2M4:  THE ICE GROTTO",
    "E2M5:  THE CATACOMBS",
    "E2M6:  THE LABYRINTH",
    "E2M7:  THE GREAT HALL",
    "E2M8:  THE PORTALS OF CHAOS",
    "E2M9:  THE GLACIER",
    // EPISODE 3 - THE DOME OF D'SPARIL
    "E3M1:  THE STOREHOUSE",
    "E3M2:  THE CESSPOOL",
    "E3M3:  THE CONFLUENCE",
    "E3M4:  THE AZURE FORTRESS",
    "E3M5:  THE OPHIDIAN LAIR",
    "E3M6:  THE HALLS OF FEAR",
    "E3M7:  THE CHASM",
    "E3M8:  D'SPARIL'S KEEP",
    "E3M9:  THE AQUIFER",
    // EPISODE 4: THE OSSUARY
    "E4M1:  CATAFALQUE",
    "E4M2:  BLOCKHOUSE",
    "E4M3:  AMBULATORY",
    "E4M4:  SEPULCHER",
    "E4M5:  GREAT STAIR",
    "E4M6:  HALLS OF THE APOSTATE",
    "E4M7:  RAMPARTS OF PERDITION",
    "E4M8:  SHATTERED BRIDGE",
    "E4M9:  MAUSOLEUM",
    // EPISODE 5: THE STAGNANT DEMESNE
    "E5M1:  OCHRE CLIFFS",
    "E5M2:  RAPIDS",
    "E5M3:  QUAY",
    "E5M4:  COURTYARD",
    "E5M5:  HYDRATYR",
    "E5M6:  COLONNADE",
    "E5M7:  FOETID MANSE",
    "E5M8:  FIELD OF JUDGEMENT",
    "E5M9:  SKEIN OF D'SPARIL",
    // EPISODE 6: unnamed
    "E6M1:  ",
    "E6M2:  ",
    "E6M3:  ",
};

// [crispy] simplify (automap framebuffer)
#define fb I_VideoBuffer

// [crispy] Used for automap background tiling and scrolling
#define MAPBGROUNDWIDTH  (ORIGWIDTH)
#define MAPBGROUNDHEIGHT (ORIGHEIGHT - 42)

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
#define IDDT_REDS_RANGE (7)
#define IDDT_REDS_MIN   (152)
#define IDDT_REDS_MAX   (IDDT_REDS_MIN + IDDT_REDS_RANGE)
static  int     iddt_reds = IDDT_REDS_MIN;
static  boolean iddt_reds_direction = false;
// [JN] Pulse player arrow in Spectator mode.
#define ARROW_WHITE_RANGE (10)
#define ARROW_WHITE_MIN   (19)
#define ARROW_WHITE_MAX   (35)
static  int     arrow_color = 19;
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

vertex_t KeyPoints[NUM_KEY_TYPES];

// -----------------------------------------------------------------------------
// The vector graphics for the automap.
// A line drawing of the player pointing right, starting from the middle.
// -----------------------------------------------------------------------------

#define R ((8*FRACUNIT)/7)

static mline_t player_arrow[] = {
    { { -R+R/4,    0 }, {      0,    0 } }, // center line.
    { { -R+R/4,  R/8 }, {      R,    0 } }, // blade
    { { -R+R/4, -R/8 }, {      R,    0 } },
    { { -R+R/4, -R/4 }, { -R+R/4,  R/4 } }, // crosspiece
    { { -R+R/8, -R/4 }, { -R+R/8,  R/4 } },
    { { -R+R/8, -R/4 }, { -R+R/4, -R/4 } }, //crosspiece connectors
    { { -R+R/8,  R/4 }, { -R+R/4,  R/4 } },
    { { -R-R/4,  R/8 }, { -R-R/4, -R/8 } }, //pommel
    { { -R-R/4,  R/8 }, { -R+R/8,  R/8 } },
    { { -R-R/4, -R/8 }, { -R+R/8, -R/8 } }
};

static mline_t keysquare[] = {
    { {      0,    0 }, {    R/4, -R/2 } },
    { {    R/4, -R/2 }, {    R/2, -R/2 } },
    { {    R/2, -R/2 }, {    R/2,  R/2 } },
    { {    R/2,  R/2 }, {    R/4,  R/2 } },
    { {    R/4,  R/2 }, {      0,    0 } }, // handle part type thing
    { {      0,    0 }, {     -R,    0 } }, // stem
    { {     -R,    0 }, {     -R, -R/2 } }, // end lockpick part
    { { -3*R/4,    0 }, { -3*R/4, -R/4 } }
};
#undef R

#define NUMPLYRLINES (sizeof(player_arrow)/sizeof(mline_t))
#define NUMKEYSQUARELINES (sizeof(keysquare)/sizeof(mline_t))

#define R (FRACUNIT)
#define NUMTRIANGLEGUYLINES (sizeof(triangle_guy)/sizeof(mline_t))
mline_t triangle_guy[] = {
  { { (fixed_t)(-.867*R), (fixed_t)(-.5*R) }, { (fixed_t)(.867*R ), (fixed_t)(-.5*R) } },
  { { (fixed_t)(.867*R ), (fixed_t)(-.5*R) }, { (fixed_t)(0      ), (fixed_t)(R    ) } },
  { { (fixed_t)(0      ), (fixed_t)(R    ) }, { (fixed_t)(-.867*R), (fixed_t)(-.5*R) } }
  };

mline_t thintriangle_guy[] = {
  { { (fixed_t)(-.5*R), (fixed_t)(-.7*R) }, { (fixed_t)(R    ), (fixed_t)(0    ) } },
  { { (fixed_t)(R    ), (fixed_t)(0    ) }, { (fixed_t)(-.5*R), (fixed_t)(.7*R ) } },
  { { (fixed_t)(-.5*R), (fixed_t)(.7*R ) }, { (fixed_t)(-.5*R), (fixed_t)(-.7*R) } }
  };
#undef R
#define NUMTHINTRIANGLEGUYLINES (sizeof(thintriangle_guy)/sizeof(mline_t))


int ravmap_cheating = 0;
static int     grid = 0;

// location of window on screen
static int  f_x;
static int  f_y;

// size of window on screen
static int  f_w;
static int  f_h;

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

#define NUMALIAS      24
#define NUMLEVELS     8
#define INTENSITYBITS 3

// [crispy] line colors for map normal mode
static byte antialias_normal[NUMALIAS][NUMLEVELS] = {
    { 96,  97,  98,  99, 100, 101, 102, 103},   // WALLCOLORS
    {110, 109, 108, 107, 106, 105, 104, 103},   // FDWALLCOLORS
    { 75,  76,  77,  78,  79,  80,  81, 103},   // CDWALLCOLORS
    { 40,  40,  41,  41,  42,  42,  43,  43},   // MLDONTDRAW1
    { 40,  40,  41,  41,  42,  42,  43,  43},   // MLDONTDRAW2
    {143, 143, 142, 142, 141, 141, 141, 141},   // YELLOWKEY
    {223, 223, 222, 222, 221, 221, 220, 220},   // GREENKEY
    {197, 197, 197, 196, 196, 196, 195, 195},   // BLUEKEY
    {173, 173, 173, 173, 173, 173, 173, 173},   // SECRETCOLORS
    {206, 206, 206, 206, 206, 206, 206, 206},   // FSECRETCOLORS (revealed secrets)
    {182, 182, 182, 182, 181, 181, 181, 181},   // EXITS
    // IDDT extended colors
    {224, 224, 223, 223, 222, 222, 221, 221},   // IDDT_GREEN
    {142, 142, 141, 141, 141, 140, 140, 140},   // IDDT_YELLOW
    {150, 150, 149, 149, 148, 148, 147, 147},   // IDDT_RED
    {151, 151, 150, 150, 149, 149, 148, 148},
    {152, 152, 151, 151, 150, 150, 149, 149},
    {153, 153, 152, 152, 151, 151, 150, 150},
    {154, 154, 153, 153, 152, 152, 151, 151},
    {155, 155, 154, 154, 153, 153, 152, 152},
    {156, 156, 155, 155, 154, 154, 153, 153},   // Used for TELEPORTERS as well
    {157, 157, 156, 156, 155, 155, 154, 154},
    {158, 158, 157, 157, 156, 156, 155, 155},
    {159, 159, 158, 158, 157, 157, 156, 156},    
};

// [crispy] line colors for map overlay mode
static byte antialias_overlay[NUMALIAS][NUMLEVELS] = {
    {100,  99,  98,  98,  97,  97,  96,  96},   // WALLCOLORS
    {106, 105, 104, 103, 102, 101, 100,  99},   // FDWALLCOLORS
    { 75,  75,  74,  74,  73,  73,  72,  72},   // CDWALLCOLORS
    { 40,  39,  39,  38,  38,  37,  37,  36},   // MLDONTDRAW1
    { 43,  42,  41,  40,  39,  38,  37,  36},   // MLDONTDRAW2
    {143, 143, 142, 142, 141, 141, 140, 140},   // YELLOWKEY
    {223, 222, 221, 220, 219, 218, 217, 216},   // GREENKEY
    {198, 198, 197, 197, 196, 196, 195, 195},   // BLUEKEY
    {175, 175, 174, 174, 173, 173, 172, 172},   // SECRETCOLORS
    {206, 206, 206, 205, 205, 205, 204, 204},   // FSECRETCOLORS
    {182, 182, 181, 181, 180, 180, 179, 179},   // EXITS
    // IDDT extended colors
    {224, 223, 222, 221, 220, 219, 218, 217},   // IDDT_GREEN
    {142, 142, 141, 141, 140, 139, 138, 137},   // IDDT_YELLOW
    {150, 150, 149, 149, 148, 148, 147, 147},   // IDDT_RED
    {151, 151, 150, 150, 149, 149, 148, 148},
    {152, 152, 151, 151, 150, 150, 149, 149},
    {153, 153, 152, 152, 151, 151, 150, 150},
    {154, 154, 153, 153, 152, 152, 151, 151},
    {155, 155, 154, 154, 153, 153, 152, 152},
    {156, 156, 155, 155, 154, 154, 153, 153},   // Used for TELEPORTERS as well
    {157, 157, 156, 156, 155, 155, 154, 154},
    {158, 158, 157, 157, 156, 156, 155, 155},
    {159, 159, 158, 158, 157, 157, 156, 156},   
};

static byte (*antialias)[NUMALIAS][NUMLEVELS]; // [crispy]
static byte *maplump;           // pointer to the raw data for the automap background.

int followplayer = 1; // specifies whether to follow the player around
// [PN] Accumulated automap pan delta from mouse movement
static int mouse_pan_x = 0;
static int mouse_pan_y = 0;

boolean automapactive = false;
static boolean stopped = true;

// [crispy/Woof!] automap rotate mode and square aspect ratio need these early on
#define ADJUST_ASPECT_RATIO (vid_aspect_ratio_correct == 1 && automap_square)
static void AM_rotate (int64_t *x, int64_t *y, angle_t a);
static void AM_transformPoint (mpoint_t *pt);
static mpoint_t mapcenter;
static angle_t mapangle;

static void DrawWuLine(fline_t* fl, byte *BaseColor);


// -----------------------------------------------------------------------------
// AM_Init
// [JN] Predefine some variables at program startup.
// -----------------------------------------------------------------------------

void AM_Init (void)
{
    char namebuf[9];

    for (int i = 0 ; i < 10 ; i++)
    {
        DEH_snprintf(namebuf, 9, "SMALLIN%d", i);
        marknums[i] = W_CacheLumpName(namebuf, PU_STATIC);
    }
    maplump = W_CacheLumpName(DEH_String("AUTOPAGE"), PU_STATIC);
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
// AM_MousePanning
//  [PN] Moves the map window by using the mouse.
// -----------------------------------------------------------------------------

static void AM_MousePanning (void)
{
    static fixed_t prev_frac = 0;

    // Compute frame delta
    fixed_t delta = (vid_uncapped_fps && realleveltime > oldleveltime)
                  ? (fractionaltic - prev_frac + FRACUNIT) & (FRACUNIT - 1)
                  : FRACUNIT;

    prev_frac = fractionaltic;

    // Interpolated movement step
    int64_t step_x = FixedMul(mouse_pan_x, delta);
    int64_t step_y = FixedMul(mouse_pan_y, delta);

    // Save original unrotated values
    const int64_t original_x = step_x;
    const int64_t original_y = step_y;

    if (!(step_x | step_y))
        return;

    const int32_t center_x = m_x + (m_w >> 1) + FTOM(step_x);
    const int32_t center_y = m_y + (m_h >> 1) + FTOM(step_y);

    // Clamp to map bounds
    if (center_x > max_x) step_x -= MTOF(center_x - max_x);
    else if (center_x < min_x) step_x += MTOF(min_x - center_x);

    if (center_y > max_y) step_y -= MTOF(center_y - max_y);
    else if (center_y < min_y) step_y += MTOF(min_y - center_y);

    // Apply pan
    m_x += FTOM(step_x);
    m_y += FTOM(step_y);

    // Remove applied portion from accumulator
    mouse_pan_x -= original_x;
    mouse_pan_y -= original_y;

    // Update extents
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

// -----------------------------------------------------------------------------
// AM_initOverlayMode
// -----------------------------------------------------------------------------

void AM_initOverlayMode (void)
{
    // [crispy] pointer to antialiased tables for line drawing
    antialias = (automap_overlay || !automap_textured_bg) ? &antialias_overlay : &antialias_normal;
}

// -----------------------------------------------------------------------------
// AM_initVariables
// -----------------------------------------------------------------------------

void AM_initVariables (void)
{
    const thinker_t *think;
    const mobj_t *mo;

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

    // load in the location of keys, if in baby mode

    memset(KeyPoints, 0, sizeof(vertex_t) * 3);
    if (gameskill == sk_baby)
    {
        for (think = thinkercap.next; think != &thinkercap;
             think = think->next)
        {
            if (think->function != P_MobjThinker)
            {                   //not a mobj
                continue;
            }
            mo = (mobj_t *) think;
            if (mo->type == MT_CKEY)
            {
                KeyPoints[0].x = mo->x;
                KeyPoints[0].y = mo->y;
            }
            else if (mo->type == MT_AKYY)
            {
                KeyPoints[1].x = mo->x;
                KeyPoints[1].y = mo->y;
            }
            else if (mo->type == MT_BKYY)
            {
                KeyPoints[2].x = mo->x;
                KeyPoints[2].y = mo->y;
            }
        }
    }

    AM_initOverlayMode();
}

// -----------------------------------------------------------------------------
// AM_ClearMarks
// -----------------------------------------------------------------------------

void AM_ClearMarks (void)
{
    markpointnum = 0;
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
    f_h = SCREENHEIGHT - SBARHEIGHT;

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

    if (gamestate != GS_LEVEL)
    {
        return;                 // don't show automap if we aren't in a game!
    }
    if (lastlevel != gamemap || lastepisode != gameepisode)
    {
        AM_LevelInit(false);
        lastlevel = gamemap;
        lastepisode = gameepisode;
    }

    AM_initVariables();
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

boolean AM_Responder (const event_t *ev)
{
    int         rc;
    static int  bigstate=0;
    static char buffer[20];
    static int  joywait = 0;
    int         key;

    key = ev->data1;

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
    && (ev->data1 & (1 << joybautomap)) != 0 && joywait < I_GetTime())
    {
        joywait = I_GetTime() + 5;

        if (!automapactive)
        {
            AM_Start ();
            if (!automap_overlay)
            {
                // [JN] Redraw status bar background.
                SB_state = -1;
            }
        }
        else
        {
            bigstate = 0;
            AM_Stop ();
        }
    }

    if (!automapactive)
    {
        if (ev->type == ev_keydown && key == key_map_toggle
         && gamestate == GS_LEVEL)
        {
            AM_Start ();
            if (!automap_overlay)
            {
                // [JN] Redraw status bar background.
                SB_state = -1;
            }
            rc = true;
        }
    }
    // [crispy] zoom Automap with the mouse wheel
    // [JN] Mouse wheel "buttons" hardcoded.
    else if (ev->type == ev_mouse && !MenuActive && !askforquit)
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
        else // [PN] Move the map window by using the mouse
        if (!followplayer && automap_mouse_pan && (ev->data2 || ev->data3))
        {
            int dx = ev->data2;
            int dy = ev->data3;

            // Invert horizontal movement if the level is flipped
            if (gp_flip_levels)
                dx = -dx;

            // Rotate pan direction if automap is in rotate mode
            if (automap_rotate)
            {
                int64_t incx = dx;
                int64_t incy = dy;
                AM_rotate(&incx, &incy, 0 - mapangle);
                dx = (int)incx;
                dy = (int)incy;
            }

            // Accumulate mouse movement into pan buffer,
            // scaled by resolution and sensitivity.
            // The >> 5 keeps movement smooth across wide FPS ranges and DPI setups.
            mouse_pan_x += (dx * vid_resolution * mouseSensitivity) >> 5;
            mouse_pan_y += (dy * vid_resolution * mouse_sensitivity_y) >> 5;

            rc = true;
        }
    }
    else if (ev->type == ev_keydown)
    {
        rc = true;

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
                AM_ClearMarks();
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
            CT_SetMessage(plr, DEH_String(automap_overlay ? ID_AUTOMAPOVERLAY_ON :
                                                            ID_AUTOMAPOVERLAY_OFF), false, NULL);
            // [JN] Redraw status bar background.
            SB_state = -1;
            AM_initOverlayMode();
        }
        else if (key == key_map_mousepan)
        {
            // [PN] Mouse panning mode.
            automap_mouse_pan = !automap_mouse_pan;
            CT_SetMessage(plr, DEH_String(automap_mouse_pan ?
                          ID_AUTOMAPMOUSEPAN_ON : ID_AUTOMAPMOUSEPAN_OFF), false, NULL);

        }
        else
        {
            rc = false;
        }
    }
    else if (ev->type == ev_keyup)
    {
        rc = false;

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
// Automap zooming.
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
        if (iddt_reds_direction)
        {
            // Darkening
            if (--iddt_reds == IDDT_REDS_MIN)
            {
                iddt_reds_direction = false;
            }
        }
        else
        {
            // Brightening
            if (++iddt_reds == IDDT_REDS_MAX)
            {
                iddt_reds_direction = true;
            }
        }
    }

    // [JN/PN] Pulse player arrow in Spectator mode:
    arrow_color += arrow_color_direction ? -1 : 1;
    if (arrow_color == ARROW_WHITE_MAX || arrow_color == ARROW_WHITE_MIN)
    {
        arrow_color_direction = !arrow_color_direction;
    }
}

// -----------------------------------------------------------------------------
// AM_drawBackground
//  [PN] Unified background drawing: smoothly scrolls with map movement when
//  automap_rotate is off, and freezes the offset when automap_rotate is on
//  to avoid visual jump of background drawing.
// -----------------------------------------------------------------------------

static void AM_drawBackground (void)
{
    if (automap_textured_bg)
    {
    pixel_t *restrict dest = I_VideoBuffer;
    const byte *restrict src = maplump;
    static int bg_xoffs = 0;
    static int bg_yoffs = 0;

    // [PN] Update background offsets only when automap_rotate is disabled
    if (!automap_rotate && automap_scroll_bg)
    {
        bg_xoffs = (MTOF(m_x) / 4) % MAPBGROUNDWIDTH;
        bg_yoffs = (MTOF(m_y) / 8) % MAPBGROUNDHEIGHT;
        if (bg_xoffs < 0) bg_xoffs += MAPBGROUNDWIDTH;
        if (bg_yoffs < 0) bg_yoffs += MAPBGROUNDHEIGHT;
    }

    for (int y = 0; y < SCREENHEIGHT - SBARHEIGHT; y++)
    {
        const int ysrc = (y + bg_yoffs) % MAPBGROUNDHEIGHT;
        const byte *restrict row = src + ysrc * MAPBGROUNDWIDTH;

        for (int x = 0; x < SCREENWIDTH; x++)
        {
            const int xsrc = (x + bg_xoffs) % MAPBGROUNDWIDTH;
            dest[y * SCREENWIDTH + x] = pal_color[row[xsrc]];
        }
    }
    }
    else
    {
    memset(I_VideoBuffer, 0, f_w*f_h*sizeof(*I_VideoBuffer));
    }
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
                  : SCREENWIDTH * (SCREENHEIGHT - SBARHEIGHT);

    for (int i = 0; i < scr; i++)
    {
        *dest = I_BlendDark(*dest, I_ShadeFactor[shade]);
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

#define DOT(xx,yy,cc) fb[(yy)*f_w+(flipscreenwidth[xx])]=(pal_color[cc])    //the MACRO!

// -----------------------------------------------------------------------------
// PUTDOT_THICK
// [PN] Draws a "thick" pixel by filling an area around the target pixel.
// Takes the current resolution into account to determine the thickness.
// Includes boundary checks to prevent out-of-bounds access.
// [JN] With support for "user-defined" (1x...6x) and "auto" thickness.
// -----------------------------------------------------------------------------

static inline void PUTDOT_THICK(int x, int y, pixel_t color)
{
    // Determine the line thickness.
    const int thickness = automap_thick == 6
                        ? vid_resolution / 2  // Auto thickness
                        : automap_thick;      // User-defined thickness

    // Precompute valid bounding box to reduce per-pixel checks.
    int minx = x - thickness; if (minx < 0)    minx = 0;
    int maxx = x + thickness; if (maxx >= f_w) maxx = f_w - 1;
    int miny = y - thickness; if (miny < 0)    miny = 0;
    int maxy = y + thickness; if (maxy >= f_h) maxy = f_h - 1;

    // Precompute squared thickness for distance checks.
    const int thick_sq = thickness * thickness;

    // Fill pixels within the circle defined by 'thickness'.
    for (int nx = minx; nx <= maxx; nx++)
    {
        const int dx = nx - x;
        for (int ny = miny; ny <= maxy; ny++)
        {
            const int dy = ny - y;
            const int dist2 = dx * dx + dy * dy;

            if (dist2 <= thick_sq)
            {
                DOT(nx, ny, color);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// AM_drawFline
// Classic Bresenham w/ whatever optimizations needed for speed.
// [PN] Refactored to handle both antialiased and non-antialiased line drawing.
// -----------------------------------------------------------------------------

static void AM_drawFline(fline_t * fl, int color)
{
    int actual_color = 0;

    if (automap_smooth_hr)
    {
        // Use antialiasing if enabled
        switch (color)
        {
            case WALLCOLORS:    DrawWuLine(fl, &(*antialias)[0][0]);  return;
            case FDWALLCOLORS:  DrawWuLine(fl, &(*antialias)[1][0]);  return;
            case CDWALLCOLORS:  DrawWuLine(fl, &(*antialias)[2][0]);  return;
            // [JN] Apply antialiasing for some extra lines as well:
            case MLDONTDRAW1:   DrawWuLine(fl, &(*antialias)[3][0]);  return;
            case MLDONTDRAW2:   DrawWuLine(fl, &(*antialias)[4][0]);  return;
            case YELLOWKEY:     DrawWuLine(fl, &(*antialias)[5][0]);  return;
            case GREENKEY:      DrawWuLine(fl, &(*antialias)[6][0]);  return;
            case BLUEKEY:       DrawWuLine(fl, &(*antialias)[7][0]);  return;
            case SECRETCOLORS:  DrawWuLine(fl, &(*antialias)[8][0]);  return;
            case FSECRETCOLORS: DrawWuLine(fl, &(*antialias)[9][0]);  return;
            case EXITS:         DrawWuLine(fl, &(*antialias)[10][0]); return;
            // IDDT extended colors:
            case IDDT_GREEN:    DrawWuLine(fl, &(*antialias)[11][0]); return;
            case IDDT_YELLOW:   DrawWuLine(fl, &(*antialias)[12][0]); return;
            case 150:           DrawWuLine(fl, &(*antialias)[13][0]); return;
            case 151:           DrawWuLine(fl, &(*antialias)[14][0]); return;
            case 152:           DrawWuLine(fl, &(*antialias)[15][0]); return;
            case 153:           DrawWuLine(fl, &(*antialias)[16][0]); return;
            case 154:           DrawWuLine(fl, &(*antialias)[17][0]); return;
            case 155:           DrawWuLine(fl, &(*antialias)[18][0]); return;
            case 156:           DrawWuLine(fl, &(*antialias)[19][0]); return;  // Used for TELEPORTERS as well
            case 157:           DrawWuLine(fl, &(*antialias)[20][0]); return;
            case 158:           DrawWuLine(fl, &(*antialias)[21][0]); return;
            case 159:           DrawWuLine(fl, &(*antialias)[22][0]); return;
            default: break;
        }
    }
    else
    {
        // No antialiasing: map colors
        switch (color)
        {
            case WALLCOLORS:    actual_color = automap_overlay ? 100 : 96;  break;
            case FDWALLCOLORS:  actual_color = automap_overlay ? 106 : 110; break;
            case CDWALLCOLORS:  actual_color = 75; break;
            default: actual_color = color; break;
        }
    }

    // Debugging: check bounds
    if (fl->a.x < 0 || fl->a.x >= f_w || fl->a.y < 0 || fl->a.y >= f_h
    ||  fl->b.x < 0 || fl->b.x >= f_w || fl->b.y < 0 || fl->b.y >= f_h)
    {
        return;
    }

    // Bresenham's line algorithm
    const int dx = fl->b.x - fl->a.x;
    const int ax = 2 * (dx < 0 ? -dx : dx);
    const int sx = dx < 0 ? -1 : 1;

    const int dy = fl->b.y - fl->a.y;
    const int ay = 2 * (dy < 0 ? -dy : dy);
    const int sy = dy < 0 ? -1 : 1;

    int x = fl->a.x;
    int y = fl->a.y;

    if (ax > ay)
    {
        int d = ay - ax / 2;
        while (1)
        {
            PUTDOT_THICK(x, y, automap_smooth_hr ? color : actual_color);
            if (x == fl->b.x)
                return;
            if (d >= 0)
            {
                y += sy;
                d -= ax;
            }
            x += sx;
            d += ay;
        }
    }
    else
    {
        int d = ax - ay / 2;
        while (1)
        {
            PUTDOT_THICK(x, y, automap_smooth_hr ? color : actual_color);
            if (y == fl->b.y)
                return;
            if (d >= 0)
            {
                x += sx;
                d -= ay;
            }
            y += sy;
            d += ax;
        }
    }
}

// -----------------------------------------------------------------------------
// DrawWuLine
// [PN] Optimized and simplified DrawWuLine:
// - Combined common operations and removed redundant code to reduce duplication.
// - Simplified direction handling and error accumulation for better readability.
// - Maintained the original functionality while making the code more compact.
// -----------------------------------------------------------------------------

static void DrawWuLine (fline_t *fl, byte *BaseColor)
{
    int X0 = fl->a.x, Y0 = fl->a.y, X1 = fl->b.x, Y1 = fl->b.y;
    unsigned short IntensityShift, ErrorAdj, ErrorAcc;
    unsigned short ErrorAccTemp, Weighting, WeightingComplementMask;
    short DeltaX, DeltaY, Temp, XDir;

    /* Make sure the line runs top to bottom */
    if (Y0 > Y1)
    {
        Temp = Y0; Y0 = Y1; Y1 = Temp;
        Temp = X0; X0 = X1; X1 = Temp;
    }

    /* Draw the initial pixel, which is always exactly intersected by
       the line and so needs no weighting */
    PUTDOT_THICK(X0, Y0, BaseColor[0]);

    DeltaX = X1 - X0;
    DeltaY = Y1 - Y0;
    XDir = (DeltaX >= 0) ? 1 : -1;
    DeltaX = (DeltaX >= 0) ? DeltaX : -DeltaX;

    /* Special-case horizontal, vertical, and diagonal lines, which
       require no weighting because they go right through the center of
       every pixel */
    if (DeltaY == 0)
    {
        /* Horizontal line */
        while (DeltaX--)
        {
            X0 += XDir;
            PUTDOT_THICK(X0, Y0, BaseColor[0]);
        }
        return;
    }
    if (DeltaX == 0)
    {
        /* Vertical line */
        while (DeltaY--)
        {
            Y0++;
            PUTDOT_THICK(X0, Y0, BaseColor[0]);
        }
        return;
    }
    if (DeltaX == DeltaY)
    {
        /* Diagonal line */
        while (DeltaY--)
        {
            X0 += XDir;
            Y0++;
            PUTDOT_THICK(X0, Y0, BaseColor[0]);
        }
        return;
    }

    /* General case */
    ErrorAcc = 0;
    IntensityShift = 16 - INTENSITYBITS;
    WeightingComplementMask = NUMLEVELS - 1;

    if (DeltaY > DeltaX)
    {
        /* Y-major line */
        ErrorAdj = ((unsigned int) DeltaX << 16) / (unsigned int) DeltaY;
        while (--DeltaY)
        {
            ErrorAccTemp = ErrorAcc;
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
    else
    {
        /* X-major line */
        ErrorAdj = ((unsigned int) DeltaY << 16) / (unsigned int) DeltaX;
        while (--DeltaX)
        {
            ErrorAccTemp = ErrorAcc;
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
        AM_drawMline(&ml, GRIDCOLORS);
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
        AM_drawMline(&ml, GRIDCOLORS);
    }
}

// -----------------------------------------------------------------------------
// AM_drawWalls
// Determines visible lines, draws them. 
// This is LineDef based, not LineSeg based.
// -----------------------------------------------------------------------------

#define LINESECRETSECTOR_1S automap_secrets > 1 && lines[i].frontsector->special == 9
#define LINESECRETSECTOR_2S automap_secrets > 1 && (lines[i].frontsector->special == 9 || lines[i].backsector->special == 9)
#define LINEFOUNDSECRETSECTOR_1S automap_secrets && lines[i].frontsector->oldspecial == 9
#define LINEFOUNDSECRETSECTOR_2S automap_secrets && (lines[i].frontsector->oldspecial == 9 || lines[i].backsector->oldspecial == 9)

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

        if (ravmap_cheating || (lines[i].flags & ML_MAPPED))
        {
            if ((lines[i].flags & ML_DONTDRAW) && !ravmap_cheating)
                continue;
            if (!lines[i].backsector)
            {
                // [JN] Colorize 1-sided exits with azure.
                if (lines[i].special == 11 || lines[i].special == 51)
                {
                    array_push(lines_1S, ((am_line_t){l, EXITS}));
                }
                // [JN] Mark secret sectors.
                else
                if (LINESECRETSECTOR_1S)
                {
                    array_push(lines_1S, ((am_line_t){l, SECRETCOLORS}));
                }
                else
                if (LINEFOUNDSECRETSECTOR_1S)
                {
                    array_push(lines_1S, ((am_line_t){l, FSECRETCOLORS}));
                }
                else
                {
                    array_push(lines_1S, ((am_line_t){l, WALLCOLORS}));
                }
            }
            else
            {
                if (lines[i].flags & ML_SECRET)    // secret door
                {
                     // [plums] secret sectors still get colored when option is on
                    if (LINESECRETSECTOR_2S)
                        AM_drawMline(&l, SECRETCOLORS);
                    else
                    if (LINEFOUNDSECRETSECTOR_2S)
                        AM_drawMline(&l, FSECRETCOLORS);
                    else
                    if (ravmap_cheating)
                        AM_drawMline(&l, 0);
                    else
                        AM_drawMline(&l, WALLCOLORS);
                }
                else
                {
                // [plums] check only colored door types, to not interfere
                // with secret sector marking of types 28-31.
                if ((lines[i].special > 25 && lines[i].special < 29)
                ||  (lines[i].special > 31 && lines[i].special < 35))
                {
                    switch (lines[i].special)
                    {
                        case 26:
                        case 32:
                            AM_drawMline(&l, BLUEKEY);
                            break;
                        case 27:
                        case 34:
                            AM_drawMline(&l, YELLOWKEY);
                            break;
                        case 28:
                        case 33:
                            AM_drawMline(&l, GREENKEY);
                            break;
                        default:
                            break;
                    }
                }
                // [JN] Colorize teleporters with red.
                else
                if (lines[i].special == 39
                ||  lines[i].special == 97)
                {
                    AM_drawMline(&l, TELEPORTERS);
                }
                // [JN] Colorize 2-sided exits with azure.
                // Switches can still be 2-sided.
                else
                if (lines[i].special == 52 || lines[i].special == 105
                ||  lines[i].special == 11 || lines[i].special == 51)
                {
                    AM_drawMline(&l, EXITS);
                }
                // [JN] Mark secret sectors.
                else if (LINESECRETSECTOR_2S)
                {
                    AM_drawMline(&l, SECRETCOLORS);
                }
                else if (LINEFOUNDSECRETSECTOR_2S)
                {
                    AM_drawMline(&l, FSECRETCOLORS);
                }
                else if (lines[i].backsector->floorheight
                         != lines[i].frontsector->floorheight)
                {
                    AM_drawMline(&l, FDWALLCOLORS);  // floor level change
                }
                else if (lines[i].backsector->ceilingheight
                         != lines[i].frontsector->ceilingheight)
                {
                    AM_drawMline(&l, CDWALLCOLORS);  // ceiling level change
                }
                else if (ravmap_cheating)
                {
                    AM_drawMline(&l, MLDONTDRAW1);
                }
                }
            }
        }
        else if (plr->powers[pw_allmap])
        {
            if (!(lines[i].flags & ML_DONTDRAW))
            {
                AM_drawMline(&l, MLDONTDRAW2);
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

static void AM_transformPoint (mpoint_t *pt)
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
    int i;
    mpoint_t  pt;
    player_t *p;
    static int their_colors[] = { PL_GREEN, PL_YELLOW, PL_RED, PL_BLUE };
    int color;
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
        AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0, smoothangle,
                             crl_spectating ? arrow_color : PL_WHITE, pt.x, pt.y);
        return;
    }

    // [PN] Draw arrows for all players in netgame
    for (i = 0 ; i < MAXPLAYERS ; i++)
    {
        p = &players[i];
        if ((deathmatch && !singledemo && p == plr) || !playeringame[i])
            continue;

        color = p->powers[pw_invisibility] ? 102 : their_colors[i % 4];

        // [JN] Interpolate other player arrows.
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

        AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0,
                             smoothangle, color, pt.x, pt.y);
    }
}

// -----------------------------------------------------------------------------
// AM_drawThings
// Draws the things on the automap in double IDDT cheat mode.
// -----------------------------------------------------------------------------

static void AM_drawThings (void)
{
    int i;
    mpoint_t  pt;
    mobj_t *t;
    angle_t   actualangle;

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

            AM_drawLineCharacter(thintriangle_guy, NUMTHINTRIANGLEGUYLINES,
                                 actualradius, actualangle,
                                 // Monsters
                                 // [JN] CRL - ReMooD-inspired monsters coloring.
                                 t->flags & MF_COUNTKILL ? (t->health > 0 ? iddt_reds : 15) :
                                 // Explosive pod (does not have a MF_COUNTKILL flag)
                                 t->type == MT_POD ? IDDT_YELLOW :
                                 // Countable items
                                 t->flags & MF_COUNTITEM ? IDDT_GREEN :
                                 // Everything else
                                 IDDT_GRAY,
                                 pt.x, pt.y);
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
    for ( i = 0 ; i < markpointnum ; i++)
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

            if (fx >= f_x && fx <= (f_w / vid_resolution) - 5
            &&  fy >= f_y && fy <= (f_h / vid_resolution) - 6)
            {
                dp_translation = cr[CR_GREEN];
                V_DrawPatch(fx_flip - WIDESCREENDELTA, fy, marknums[d]);
                dp_translation = NULL;
            }

            // killough 2/22/98: 1 space backwards
            fx_flip -= MARK_W - (MARK_FLIP_1);

            j /= 10;
        } while (j > 0);
    }
}

// -----------------------------------------------------------------------------
// AM_drawkeys
// [PN] Simplified:
// - Replaced repeated code with a loop to handle all key points.
// - Used an array to store colors for each key, making the code more compact.
// -----------------------------------------------------------------------------

static void AM_drawkeys (void)
{
    mpoint_t pt;
    int i;
    const int colors[3] = { YELLOWKEY, GREENKEY, BLUEKEY };

    for (i = 0; i < 3; i++)
    {
        if (KeyPoints[i].x != 0 || KeyPoints[i].y != 0)
        {
            pt.x = KeyPoints[i].x >> FRACTOMAPBITS;
            pt.y = KeyPoints[i].y >> FRACTOMAPBITS;
            AM_transformPoint(&pt);
            AM_drawLineCharacter(keysquare, NUMKEYSQUARELINES, 0, 0, colors[i], pt.x, pt.y);
        }
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
    // y:  78 = (ORIGHEIGHT / 2) - 22

    static const byte am_xhair[] =
    {
        3,0,3,0,0,0,0,0,20,0,0,0,26,0,0,0,
        34,0,0,0,1,1,32,32,32,255,0,3,32,32,32,32,
        32,255,1,1,32,32,32,255
    };

    dp_translation = cr[CR_WHITE];
    V_DrawPatch(159, 78, (patch_t*)&am_xhair);
    dp_translation = NULL;
}

// -----------------------------------------------------------------------------
// AM_LevelNameDrawer
// -----------------------------------------------------------------------------

void AM_LevelNameDrawer (void)
{
    int numepisodes;

    if (gamemode == retail)
    {
        numepisodes = 5;
    }
    else
    {
        numepisodes = 3;
    }

    if (gameepisode <= numepisodes && gamemap < 10)
    {
        int x, y;

        // [JN] Move widgets slightly down when using a fullscreen status bar.
        if (dp_screen_size > 10 && (!automapactive || automap_overlay))
        {
            x = (widget_alignment ==  0) ? -WIDESCREENDELTA :     // left
                (widget_alignment ==  1) ? 20 :                   // status bar
                (dp_screen_size   >= 12) ? -WIDESCREENDELTA : 0;  // auto
            y = 159;
        }
        else
        {
            x = 20;
            y = 146;
        }

        const char *const level_name = LevelNames[(gameepisode - 1) * 9 + gamemap - 1];
        MN_DrTextA(DEH_String(level_name), x, y,
                   // [JN] Woof and DSDA widget color scheme using yellow map name.
                   widget_scheme > 2 ? cr[CR_YELLOW] : NULL);
    }
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
    // [JN] Moved from AM_Ticker for panning interpolation.
    if (m_paninc.x || m_paninc.y)
    {
        AM_changeWindowLoc();
    }

    // [PN] Moves the map window by using the mouse.
    if (mouse_pan_x != 0 || mouse_pan_y != 0)
    {
        AM_MousePanning();
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
        AM_drawBackground();
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

    if (ravmap_cheating == 2)
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

    if (gameskill == sk_baby)
    {
        AM_drawkeys();
    }

    // [JN] Draw level name only if Level Name widget is set to "automap".
    if (!widget_levelname)
    {
        AM_LevelNameDrawer();
    }
}
