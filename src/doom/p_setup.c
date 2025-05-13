//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2025 Polina "Aura" N.
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
// DESCRIPTION:
//	Do all the WAD I/O, get map description,
//	set up initial state and misc. LUTs.
//


#include <math.h>
#include "z_zone.h"
#include "deh_main.h"
#include "i_swap.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "d_main.h"
#include "g_game.h"
#include "i_system.h"
#include "i_timer.h"
#include "w_wad.h"
#include "p_local.h"
#include "s_sound.h"
#include "doomstat.h"
#include "d_englsh.h"
#include "p_extnodes.h" // [crispy] support extended node formats
#include "ct_chat.h"

#include "id_vars.h"
#include "id_func.h"


//
// MAP related lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//

int         numvertexes;
vertex_t   *vertexes;

int         numsegs;
seg_t      *segs;

int         numsectors;
sector_t   *sectors;

int          numsubsectors;
subsector_t *subsectors;

int         numnodes;
node_t     *nodes;

int         numlines;
line_t     *lines;

int         numsides;
side_t     *sides;

static int  totallines;

//
// BLOCKMAP
// Created from axis-aligned bounding box
// of the map, a rectangular array of
// blocks of size ... Used to speed up
// collision detection by spatial subdivision in 2D.
//

// Blockmap size
int        bmapwidth;
int        bmapheight;     // size in mapblocks

int32_t   *blockmap;       // [crispy] int for larger maps BLOCKMAP limit
int32_t   *blockmaplump;   // [crispy] offsets in blockmap are from here

// Blockmap origin
fixed_t    bmaporgx;
fixed_t    bmaporgy;

// Thing chains per block
mobj_t   **blocklinks;

// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without special effect, this could be
//  used as a PVS lookup as well.
//

byte *rejectmatrix;


// Maintain single and multi player starting spots.
#define MAX_DEATHMATCH_STARTS	10

mapthing_t  deathmatchstarts[MAX_DEATHMATCH_STARTS];
mapthing_t *deathmatch_p;
mapthing_t  playerstarts[MAXPLAYERS];
boolean     playerstartsingame[MAXPLAYERS];


// -----------------------------------------------------------------------------
// [JN] Builtin map names. Set for automap level name.
// -----------------------------------------------------------------------------

const char *level_name;

// DOOM 1 map names
#define HU_TITLE        (mapnames[(gameepisode-1)*9+gamemap-1])

// DOOM 2 map names
#define HU_TITLE2       (mapnames_commercial[gamemap-1])

// No Rest for the Living
#define HU_TITLEN       (mapnames_nerve[gamemap-1])

// Plutonia map names
#define HU_TITLEP       (mapnames_commercial[gamemap-1 + 32])

// TNT map names
#define HU_TITLET       (mapnames_commercial[gamemap-1 + 64])

// Chex Quest map names
#define HU_TITLE_CHEX   (mapnames_chex[(gameepisode-1)*9+gamemap-1])


static char *mapnames[] = // DOOM shareware/registered/retail (Ultimate) names.
{
    HUSTR_E1M1, HUSTR_E1M2, HUSTR_E1M3, HUSTR_E1M4, HUSTR_E1M5, 
    HUSTR_E1M6, HUSTR_E1M7, HUSTR_E1M8, HUSTR_E1M9,
    HUSTR_E2M1, HUSTR_E2M2, HUSTR_E2M3, HUSTR_E2M4, HUSTR_E2M5,
    HUSTR_E2M6, HUSTR_E2M7, HUSTR_E2M8, HUSTR_E2M9,
    HUSTR_E3M1, HUSTR_E3M2, HUSTR_E3M3, HUSTR_E3M4, HUSTR_E3M5,
    HUSTR_E3M6, HUSTR_E3M7, HUSTR_E3M8, HUSTR_E3M9,
    HUSTR_E4M1, HUSTR_E4M2, HUSTR_E4M3, HUSTR_E4M4, HUSTR_E4M5,
    HUSTR_E4M6, HUSTR_E4M7, HUSTR_E4M8, HUSTR_E4M9,

    // [crispy] Sigil
    HUSTR_E5M1, HUSTR_E5M2, HUSTR_E5M3, HUSTR_E5M4, HUSTR_E5M5,
    HUSTR_E5M6, HUSTR_E5M7, HUSTR_E5M8, HUSTR_E5M9,

    // [crispy] Sigil II
    HUSTR_E6M1, HUSTR_E6M2, HUSTR_E6M3, HUSTR_E6M4, HUSTR_E6M5,
    HUSTR_E6M6, HUSTR_E6M7, HUSTR_E6M8, HUSTR_E6M9,

    "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL",
    "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL"
};

static char *mapnames_chex[] = // Chex Quest names.
{
    HUSTR_E1M1, HUSTR_E1M2, HUSTR_E1M3, HUSTR_E1M4, HUSTR_E1M5,
    HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5,
    HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5,
    HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5,
    HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5,
    HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5,
    HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5, HUSTR_E1M5,
    HUSTR_E1M5,

    "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL",
    "NEWLEVEL", "NEWLEVEL", "NEWLEVEL", "NEWLEVEL"
};

// List of names for levels in commercial IWADs
// (doom2.wad, plutonia.wad, tnt.wad).  These are stored in a
// single large array; WADs like pl2.wad have a MAP33, and rely on
// the layout in the Vanilla executable, where it is possible to
// overflow the end of one array into the next.

static char *mapnames_commercial[] =
{
    // DOOM 2 map names.
    HUSTR_1,  HUSTR_2,  HUSTR_3,  HUSTR_4,  HUSTR_5,
    HUSTR_6,  HUSTR_7,  HUSTR_8,  HUSTR_9,  HUSTR_10,
    HUSTR_11, HUSTR_12, HUSTR_13, HUSTR_14, HUSTR_15,
    HUSTR_16, HUSTR_17, HUSTR_18, HUSTR_19, HUSTR_20,
    HUSTR_21, HUSTR_22, HUSTR_23, HUSTR_24, HUSTR_25,
    HUSTR_26, HUSTR_27, HUSTR_28, HUSTR_29, HUSTR_30,
    HUSTR_31, HUSTR_32,

    // Plutonia WAD map names.
    PHUSTR_1,  PHUSTR_2,  PHUSTR_3,  PHUSTR_4,  PHUSTR_5,
    PHUSTR_6,  PHUSTR_7,  PHUSTR_8,  PHUSTR_9,  PHUSTR_10,
    PHUSTR_11, PHUSTR_12, PHUSTR_13, PHUSTR_14, PHUSTR_15,
    PHUSTR_16, PHUSTR_17, PHUSTR_18, PHUSTR_19, PHUSTR_20,
    PHUSTR_21, PHUSTR_22, PHUSTR_23, PHUSTR_24, PHUSTR_25,
    PHUSTR_26, PHUSTR_27, PHUSTR_28, PHUSTR_29, PHUSTR_30,
    PHUSTR_31, PHUSTR_32,

    // TNT WAD map names.
    THUSTR_1,  THUSTR_2,  THUSTR_3,  THUSTR_4,  THUSTR_5,
    THUSTR_6,  THUSTR_7,  THUSTR_8,  THUSTR_9,  THUSTR_10,
    THUSTR_11, THUSTR_12, THUSTR_13, THUSTR_14, THUSTR_15,
    THUSTR_16, THUSTR_17, THUSTR_18, THUSTR_19, THUSTR_20,
    THUSTR_21, THUSTR_22, THUSTR_23, THUSTR_24, THUSTR_25,
    THUSTR_26, THUSTR_27, THUSTR_28, THUSTR_29, THUSTR_30,
    THUSTR_31, THUSTR_32
};

static char *mapnames_nerve[] =
{
    // No Rest for the Living map names.
    NHUSTR_1, NHUSTR_2, NHUSTR_3, NHUSTR_4, NHUSTR_5,
    NHUSTR_6, NHUSTR_7, NHUSTR_8, NHUSTR_9,
};

static void P_LevelNameInit (void)
{
    const char *s;

    switch (logical_gamemission)
    {
        case doom:
        s = HU_TITLE;
        break;

        case doom2:
        if (nerve && gamemap <= 9)
        {
            s = HU_TITLEN;
        }
        else
        {
            s = HU_TITLE2;
        }
        break;

        case pack_plut:
        s = HU_TITLEP;
        break;

        case pack_tnt:
        s = HU_TITLET;
        break;
        
        default:
        s = "UNKNOWN LEVEL";
        break;
    }

    if (logical_gamemission == doom && gameversion == exe_chex)
    {
        s = HU_TITLE_CHEX;
    }

    // dehacked substitution to get modified level name
    s = DEH_String(s);

    level_name = s;
}

// -----------------------------------------------------------------------------
// P_LoadVertexes
// -----------------------------------------------------------------------------

static void P_LoadVertexes (int lump)
{
    // Determine number of vertices
    const int lumpSize = W_LumpLength(lump);
    const int count = lumpSize / sizeof(mapvertex_t);
    numvertexes = count;

    // Allocate zone memory for vertices
    vertex_t *const verts = Z_Malloc((size_t)count * sizeof(*verts), PU_LEVEL, 0);
    vertexes = verts;

    // Load raw vertex data into cache
    const byte *const dataV = W_CacheLumpNum(lump, PU_STATIC);
    const mapvertex_t *restrict srcV = (const mapvertex_t *)dataV;

    // Copy and convert vertex coordinates to fixed-point
    for (int i = 0; i < count; ++i)
    {
        const fixed_t x = SHORT(srcV[i].x) << FRACBITS;
        const fixed_t y = SHORT(srcV[i].y) << FRACBITS;

        verts[i].x = x;
        verts[i].y = y;

        // [crispy] initialize vertex coordinates *only* used in rendering
        verts[i].r_x = x;
        verts[i].r_y = y;
        verts[i].moved = false;
    }

    // Release the cached lump
    W_ReleaseLumpNum(lump);
}

// -----------------------------------------------------------------------------
// GetSectorAtNullAddress
// -----------------------------------------------------------------------------

sector_t *GetSectorAtNullAddress (void)
{
    static boolean null_sector_is_initialized = false;
    static sector_t null_sector;

    if (!null_sector_is_initialized)
    {
        memset(&null_sector, 0, sizeof(null_sector));
        I_GetMemoryValue(0, &null_sector.floorheight, 4);
        I_GetMemoryValue(4, &null_sector.ceilingheight, 4);
        null_sector_is_initialized = true;
    }

    return &null_sector;
}

// -----------------------------------------------------------------------------
// GetOffset
// [crispy] recalculate seg offsets
// adapted from prboom-plus/src/p_setup.c:474-482
// -----------------------------------------------------------------------------

fixed_t GetOffset (const vertex_t *restrict v1, const vertex_t *restrict v2)
{
    // Compute delta in map units
    const int32_t dx = (v1->x - v2->x) >> FRACBITS;
    const int32_t dy = (v1->y - v2->y) >> FRACBITS;

    // Euclidean distance in map units
    const double dist = sqrt((double)dx * dx + (double)dy * dy);

    // Convert back to fixed-point
    return (fixed_t)(dist * FRACUNIT);
}

// -----------------------------------------------------------------------------
// P_LoadSegs
// -----------------------------------------------------------------------------

static void P_LoadSegs (int lump)
{
    // Determine number of segments
    const int lumpSize = W_LumpLength(lump);
    const int count = lumpSize / sizeof(mapseg_t);
    numsegs = count;

    // Allocate and zero-initialize segment array
    seg_t *const dst = Z_Malloc((size_t)count * sizeof(*dst), PU_LEVEL, 0);
    memset(dst, 0, (size_t)count * sizeof(*dst));
    segs = dst;

    // Load raw segment data into cache
    const byte *const dataS = W_CacheLumpNum(lump, PU_STATIC);
    const mapseg_t *restrict srcS = (const mapseg_t *)dataS;

    // Process each segment
    for (int i = 0; i < count; ++i)
    {
        const mapseg_t *const ms = &srcS[i];
        seg_t *const si = &dst[i];

        // Vertex references (extended nodes)
        const unsigned short v1i = SHORT(ms->v1);
        const unsigned short v2i = SHORT(ms->v2);
        si->v1 = &vertexes[v1i];
        si->v2 = &vertexes[v2i];

        // Angle
        si->angle = SHORT(ms->angle) << FRACBITS;

        // Linedef
        const unsigned short lineIdx = (unsigned short)SHORT(ms->linedef);
        if ((unsigned)lineIdx >= (unsigned)numlines)
            I_Error("P_LoadSegs: seg %d references a non-existent linedef %d", i, (unsigned)lineIdx);
        line_t *const ldef = &lines[lineIdx];
        si->linedef = ldef;

        // Side index
        const int side = SHORT(ms->side);
        const int sideNum = ldef->sidenum[side];
        if ((unsigned)sideNum >= (unsigned)numsides)
            I_Error("P_LoadSegs: linedef %d for seg %d references a non-existent sidedef %d", (unsigned)lineIdx, i, sideNum);

        si->sidedef = &sides[sideNum];
        si->frontsector = sides[sideNum].sector;

        // Recalculate offset
        si->offset = GetOffset(si->v1, side ? ldef->v2 : ldef->v1);

        // Backsector logic for two-sided lines
        if (ldef->flags & ML_TWOSIDED)
        {
            const int otherSide = ldef->sidenum[side ^ 1];
            if ((unsigned)otherSide >= (unsigned)numsides)
            {
                if (si->sidedef->midtexture)
                {
                    si->backsector = 0;
                    fprintf(stderr,
                        "P_LoadSegs: Linedef %d has two-sided flag set, but no second sidedef\n",
                        (unsigned)lineIdx);
                }
                else
                {
                    si->backsector = GetSectorAtNullAddress();
                }
            }
            else
            {
                si->backsector = sides[otherSide].sector;
            }
        }
        else
        {
            si->backsector = 0;
        }
    }

    // Release the cached lump
    W_ReleaseLumpNum(lump);
}


// [crispy] fix long wall wobble

static angle_t anglediff(angle_t a, angle_t b)
{
	if (b > a)
		return anglediff(b, a);

	if (a - b < ANG180)
		return a - b;
	else // [crispy] wrap around
		return b - a;
}

// -----------------------------------------------------------------------------
// P_SegLengths
// -----------------------------------------------------------------------------

void P_SegLengths (boolean contrast_only)
{
    const int count = numsegs;
    const int rightangle = abs(finesine[(ANG60/2) >> ANGLETOFINESHIFT]);
    // [JN] Make fake contrast optional.
    const int fakecont_val  = vis_fake_contrast ? LIGHTBRIGHT : 0;
    // [JN] Apply smoother fake contrast for smooth diminishing lighting.
    const int smoothlit_val = (vis_fake_contrast && vis_smooth_light) ? LIGHTBRIGHT : 0;

    seg_t *const segArray = segs;

    for (int i = 0; i < count; ++i)
    {
        seg_t *const s = &segArray[i];
        const int64_t dx = s->v2->r_x - s->v1->r_x;
        const int64_t dy = s->v2->r_y - s->v1->r_y;

        if (!contrast_only)
        {
            const double dist = sqrt((double)dx * dx + (double)dy * dy);
            s->length = (uint32_t)(dist * 0.5);

            // [crispy] re-calculate angle used for rendering
            viewx = s->v1->r_x;
            viewy = s->v1->r_y;
            const angle_t newAngle = R_PointToAngleCrispy(s->v2->r_x, s->v2->r_y);
            s->r_angle = (anglediff(newAngle, s->angle) > ANG60/2) ? s->angle : newAngle;
        }

        // [crispy] smoother fake contrast
        // [PN] Use shifted angle for lookup
        const angle_t shifted = s->r_angle >> ANGLETOFINESHIFT;
        const fixed_t sine_val = abs(finesine[shifted]);
        const fixed_t cosine_val = abs(finecosine[shifted]);
        int fc;

        if (dy == 0)
        {
            // [PN] Strong horizontal contrast
            fc = -fakecont_val;
        }
        else if (sine_val < rightangle)
        {
            // [PN] Smooth dimming for near-horizontal
            fc = -smoothlit_val + (smoothlit_val * sine_val / rightangle);
        }
        else if (dx == 0)
        {
            // [PN] Strong vertical contrast
            fc = fakecont_val;
        }
        else if (cosine_val < rightangle)
        {
            // [PN] Smooth dimming for near-vertical
            fc = smoothlit_val - (smoothlit_val * cosine_val / rightangle);
        }
        else
        {
            // [PN] Default case
            fc = 0;
        }
        s->fakecontrast = fc;
    }
}

// -----------------------------------------------------------------------------
// P_LoadSubsectors
// -----------------------------------------------------------------------------

static void P_LoadSubsectors (int lump)
{
    // Determine number of subsectors
    const int lumpSize = W_LumpLength(lump);
    const int count = lumpSize / sizeof(mapsubsector_t);
    numsubsectors = count;

    // Allocate zone memory for subsectors
    subsector_t *const dst = Z_Malloc((size_t)count * sizeof(*dst), PU_LEVEL, 0);
    subsectors = dst;

    // Load raw subsector data into cache
    const byte *const data = W_CacheLumpNum(lump, PU_STATIC);
    if (!data || count == 0)
        I_Error("P_LoadSubsectors: No subsectors in map! (lump %d)", lump);
    const mapsubsector_t *restrict src = (const mapsubsector_t *)data;

    // Zero-initialize destination array
    memset(dst, 0, (size_t)count * sizeof(*dst));

    // Copy fields
    for (int i = 0; i < count; ++i)
    {
        dst[i].numlines  = (unsigned short)SHORT(src[i].numsegs);   // [crispy] extended nodes
        dst[i].firstline = (unsigned short)SHORT(src[i].firstseg);  // [crispy] extended nodes
    }

    // Release the cached lump
    W_ReleaseLumpNum(lump);
}

// -----------------------------------------------------------------------------
// P_LoadSectors
// -----------------------------------------------------------------------------

static void P_LoadSectors (int lump)
{
    // Validate lump index
    if ((unsigned)lump >= (unsigned)numlumps)
        I_Error("P_LoadSectors: No sectors in map! (lump %d)", lump);

    // Determine number of sectors
    const int lumpSize = W_LumpLength(lump);
    const int count = lumpSize / sizeof(mapsector_t);
    numsectors = count;

    // Allocate and zero-initialize sector array
    sector_t *const dst = Z_Malloc((size_t)count * sizeof(*dst), PU_LEVEL, 0);
    memset(dst, 0, (size_t)count * sizeof(*dst));
    sectors = dst;

    // Load raw sector data into cache
    const byte *const data = W_CacheLumpNum(lump, PU_STATIC);
    if (!data || count == 0)
        I_Error("P_LoadSectors: No sectors in map! (lump %d)", lump);
    const mapsector_t *restrict src = (const mapsector_t *)data;

    // Copy fields
    for (int i = 0; i < count; ++i)
    {
        dst[i].floorheight         = SHORT(src[i].floorheight) << FRACBITS;
        dst[i].ceilingheight       = SHORT(src[i].ceilingheight) << FRACBITS;
        dst[i].floorpic            = R_FlatNumForName(src[i].floorpic);
        dst[i].ceilingpic          = R_FlatNumForName(src[i].ceilingpic);
        dst[i].lightlevel          = SHORT(src[i].lightlevel);
        dst[i].special             = SHORT(src[i].special);
        dst[i].tag                 = SHORT(src[i].tag);
        dst[i].thinglist           = NULL;
        dst[i].cachedheight        = 0; // [crispy] WiggleFix: [kb] for R_FixWiggle()

        // [AM] Sector interpolation. Even if we're
        // not running uncapped, the renderer still uses this data.
        dst[i].oldfloorheight      = dst[i].floorheight;
        dst[i].interpfloorheight   = dst[i].floorheight;
        dst[i].oldceilingheight    = dst[i].ceilingheight;
        dst[i].interpceilingheight = dst[i].ceilingheight;
        // [crispy] inhibit sector interpolation during the 0th gametic
        dst[i].oldgametic          = -1;
    }

    // Release the cached lump
    W_ReleaseLumpNum(lump);
}

// -----------------------------------------------------------------------------
// P_LoadNodes
// -----------------------------------------------------------------------------

static void P_LoadNodes (int lump)
{
    // Determine number of nodes
    const int lumpSize = W_LumpLength(lump);
    const int count = lumpSize / sizeof(mapnode_t);
    numnodes = count;

    // Allocate memory for nodes
    node_t *const dst = Z_Malloc((size_t)count * sizeof(*dst), PU_LEVEL, 0);
    nodes = dst;

    // Load raw node data into cache
    const byte *const data = W_CacheLumpNum(lump, PU_STATIC);
    if (!data || count == 0)
    {
        if (numsubsectors == 1)
            fprintf(stderr, "P_LoadNodes: No nodes in map, but only one subsector.\n");
        else
            I_Error("P_LoadNodes: No nodes in map!");
    }
    const mapnode_t *restrict src = (const mapnode_t *)data;

    // Copy fields
    for (int i = 0; i < count; ++i)
    {
        dst[i].x  = SHORT(src[i].x)  << FRACBITS;
        dst[i].y  = SHORT(src[i].y)  << FRACBITS;
        dst[i].dx = SHORT(src[i].dx) << FRACBITS;
        dst[i].dy = SHORT(src[i].dy) << FRACBITS;

        for (int j = 0; j < 2; ++j)
        {
            int child = (unsigned short)SHORT(src[i].children[j]); // [crispy] extended nodes

            if (child == NO_INDEX)
            {
                child = -1;
            }
            else if (child & NF_SUBSECTOR_VANILLA)
            {
                child &= ~NF_SUBSECTOR_VANILLA;
                if (child >= numsubsectors)
                    child = 0;
                child |= NF_SUBSECTOR;
            }

            dst[i].children[j] = child;

            // Bounding box
            for (int k = 0; k < 4; ++k)
            {
                dst[i].bbox[j][k] = SHORT(src[i].bbox[j][k]) << FRACBITS;
            }
        }
    }

    // Release the cached lump
    W_ReleaseLumpNum(lump);
}

// -----------------------------------------------------------------------------
// P_LoadThings
// -----------------------------------------------------------------------------

static void P_LoadThings (int lump)
{
    // Load raw things data into cache
    const byte *const data = W_CacheLumpNum(lump, PU_STATIC);
    if (!data)
        I_Error("P_LoadThings: Failed to load lump %d", lump);

    // Determine number of things
    const int count = W_LumpLength(lump) / sizeof(mapthing_t);
    const mapthing_t *restrict src = (const mapthing_t *)data;

    mapthing_t spawnthing;

    // Iterate and spawn
    for (int i = 0; i < count; ++i)
    {
        const mapthing_t *mt = &src[i];
        boolean spawn = true;

        // Do not spawn cool, new monsters if !commercial
        if (gamemode != commercial)
        {
            switch (SHORT(mt->type))
            {
                case 68:  // Arachnotron
                case 64:  // Archvile
                case 88:  // Boss Brain
                case 89:  // Boss Shooter
                case 69:  // Hell Knight
                case 67:  // Mancubus
                case 71:  // Pain Elemental
                case 65:  // Former Human Commando
                case 66:  // Revenant
                case 84:  // Wolf SS
                    spawn = false;
                    break;
            }
        }
        if (!spawn)
            continue;

        // Prepare spawnthing
        spawnthing.x       = SHORT(mt->x);
        spawnthing.y       = SHORT(mt->y);
        spawnthing.angle   = SHORT(mt->angle);
        spawnthing.type    = SHORT(mt->type);
        spawnthing.options = SHORT(mt->options);

        P_SpawnMapThing(&spawnthing);
    }

    // Validate player starts
    if (!deathmatch)
    {
        for (int i = 0; i < MAXPLAYERS; ++i)
        {
            if (playeringame[i] && !playerstartsingame[i])
                I_Error("P_LoadThings: Player %d start missing (vanilla crashes here)", i + 1);

            playerstartsingame[i] = false;
        }
    }

    // Release the cached lump
    W_ReleaseLumpNum(lump);
}

// -----------------------------------------------------------------------------
// P_LoadLineDefs
// Also counts secret lines for intermissions.
// -----------------------------------------------------------------------------

static void P_LoadLineDefs (int lump)
{
    // Determine number of lines
    const int lumpSize = W_LumpLength(lump);
    const int count = lumpSize / sizeof(maplinedef_t);
    numlines = count;

    // Allocate and zero-initialize line array
    line_t *const dst = Z_Malloc((size_t)count * sizeof(*dst), PU_LEVEL, 0);
    memset(dst, 0, (size_t)count * sizeof(*dst));
    lines = dst;

    // Load raw linedef data into cache
    const byte *const data = W_CacheLumpNum(lump, PU_STATIC);
    if (!data)
        I_Error("P_LoadLineDefs: Failed to load lump %d", lump);
    const maplinedef_t *restrict src = (const maplinedef_t *)data;

    int warn = 0, warn2 = 0;

    for (int i = 0; i < count; ++i)
    {
        line_t *const ld = &dst[i];
        const maplinedef_t *const ml = &src[i];

        // Flags and special
        ld->flags = (unsigned short)SHORT(ml->flags);
        ld->special = SHORT(ml->special);

        // Warn on unknown specials
        if ((unsigned)ld->special > 141 && ld->special != 271 && ld->special != 272)
        {
            fprintf(stderr, "P_LoadLineDefs: Unknown special %d at line %d.\n", ld->special, i);
            ++warn;
        }

        // Tag
        ld->tag = SHORT(ml->tag);
        if (ld->special && !ld->tag)
        {
            switch (ld->special)
            {
                case 1: case 26: case 27: case 28:
                case 31: case 32: case 33: case 34:
                case 48: case 85: case 11: case 51:
                case 52: case 117: case 118: case 271:
                case 272:
                    break;
                default:
                    fprintf(stderr, "P_LoadLineDefs: Special linedef %d without tag.\n", i);
                    ++warn2;
            }
        }

        // Vertices
        const unsigned short v1idx = (unsigned short)SHORT(ml->v1);
        const unsigned short v2idx = (unsigned short)SHORT(ml->v2);
        ld->v1 = &vertexes[v1idx];
        ld->v2 = &vertexes[v2idx];
        ld->dx = ld->v2->x - ld->v1->x;
        ld->dy = ld->v2->y - ld->v1->y;

        // Slope type
        if (!ld->dx)
            ld->slopetype = ST_VERTICAL;
        else if (!ld->dy)
            ld->slopetype = ST_HORIZONTAL;
        else
            ld->slopetype = (FixedDiv(ld->dy, ld->dx) > 0) ? ST_POSITIVE : ST_NEGATIVE;

        // Bounding box
        ld->bbox[BOXLEFT]   = MIN(ld->v1->x, ld->v2->x);
        ld->bbox[BOXRIGHT]  = MAX(ld->v1->x, ld->v2->x);
        ld->bbox[BOXBOTTOM] = MIN(ld->v1->y, ld->v2->y);
        ld->bbox[BOXTOP]    = MAX(ld->v1->y, ld->v2->y);

        // Sound origin: midpoint
        ld->soundorg.x = (ld->bbox[BOXLEFT] + ld->bbox[BOXRIGHT]) >> 1;
        ld->soundorg.y = (ld->bbox[BOXBOTTOM] + ld->bbox[BOXTOP]) >> 1;

        // Side numbers
        ld->sidenum[0] = SHORT(ml->sidenum[0]);
        ld->sidenum[1] = SHORT(ml->sidenum[1]);

        // Frontsector/backsector
        if (ld->sidenum[0] == NO_INDEX)
        {
            ld->sidenum[0] = 0;
            fprintf(stderr, "P_LoadLineDefs: linedef %d without first sidedef!\n", i);
        }
        ld->frontsector = (ld->sidenum[0] != NO_INDEX) ? sides[ld->sidenum[0]].sector : NULL;
        ld->backsector  = (ld->sidenum[1] != NO_INDEX) ? sides[ld->sidenum[1]].sector : NULL;
    }

    // Post-warnings
    if (warn)
        fprintf(stderr, "P_LoadLineDefs: Found %d unknown special linedef(s).\n", warn);
    if (warn2)
        fprintf(stderr, "P_LoadLineDefs: Found %d special linedef(s) without tag.\n", warn2);
    if (warn || warn2)
        fprintf(stderr, "THIS MAP MAY NOT WORK AS EXPECTED!\n");

    // Release the cached lump
    W_ReleaseLumpNum(lump);
}

// -----------------------------------------------------------------------------
// P_LoadSideDefs
// -----------------------------------------------------------------------------

static void P_LoadSideDefs (int lump)
{
    // Determine number of sidedefs
    const int lumpSize = W_LumpLength(lump);
    const int count = lumpSize / sizeof(mapsidedef_t);
    numsides = count;

    // Allocate and zero-initialize sidedef array
    side_t *const dst = Z_Malloc((size_t)count * sizeof(*dst), PU_LEVEL, 0);
    memset(dst, 0, (size_t)count * sizeof(*dst));
    sides = dst;

    // Load raw sidedef data into cache
    const byte *const data = W_CacheLumpNum(lump, PU_STATIC);
    if (!data)
        I_Error("P_LoadSideDefs: Failed to load lump %d", lump);
    const mapsidedef_t *restrict src = (const mapsidedef_t *)data;

    // Copy fields
    for (int i = 0; i < count; ++i)
    {
        side_t *const sd = &dst[i];
        const mapsidedef_t *const ms = &src[i];

        sd->textureoffset    = SHORT(ms->textureoffset) << FRACBITS;
        sd->rowoffset        = SHORT(ms->rowoffset)    << FRACBITS;
        sd->toptexture       = R_TextureNumForName(ms->toptexture);
        sd->bottomtexture    = R_TextureNumForName(ms->bottomtexture);
        sd->midtexture       = R_TextureNumForName(ms->midtexture);
        
        // Sector reference (extended nodes)
        const unsigned short secIdx = (unsigned short)SHORT(ms->sector);
        sd->sector = &sectors[secIdx];

        // [crispy] smooth texture scrolling
        sd->basetextureoffset = sd->textureoffset;
    }

    // Release the cached lump
    W_ReleaseLumpNum(lump);
}

// -----------------------------------------------------------------------------
// P_LoadBlockMap
// -----------------------------------------------------------------------------

static boolean P_LoadBlockMap (int lump)
{
    // Early exit conditions
    const int numlumps_u = numlumps;
    const int lumplen = W_LumpLength(lump);
    const int count = lumplen / sizeof(short);

    if (M_CheckParm("-blockmap")
    || (unsigned)lump >= (unsigned)numlumps_u
    || lumplen < 8
    || count >= 0x10000)
    {
        return false;
    }

    // Load raw blockmap lump
    short *const raw = Z_Malloc((size_t)lumplen, PU_LEVEL, NULL);
    W_ReadLump(lump, raw);

    // Allocate blockmaplump and setup pointer
    blockmaplump = Z_Malloc((size_t)count * sizeof(*blockmaplump), PU_LEVEL, NULL);
    blockmap = blockmaplump + 4;

    // Header entries
    blockmaplump[0] = SHORT(raw[0]);
    blockmaplump[1] = SHORT(raw[1]);
    blockmaplump[2] = (int32_t)(SHORT(raw[2])) & 0xffff;
    blockmaplump[3] = (int32_t)(SHORT(raw[3])) & 0xffff;

    // Swap all short integers to native ordering
    for (int idx = 4; idx < count; ++idx)
    {
        const short val = SHORT(raw[idx]);
        blockmaplump[idx] = (val == -1) ? -1l : (int32_t)val & 0xffff;
    }

    // Free the temporary lump
    Z_Free(raw);

    // Read header values into globals
    bmaporgx   = blockmaplump[0] << FRACBITS;
    bmaporgy   = blockmaplump[1] << FRACBITS;
    bmapwidth  = blockmaplump[2];
    bmapheight = blockmaplump[3];

    // Allocate and clear blocklinks
    const size_t linkCount = (size_t)bmapwidth * (size_t)bmapheight;
    const size_t linkSize  = linkCount * sizeof(*blocklinks);
    blocklinks = Z_Malloc(linkSize, PU_LEVEL, 0);
    memset(blocklinks, 0, linkSize);

    return true;
}

// -----------------------------------------------------------------------------
// P_CreateBlockMap
// [PN] This function was fully refactored for clarity and efficiency.
//  The original logic is preserved, including the traversal algorithm,
//  compression layout, and handling of special cases. Local variables
//  are declared closer to usage; shifts and absolutes are computed once,
//  not repeatedly. Memory allocations for block lists are done up front
//  using calloc, with dynamic growth using a doubling strategy. Loops
//  are rewritten in structured form for better readability and control.
//  The blockmap is built and compressed exactly as before, but the code
//  is now more maintainable, less error-prone, and slightly faster overall.
// -----------------------------------------------------------------------------

static void P_CreateBlockMap(void)
{
    // Compute map bounds in block units
    int32_t minX = INT_MAX, minY = INT_MAX;
    int32_t maxX = INT_MIN, maxY = INT_MIN;

    for (int i = 0; i < numvertexes; ++i)
    {
        const int32_t x = vertexes[i].x >> FRACBITS;
        const int32_t y = vertexes[i].y >> FRACBITS;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }

    // Save blockmap origin and dimensions
    bmaporgx   = minX << FRACBITS;
    bmaporgy   = minY << FRACBITS;
    bmapwidth  = ((maxX - minX) >> MAPBTOFRAC) + 1;
    bmapheight = ((maxY - minY) >> MAPBTOFRAC) + 1;

    // Build temporary block lists
    typedef struct { int count, alloc; int *restrict items; } BlockList;
    const unsigned totalBlocks = (unsigned)bmapwidth * (unsigned)bmapheight;
    BlockList *const restrict blocks = calloc(totalBlocks, sizeof *blocks);

    for (int lineIdx = 0; lineIdx < numlines; ++lineIdx)
    {
        const line_t *restrict ln = &lines[lineIdx];
        // Map endpoints to blocks
        int x0 = (ln->v1->x >> FRACBITS) - minX;
        int y0 = (ln->v1->y >> FRACBITS) - minY;
        int x1 = (ln->v2->x >> FRACBITS) - minX;
        int y1 = (ln->v2->y >> FRACBITS) - minY;
        int dx = (ln->dx >> FRACBITS) < 0 ? -1 : 1;
        int dy = (ln->dy >> FRACBITS) < 0 ? -1 : 1;
        int adx = abs(ln->dx >> FRACBITS);
        int ady = abs(ln->dy >> FRACBITS);

        // Bresenham-like traversal
        int diff = !adx ? 1 : !ady ? -1
            : (((x0 >> MAPBTOFRAC)<<MAPBTOFRAC) + ((dx>0)?MAPBLOCKUNITS-1:0) - x0) * ady * dx
            - (((y0 >> MAPBTOFRAC)<<MAPBTOFRAC) + ((dy>0)?MAPBLOCKUNITS-1:0) - y0) * adx * dy;
        unsigned idx = (unsigned)((y0 >> MAPBTOFRAC) * bmapwidth + (x0 >> MAPBTOFRAC));
        unsigned endIdx = (unsigned)((y1 >> MAPBTOFRAC) * bmapwidth + (x1 >> MAPBTOFRAC));
        const int stepYBlock = dy * bmapwidth;
        adx <<= MAPBTOFRAC; ady <<= MAPBTOFRAC;

        while (idx < totalBlocks)
        {
            BlockList *restrict bl = &blocks[idx];
            if (bl->count >= bl->alloc)
            {
                int newAlloc = bl->alloc ? bl->alloc * 2 : 8;
                bl->items = I_Realloc(bl->items, newAlloc * sizeof *bl->items);
                bl->alloc = newAlloc;
            }
            bl->items[bl->count++] = lineIdx;
            if (idx == endIdx)
                break;
            if (diff < 0) { diff += ady; idx += dx; }
            else         { diff -= adx; idx += stepYBlock; }
        }
    }

    // Compute total size for compressed blockmap
    unsigned lumpSize = totalBlocks + 6;
    for (unsigned b = 0; b < totalBlocks; ++b)
        if (blocks[b].count)
            lumpSize += blocks[b].count + 2;

    // Allocate blockmap lump
    blockmaplump = Z_Malloc((size_t)lumpSize * sizeof *blockmaplump, PU_LEVEL, 0);
    unsigned pos = totalBlocks + 4;
    blockmaplump[pos++] = 0;
    blockmaplump[pos++] = -1;

    // Compress into blockmaplump
    for (unsigned b = 4; b < totalBlocks + 4; ++b)
    {
        BlockList *restrict bl = &blocks[b - 4];
        if (bl->count)
        {
            blockmaplump[blockmaplump[b] = pos++] = 0;
            while (bl->count)
                blockmaplump[pos++] = bl->items[--bl->count];
            blockmaplump[pos++] = -1;
            free(bl->items);
        }
        else
        {
            blockmaplump[b] = totalBlocks + 4;
        }
    }
    free(blocks);

    // Finalize global blockmap pointer and links
    blockmap = blockmaplump + 4;
    const size_t linksCount = (size_t)bmapwidth * bmapheight;
    blocklinks = Z_Malloc(linksCount * sizeof *blocklinks, PU_LEVEL, 0);
    memset(blocklinks, 0, linksCount * sizeof *blocklinks);
}

// -----------------------------------------------------------------------------
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
// -----------------------------------------------------------------------------

void P_GroupLines (void)
{
    // Assign a sector for each subsector
    for (int i = 0; i < numsubsectors; ++i)
    {
        subsector_t *const ss = &subsectors[i];
        const seg_t *const segList = &segs[ss->firstline];
        ss->sector = NULL;

        for (int j = 0; j < ss->numlines; ++j)
        {
            const seg_t *const s = &segList[j];
            if (s->sidedef)
            {
                ss->sector = s->sidedef->sector;
                break;
            }
        }
        if (!ss->sector)
            I_Error("P_GroupLines: Subsector %d is not part of any sector!", i);
    }

    // Count total lines and initialize linecount per sector
    totallines = 0;
    for (int i = 0; i < numlines; ++i)
    {
        line_t *const ln = &lines[i];
        ++totallines;
        ln->frontsector->linecount++;
        if (ln->backsector && ln->backsector != ln->frontsector)
        {
            ln->backsector->linecount++;
            ++totallines;
        }
    }

    // Allocate line pointer buffer
    line_t **linebuffer = Z_Malloc((size_t)totallines * sizeof(*linebuffer), PU_LEVEL, 0);

    // Distribute buffer to sectors and reset their linecount
    for (int i = 0; i < numsectors; ++i)
    {
        sector_t *const sec = &sectors[i];
        sec->lines = linebuffer;
        linebuffer += sec->linecount;
        sec->linecount = 0;
    }

    // Assign each line to its sectors
    for (int i = 0; i < numlines; ++i)
    {
        line_t *const ln = &lines[i];
        sector_t *const fs = ln->frontsector;
        fs->lines[fs->linecount++] = ln;

        sector_t *const bs = ln->backsector;
        if (bs && bs != fs)
            bs->lines[bs->linecount++] = ln;
    }

    // Generate bounding boxes and blockboxes for each sector
    for (int i = 0; i < numsectors; ++i)
    {
        sector_t *const sec = &sectors[i];
        fixed_t bbox[4];
        M_ClearBox(bbox);

        for (int j = 0; j < sec->linecount; ++j)
        {
            const line_t *const ln = sec->lines[j];
            M_AddToBox(bbox, ln->v1->x, ln->v1->y);
            M_AddToBox(bbox, ln->v2->x, ln->v2->y);
        }

        // Sound origin in middle of bbox
        sec->soundorg.x = (bbox[BOXLEFT] + bbox[BOXRIGHT]) >> 1;
        sec->soundorg.y = (bbox[BOXBOTTOM] + bbox[BOXTOP]) >> 1;

        // Compute blockbox clamped to map
        const int top = (bbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;
        sec->blockbox[BOXTOP] = (top >= bmapheight) ? bmapheight - 1 : top;

        const int bottom = (bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
        sec->blockbox[BOXBOTTOM] = (bottom < 0) ? 0 : bottom;

        const int right = (bbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
        sec->blockbox[BOXRIGHT] = (right >= bmapwidth) ? bmapwidth - 1 : right;

        const int left = (bbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
        sec->blockbox[BOXLEFT] = (left < 0) ? 0 : left;
    }
}

// -----------------------------------------------------------------------------
// P_RemoveSlimeTrails
// [crispy] remove slime trails
// mostly taken from Lee Killough's implementation in mbfsrc/P_SETUP.C:849-924,
// with the exception that not the actual vertex coordinates are modified,
// but separate coordinates that are *only* used in rendering,
// i.e. r_bsp.c:R_AddLine()
// -----------------------------------------------------------------------------

static void P_RemoveSlimeTrails (void)
{
    const int count = numsegs;
    seg_t *const segArray = segs;

    for (int i = 0; i < count; ++i)
    {
        const seg_t *const seg = &segArray[i];
        const line_t *const l = seg->linedef;

        // [crispy] ignore exactly vertical or horizontal linedefs
        if (l->dx == 0 || l->dy == 0)
            continue;

        // Precompute fractional deltas
        const int32_t dxf = l->dx >> FRACBITS;
        const int32_t dyf = l->dy >> FRACBITS;
        const int64_t dx2 = (int64_t)dxf * dxf;
        const int64_t dy2 = (int64_t)dyf * dyf;
        const int64_t dxy = (int64_t)dxf * dyf;
        const int64_t s = dx2 + dy2;

        // Process both vertices of the segment
        vertex_t *const verts[2] = { seg->v1, seg->v2 };
        for (int vIdx = 0; vIdx < 2; ++vIdx)
        {
            vertex_t *const v = verts[vIdx];

            // [crispy] vertex wasn't already moved
            if (v->moved)
                continue;

            v->moved = true;

            // [crispy] ignore endpoints of linedefs
            if (v == l->v1 || v == l->v2)
                continue;

            // [crispy] move the vertex towards the linedef by projecting
            const fixed_t origX = v->x;
            const fixed_t origY = v->y;

            v->r_x = (fixed_t)((dx2 * origX + dy2 * l->v1->x + dxy * (origY - l->v1->y)) / s);
            v->r_y = (fixed_t)((dy2 * origY + dx2 * l->v1->y + dxy * (origX - l->v1->x)) / s);

            // [crispy] moved more than 8 map units? revert
            if (abs(v->r_x - origX) > 8 * FRACUNIT || abs(v->r_y - origY) > 8 * FRACUNIT)
            {
                v->r_x = origX;
                v->r_y = origY;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// PadRejectArray
// Pad the REJECT lump with extra data when the lump is too small,
// to simulate a REJECT buffer overflow in Vanilla Doom.
// -----------------------------------------------------------------------------

static void PadRejectArray (byte *restrict array, unsigned int len)
{
    // Compute padded size: totallines*4 rounded up to multiple of 4, plus header
    const unsigned int paddedSize = ((totallines * 4 + 3) & ~3U) + 24;

    // Header values for REJECT array
    const unsigned int rejectpad[4] = {
        paddedSize,     // Size
        0,              // Part of z_zone block header
        50,             // PU_LEVEL
        0x1d4a11        // DOOM_CONST_ZONEID
    };

    // Copy header into array (little-endian)
    const unsigned int maxBytes = len < sizeof(rejectpad) ? len : sizeof(rejectpad);
    for (unsigned int i = 0; i < maxBytes; ++i)
    {
        const unsigned int wordIndex = i / 4;
        const unsigned int byteShift = (i % 4) * 8;
        array[i] = (byte)((rejectpad[wordIndex] >> byteShift) & 0xFF);
    }

    // If the REJECT lump is larger than header, pad remaining bytes
    if (len > sizeof(rejectpad))
    {
        const unsigned int padStart = sizeof(rejectpad);
        const unsigned int padCount = len - padStart;
        const byte padValue = M_CheckParm("-reject_pad_with_ff") ? 0xFF : 0x00;
        memset(array + padStart, padValue, padCount);
    }
    else if (len > sizeof(rejectpad))
    {
        fprintf(stderr,
            "PadRejectArray: REJECT lump too short to pad! (%u > %zu)\n",
            len, sizeof(rejectpad));
    }
}

// -----------------------------------------------------------------------------
// P_LoadReject
// -----------------------------------------------------------------------------

static void P_LoadReject (int lumpnum)
{
    // Calculate expected REJECT lump size: one bit per sector-to-sector pair
    const int expectedSize = (numsectors * numsectors + 7) / 8;

    // Get actual lump length
    const int actualSize = W_LumpLength(lumpnum);

    if (actualSize >= expectedSize)
    {
        // Lump is large enough: load directly
        rejectmatrix = W_CacheLumpNum(lumpnum, PU_LEVEL);
    }
    else
    {
        // Allocate correct-sized buffer and read partial data
        rejectmatrix = Z_Malloc((size_t)expectedSize, PU_LEVEL, &rejectmatrix);
        W_ReadLump(lumpnum, rejectmatrix);

        // Pad remaining bytes with header-derived values
        PadRejectArray(rejectmatrix + actualSize, (unsigned)expectedSize - (unsigned)actualSize);
    }
}

// -----------------------------------------------------------------------------
// P_SetupLevel
// -----------------------------------------------------------------------------

void P_SetupLevel (int episode, int map)
{
    // Indicate level loading start time
    const int starttime = I_GetTimeMS();

    // Reset level stats
    totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
    wminfo.partime = 180;
    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].killcount      = 0;
        players[i].extrakillcount = 0;
        players[i].secretcount    = 0;
        players[i].itemcount      = 0;
    }

    // Initial view height
    players[consoleplayer].viewz = 1;

    // Stop demo warp if active
    if (demowarp == map)
    {
        demowarp = 0;
        nodrawers = singletics = false;
    }

    // Prepare memory and thinkers
    S_Start();
    Z_FreeTags(PU_LEVEL, PU_PURGELEVEL - 1);
    P_InitThinkers();
    W_Reload();

    // Determine lump name
    char lumpname[9];
    if (gamemode == commercial)
        DEH_snprintf(lumpname, sizeof lumpname, "MAP%02d", map);
    else
        DEH_snprintf(lumpname, sizeof lumpname, "E%dM%d", episode, map);

    const int lumpnum = W_GetNumForName(lumpname);

    // Reset timers
    leveltime = realleveltime = oldleveltime = 0;

    // Log loading
    if (gamemode == commercial)
        printf("P_SetupLevel: MAP%02d, ", gamemap);
    else
        printf("P_SetupLevel: E%dM%d, ", gameepisode, gamemap);

    // Load map format and data
    mapformat_t fmt    = P_CheckMapFormat(lumpnum);
    boolean validBMap  = P_LoadBlockMap(lumpnum + ML_BLOCKMAP);
    P_LoadVertexes(lumpnum + ML_VERTEXES);
    P_LoadSectors(lumpnum + ML_SECTORS);
    P_LoadSideDefs(lumpnum + ML_SIDEDEFS);

    if (fmt & MFMT_HEXEN)
        P_LoadLineDefs_Hexen(lumpnum + ML_LINEDEFS);
    else
        P_LoadLineDefs(lumpnum + ML_LINEDEFS);

    if (!validBMap)
        P_CreateBlockMap();

    if (fmt & (MFMT_ZDBSPX | MFMT_ZDBSPZ))
    {
        P_LoadNodes_ZDBSP(lumpnum + ML_NODES, fmt & MFMT_ZDBSPZ);
    }
    else if (fmt & MFMT_DEEPBSP)
    {
        P_LoadSubsectors_DeePBSP(lumpnum + ML_SSECTORS);
        P_LoadNodes_DeePBSP(lumpnum + ML_NODES);
        P_LoadSegs_DeePBSP(lumpnum + ML_SEGS);
    }
    else
    {
        P_LoadSubsectors(lumpnum + ML_SSECTORS);
        P_LoadNodes(lumpnum + ML_NODES);
        P_LoadSegs(lumpnum + ML_SEGS);
    }

    P_GroupLines();
    P_LoadReject(lumpnum + ML_REJECT);

    // Post-load adjustments
    P_RemoveSlimeTrails();
    P_SegLengths(false);
    memset(st_keyorskull, 0, sizeof st_keyorskull);
    bodyqueslot = 0;
    deathmatch_p = deathmatchstarts;

    P_LoadThings(lumpnum + ML_THINGS);
    if (deathmatch)
    {
        for (int i = 0; i < MAXPLAYERS; ++i)
            if (playeringame[i])
            {
                players[i].mo = NULL;
                G_DeathMatchSpawnPlayer(i);
            }
    }

    // Clear respawn queue and spawn specials
    iquehead = iquetail = 0;
    P_SpawnSpecials();

    // Preload graphics and finalize
    if (precache)
        R_PrecacheLevel();
    P_LevelNameInit();
    crl_spectating = 0;

    // Log load time
    printf("loaded in %d ms.\n", I_GetTimeMS() - starttime);
}

// -----------------------------------------------------------------------------
// P_Init
// -----------------------------------------------------------------------------

void P_Init (void)
{
    P_InitSwitchList();
    P_InitPicAnims();
    R_InitSprites(sprnames);
}
