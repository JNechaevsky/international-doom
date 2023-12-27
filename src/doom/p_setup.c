//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2023 Julia Nechaevskaya
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


#include <SDL.h>  // [JN] SDL_GetTicks()
#include <math.h>
#include "z_zone.h"
#include "deh_main.h"
#include "i_swap.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "d_main.h"
#include "g_game.h"
#include "i_system.h"
#include "w_wad.h"
#include "p_local.h"
#include "s_sound.h"
#include "doomstat.h"
#include "d_englsh.h"
#include "p_extnodes.h" // [crispy] support extended node formats
#include "ct_chat.h"

#include "id_vars.h"
#include "id_func.h"


boolean canmodify;

//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
int		numvertexes;
vertex_t*	vertexes;

int		numsegs;
seg_t*		segs;

int		numsectors;
sector_t*	sectors;

int		numsubsectors;
subsector_t*	subsectors;

int		numnodes;
node_t*		nodes;

int		numlines;
line_t*		lines;

int		numsides;
side_t*		sides;

static int      totallines;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.
int		bmapwidth;
int		bmapheight;	// size in mapblocks
int32_t*	blockmap;	// int for larger maps // [crispy] BLOCKMAP limit
// offsets in blockmap are from here
int32_t*	blockmaplump; // [crispy] BLOCKMAP limit
// origin of block map
fixed_t		bmaporgx;
fixed_t		bmaporgy;
// for thing chains
mobj_t**	blocklinks;		


// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without special effect, this could be
//  used as a PVS lookup as well.
//
byte*		rejectmatrix;


// Maintain single and multi player starting spots.
#define MAX_DEATHMATCH_STARTS	10

mapthing_t	deathmatchstarts[MAX_DEATHMATCH_STARTS];
mapthing_t*	deathmatch_p;
mapthing_t	playerstarts[MAXPLAYERS];
boolean     playerstartsingame[MAXPLAYERS];

// [crispy] recalculate seg offsets
// adapted from prboom-plus/src/p_setup.c:474-482
fixed_t GetOffset(vertex_t *v1, vertex_t *v2)
{
    fixed_t dx, dy;
    fixed_t r;

    dx = (v1->x - v2->x)>>FRACBITS;
    dy = (v1->y - v2->y)>>FRACBITS;
    r = (fixed_t)(sqrt(dx*dx + dy*dy))<<FRACBITS;

    return r;
}


// =============================================================================
//
// [JN] Builtin map names. Set for automap level name.
//
// =============================================================================

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


static char* mapnames[] = // DOOM shareware/registered/retail (Ultimate) names.
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

static char* mapnames_chex[] = // Chex Quest names.
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

static char* mapnames_commercial[] =
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

static char* mapnames_nerve[] =
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


//
// P_LoadVertexes
//
void P_LoadVertexes (int lump)
{
    byte*		data;
    int			i;
    mapvertex_t*	ml;
    vertex_t*		li;

    // Determine number of lumps:
    //  total lump length / vertex record length.
    numvertexes = W_LumpLength (lump) / sizeof(mapvertex_t);

    // Allocate zone memory for buffer.
    vertexes = Z_Malloc (numvertexes*sizeof(vertex_t),PU_LEVEL,0);	

    // Load data into cache.
    data = W_CacheLumpNum (lump, PU_STATIC);
	
    ml = (mapvertex_t *)data;
    li = vertexes;

    // Copy and convert vertex coordinates,
    // internal representation as fixed.
    for (i=0 ; i<numvertexes ; i++, li++, ml++)
    {
	li->x = SHORT(ml->x)<<FRACBITS;
	li->y = SHORT(ml->y)<<FRACBITS;

	// [crispy] initialize vertex coordinates *only* used in rendering
	li->r_x = li->x;
	li->r_y = li->y;
	li->moved = false;
    }

    // Free buffer memory.
    W_ReleaseLumpNum(lump);
}

//
// GetSectorAtNullAddress
//
sector_t* GetSectorAtNullAddress(void)
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

//
// P_LoadSegs
//
void P_LoadSegs (int lump)
{
    byte*		data;
    int			i;
    mapseg_t*		ml;
    seg_t*		li;
    line_t*		ldef;
    int			linedef;
    int			side;
    int                 sidenum;
	
    numsegs = W_LumpLength (lump) / sizeof(mapseg_t);
    segs = Z_Malloc (numsegs*sizeof(seg_t),PU_LEVEL,0);	
    memset (segs, 0, numsegs*sizeof(seg_t));
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    ml = (mapseg_t *)data;
    li = segs;
    for (i=0 ; i<numsegs ; i++, li++, ml++)
    {
	li->v1 = &vertexes[(unsigned short)SHORT(ml->v1)]; // [crispy] extended nodes
	li->v2 = &vertexes[(unsigned short)SHORT(ml->v2)]; // [crispy] extended nodes

	li->angle = (SHORT(ml->angle))<<FRACBITS;
//	li->offset = (SHORT(ml->offset))<<FRACBITS; // [crispy] recalculated below
	linedef = (unsigned short)SHORT(ml->linedef); // [crispy] extended nodes
	ldef = &lines[linedef];
	li->linedef = ldef;
	side = SHORT(ml->side);

	// e6y: check for wrong indexes
	if ((unsigned)linedef >= (unsigned)numlines)
	{
		I_Error("P_LoadSegs: seg %d references a non-existent linedef %d",
			i, (unsigned)linedef);
	}
	if ((unsigned)ldef->sidenum[side] >= (unsigned)numsides)
	{
		I_Error("P_LoadSegs: linedef %d for seg %d references a non-existent sidedef %d",
			linedef, i, (unsigned)ldef->sidenum[side]);
	}

	li->sidedef = &sides[ldef->sidenum[side]];
	li->frontsector = sides[ldef->sidenum[side]].sector;
	// [crispy] recalculate
	li->offset = GetOffset(li->v1, (ml->side ? ldef->v2 : ldef->v1));

        if (ldef-> flags & ML_TWOSIDED)
        {
            sidenum = ldef->sidenum[side ^ 1];

            // If the sidenum is out of range, this may be a "glass hack"
            // impassible window.  Point at side #0 (this may not be
            // the correct Vanilla behavior; however, it seems to work for
            // OTTAWAU.WAD, which is the one place I've seen this trick
            // used).

            if (sidenum < 0 || sidenum >= numsides)
            {
                // [crispy] linedef has two-sided flag set, but no valid second sidedef;
                // but since it has a midtexture, it is supposed to be rendered just
                // like a regular one-sided linedef
                if (li->sidedef->midtexture)
                {
                    li->backsector = 0;
                    fprintf(stderr, "P_LoadSegs: Linedef %d has two-sided flag set, but no second sidedef\n", linedef);
                }
                else
                li->backsector = GetSectorAtNullAddress();
            }
            else
            {
                li->backsector = sides[sidenum].sector;
            }
        }
        else
        {
	    li->backsector = 0;
        }
    }
	
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

void P_SegLengths (boolean contrast_only)
{
    int i;
    // const int rightangle = abs(finesine[(ANG60/2) >> ANGLETOFINESHIFT]);

    for (i = 0; i < numsegs; i++)
    {
	seg_t *const li = &segs[i];
	int64_t dx, dy;

	dx = li->v2->r_x - li->v1->r_x;
	dy = li->v2->r_y - li->v1->r_y;

	if (!contrast_only)
	{
		li->length = (uint32_t)(sqrt((double)dx*dx + (double)dy*dy)/2);

		// [crispy] re-calculate angle used for rendering
		viewx = li->v1->r_x;
		viewy = li->v1->r_y;
		li->r_angle = R_PointToAngleCrispy(li->v2->r_x, li->v2->r_y);
		// [crispy] more than just a little adjustment?
		// back to the original angle then
		if (anglediff(li->r_angle, li->angle) > ANG60/2)
		{
			li->r_angle = li->angle;
		}
	}

	// [crispy] smoother fake contrast
    /*
	if (!dy)
	    li->fakecontrast = -LIGHTBRIGHT;
	else
	if (abs(finesine[li->r_angle >> ANGLETOFINESHIFT]) < rightangle)
	    li->fakecontrast = -(LIGHTBRIGHT >> 1);
	else
	if (!dx)
	    li->fakecontrast = LIGHTBRIGHT;
	else
	if (abs(finecosine[li->r_angle >> ANGLETOFINESHIFT]) < rightangle)
	    li->fakecontrast = LIGHTBRIGHT >> 1;
	else
	    li->fakecontrast = 0;
    */
    }
}

//
// P_LoadSubsectors
//
void P_LoadSubsectors (int lump)
{
    byte*		data;
    int			i;
    mapsubsector_t*	ms;
    subsector_t*	ss;
	
    numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
    subsectors = Z_Malloc (numsubsectors*sizeof(subsector_t),PU_LEVEL,0);	
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    // [crispy] fail on missing subsectors
    if (!data || !numsubsectors)
	I_Error("P_LoadSubsectors: No subsectors in map!");

    ms = (mapsubsector_t *)data;
    memset (subsectors,0, numsubsectors*sizeof(subsector_t));
    ss = subsectors;
    
    for (i=0 ; i<numsubsectors ; i++, ss++, ms++)
    {
	ss->numlines = (unsigned short)SHORT(ms->numsegs); // [crispy] extended nodes
	ss->firstline = (unsigned short)SHORT(ms->firstseg); // [crispy] extended nodes
    }
	
    W_ReleaseLumpNum(lump);
}



//
// P_LoadSectors
//
void P_LoadSectors (int lump)
{
    byte*		data;
    int			i;
    mapsector_t*	ms;
    sector_t*		ss;
	
    // [crispy] fail on missing sectors
    if (lump >= numlumps)
	I_Error("P_LoadSectors: No sectors in map!");

    numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
    sectors = Z_Malloc (numsectors*sizeof(sector_t),PU_LEVEL,0);	
    memset (sectors, 0, numsectors*sizeof(sector_t));
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    // [crispy] fail on missing sectors
    if (!data || !numsectors)
	I_Error("P_LoadSectors: No sectors in map!");

    ms = (mapsector_t *)data;
    ss = sectors;
    for (i=0 ; i<numsectors ; i++, ss++, ms++)
    {
	ss->floorheight = SHORT(ms->floorheight)<<FRACBITS;
	ss->ceilingheight = SHORT(ms->ceilingheight)<<FRACBITS;
	ss->floorpic = R_FlatNumForName(ms->floorpic);
	ss->ceilingpic = R_FlatNumForName(ms->ceilingpic);
	ss->lightlevel = SHORT(ms->lightlevel);
	ss->special = SHORT(ms->special);
	ss->tag = SHORT(ms->tag);
	ss->thinglist = NULL;
	// [crispy] WiggleFix: [kb] for R_FixWiggle()
	ss->cachedheight = 0;
        // [AM] Sector interpolation.  Even if we're
        //      not running uncapped, the renderer still
        //      uses this data.
        ss->oldfloorheight = ss->floorheight;
        ss->interpfloorheight = ss->floorheight;
        ss->oldceilingheight = ss->ceilingheight;
        ss->interpceilingheight = ss->ceilingheight;
        // [crispy] inhibit sector interpolation during the 0th gametic
        ss->oldgametic = -1;
    }
	
    W_ReleaseLumpNum(lump);
}


//
// P_LoadNodes
//
void P_LoadNodes (int lump)
{
    byte*	data;
    int		i;
    int		j;
    int		k;
    mapnode_t*	mn;
    node_t*	no;
	
    numnodes = W_LumpLength (lump) / sizeof(mapnode_t);
    nodes = Z_Malloc (numnodes*sizeof(node_t),PU_LEVEL,0);	
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    // [crispy] warn about missing nodes
    if (!data || !numnodes)
    {
	if (numsubsectors == 1)
	    fprintf(stderr, "P_LoadNodes: No nodes in map, but only one subsector.\n");
	else
	    I_Error("P_LoadNodes: No nodes in map!");
    }

    mn = (mapnode_t *)data;
    no = nodes;
    
    for (i=0 ; i<numnodes ; i++, no++, mn++)
    {
	no->x = SHORT(mn->x)<<FRACBITS;
	no->y = SHORT(mn->y)<<FRACBITS;
	no->dx = SHORT(mn->dx)<<FRACBITS;
	no->dy = SHORT(mn->dy)<<FRACBITS;
	for (j=0 ; j<2 ; j++)
	{
	    no->children[j] = (unsigned short)SHORT(mn->children[j]); // [crispy] extended nodes

	    // [crispy] add support for extended nodes
	    // from prboom-plus/src/p_setup.c:937-957
	    if (no->children[j] == NO_INDEX)
		no->children[j] = -1;
	    else
	    if (no->children[j] & NF_SUBSECTOR_VANILLA)
	    {
		no->children[j] &= ~NF_SUBSECTOR_VANILLA;

		if (no->children[j] >= numsubsectors)
		    no->children[j] = 0;

		no->children[j] |= NF_SUBSECTOR;
	    }

	    for (k=0 ; k<4 ; k++)
		no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
	}
    }
	
    W_ReleaseLumpNum(lump);
}


//
// P_LoadThings
//
void P_LoadThings (int lump)
{
    byte               *data;
    int			i;
    mapthing_t         *mt;
    mapthing_t          spawnthing;
    int			numthings;
    boolean		spawn;

    data = W_CacheLumpNum (lump,PU_STATIC);
    numthings = W_LumpLength (lump) / sizeof(mapthing_t);
	
    mt = (mapthing_t *)data;
    for (i=0 ; i<numthings ; i++, mt++)
    {
	spawn = true;

	// Do not spawn cool, new monsters if !commercial
	if (gamemode != commercial)
	{
	    switch (SHORT(mt->type))
	    {
	      case 68:	// Arachnotron
	      case 64:	// Archvile
	      case 88:	// Boss Brain
	      case 89:	// Boss Shooter
	      case 69:	// Hell Knight
	      case 67:	// Mancubus
	      case 71:	// Pain Elemental
	      case 65:	// Former Human Commando
	      case 66:	// Revenant
	      case 84:	// Wolf SS
		spawn = false;
		break;
	    }
	}
	if (spawn == false)
	    break;

	// Do spawn all other stuff. 
	spawnthing.x = SHORT(mt->x);
	spawnthing.y = SHORT(mt->y);
	spawnthing.angle = SHORT(mt->angle);
	spawnthing.type = SHORT(mt->type);
	spawnthing.options = SHORT(mt->options);
	
	P_SpawnMapThing(&spawnthing);
    }

    if (!deathmatch)
    {
        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (playeringame[i] && !playerstartsingame[i])
            {
                I_Error("P_LoadThings: Player %d start missing (vanilla crashes here)", i + 1);
            }
            playerstartsingame[i] = false;
        }
    }

    W_ReleaseLumpNum(lump);
}


//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//
void P_LoadLineDefs (int lump)
{
    byte*		data;
    int			i;
    maplinedef_t*	mld;
    line_t*		ld;
    vertex_t*		v1;
    vertex_t*		v2;
    int warn, warn2; // [crispy] warn about invalid linedefs
	
    numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
    lines = Z_Malloc (numlines*sizeof(line_t),PU_LEVEL,0);	
    memset (lines, 0, numlines*sizeof(line_t));
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    mld = (maplinedef_t *)data;
    ld = lines;
    warn = warn2 = 0; // [crispy] warn about invalid linedefs
    for (i=0 ; i<numlines ; i++, mld++, ld++)
    {
	ld->flags = (unsigned short)SHORT(mld->flags); // [crispy] extended nodes
	ld->special = SHORT(mld->special);
	// [crispy] warn about unknown linedef types
	if ((unsigned short) ld->special > 141 && ld->special != 271 && ld->special != 272)
	{
	    fprintf(stderr, "P_LoadLineDefs: Unknown special %d at line %d.\n", ld->special, i);
	    warn++;
	}
	ld->tag = SHORT(mld->tag);
	// [crispy] warn about special linedefs without tag
	if (ld->special && !ld->tag)
	{
	    switch (ld->special)
	    {
		case 1:	// Vertical Door
		case 26:	// Blue Door/Locked
		case 27:	// Yellow Door /Locked
		case 28:	// Red Door /Locked
		case 31:	// Manual door open
		case 32:	// Blue locked door open
		case 33:	// Red locked door open
		case 34:	// Yellow locked door open
		case 117:	// Blazing door raise
		case 118:	// Blazing door open
		case 271:	// MBF sky transfers
		case 272:
		case 48:	// Scroll Wall Left
		case 85:	// [crispy] [JN] (Boom) Scroll Texture Right
		case 11:	// s1 Exit level
		case 51:	// s1 Secret exit
		case 52:	// w1 Exit level
		case 124:	// w1 Secret exit
		    break;
		default:
		    fprintf(stderr, "P_LoadLineDefs: Special linedef %d without tag.\n", i);
		    warn2++;
		    break;
	    }
	}
	v1 = ld->v1 = &vertexes[(unsigned short)SHORT(mld->v1)]; // [crispy] extended nodes
	v2 = ld->v2 = &vertexes[(unsigned short)SHORT(mld->v2)]; // [crispy] extended nodes
	ld->dx = v2->x - v1->x;
	ld->dy = v2->y - v1->y;
	
	if (!ld->dx)
	    ld->slopetype = ST_VERTICAL;
	else if (!ld->dy)
	    ld->slopetype = ST_HORIZONTAL;
	else
	{
	    if (FixedDiv (ld->dy , ld->dx) > 0)
		ld->slopetype = ST_POSITIVE;
	    else
		ld->slopetype = ST_NEGATIVE;
	}
		
	if (v1->x < v2->x)
	{
	    ld->bbox[BOXLEFT] = v1->x;
	    ld->bbox[BOXRIGHT] = v2->x;
	}
	else
	{
	    ld->bbox[BOXLEFT] = v2->x;
	    ld->bbox[BOXRIGHT] = v1->x;
	}

	if (v1->y < v2->y)
	{
	    ld->bbox[BOXBOTTOM] = v1->y;
	    ld->bbox[BOXTOP] = v2->y;
	}
	else
	{
	    ld->bbox[BOXBOTTOM] = v2->y;
	    ld->bbox[BOXTOP] = v1->y;
	}

	// [crispy] calculate sound origin of line to be its midpoint
	ld->soundorg.x = ld->bbox[BOXLEFT] / 2 + ld->bbox[BOXRIGHT] / 2;
	ld->soundorg.y = ld->bbox[BOXTOP] / 2 + ld->bbox[BOXBOTTOM] / 2;

	ld->sidenum[0] = SHORT(mld->sidenum[0]);
	ld->sidenum[1] = SHORT(mld->sidenum[1]);

	// [crispy] substitute dummy sidedef for missing right side
	if (ld->sidenum[0] == NO_INDEX)
	{
	    ld->sidenum[0] = 0;
	    fprintf(stderr, "P_LoadLineDefs: linedef %d without first sidedef!\n", i);
	}

	if (ld->sidenum[0] != NO_INDEX) // [crispy] extended nodes
	    ld->frontsector = sides[ld->sidenum[0]].sector;
	else
	    ld->frontsector = 0;

	if (ld->sidenum[1] != NO_INDEX) // [crispy] extended nodes
	    ld->backsector = sides[ld->sidenum[1]].sector;
	else
	    ld->backsector = 0;
    }

    // [crispy] warn about unknown linedef types
    if (warn)
    {
	fprintf(stderr, "P_LoadLineDefs: Found %d line%s with unknown linedef type.\n", warn, (warn > 1) ? "s" : "");
    }
    // [crispy] warn about special linedefs without tag
    if (warn2)
    {
	fprintf(stderr, "P_LoadLineDefs: Found %d special linedef%s without tag.\n", warn2, (warn2 > 1) ? "s" : "");
    }
    if (warn || warn2)
    {
	fprintf(stderr, "THIS MAP MAY NOT WORK AS EXPECTED!\n");
    }

    W_ReleaseLumpNum(lump);
}


//
// P_LoadSideDefs
//
void P_LoadSideDefs (int lump)
{
    byte*		data;
    int			i;
    mapsidedef_t*	msd;
    side_t*		sd;
	
    numsides = W_LumpLength (lump) / sizeof(mapsidedef_t);
    sides = Z_Malloc (numsides*sizeof(side_t),PU_LEVEL,0);	
    memset (sides, 0, numsides*sizeof(side_t));
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    msd = (mapsidedef_t *)data;
    sd = sides;
    for (i=0 ; i<numsides ; i++, msd++, sd++)
    {
	sd->textureoffset = SHORT(msd->textureoffset)<<FRACBITS;
	sd->rowoffset = SHORT(msd->rowoffset)<<FRACBITS;
	sd->toptexture = R_TextureNumForName(msd->toptexture);
	sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
	sd->midtexture = R_TextureNumForName(msd->midtexture);
	sd->sector = &sectors[SHORT(msd->sector)];
	// [crispy] smooth texture scrolling
	sd->basetextureoffset = sd->textureoffset;
    }

    W_ReleaseLumpNum(lump);
}


//
// P_LoadBlockMap
//
boolean P_LoadBlockMap (int lump)
{
    int i;
    int count;
    int lumplen;
    short *wadblockmaplump;

    // [crispy] (re-)create BLOCKMAP if necessary
    if (M_CheckParm("-blockmap") ||
        lump >= numlumps ||
        (lumplen = W_LumpLength(lump)) < 8 ||
        (count = lumplen / 2) >= 0x10000)
    {
	return false;
    }
	
    // [crispy] remove BLOCKMAP limit
    // adapted from boom202s/P_SETUP.C:1025-1076
    wadblockmaplump = Z_Malloc(lumplen, PU_LEVEL, NULL);
    W_ReadLump(lump, wadblockmaplump);
    blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, NULL);
    blockmap = blockmaplump + 4;

    blockmaplump[0] = SHORT(wadblockmaplump[0]);
    blockmaplump[1] = SHORT(wadblockmaplump[1]);
    blockmaplump[2] = (int32_t)(SHORT(wadblockmaplump[2])) & 0xffff;
    blockmaplump[3] = (int32_t)(SHORT(wadblockmaplump[3])) & 0xffff;

    // Swap all short integers to native byte ordering.
  
    for (i=4; i<count; i++)
    {
	short t = SHORT(wadblockmaplump[i]);
	blockmaplump[i] = (t == -1) ? -1l : (int32_t) t & 0xffff;
    }

    Z_Free(wadblockmaplump);
		
    // Read the header

    bmaporgx = blockmaplump[0]<<FRACBITS;
    bmaporgy = blockmaplump[1]<<FRACBITS;
    bmapwidth = blockmaplump[2];
    bmapheight = blockmaplump[3];
	
    // Clear out mobj chains

    count = sizeof(*blocklinks) * bmapwidth * bmapheight;
    blocklinks = Z_Malloc(count, PU_LEVEL, 0);
    memset(blocklinks, 0, count);

    return true;
}

// -----------------------------------------------------------------------------
// P_CreateBlockMap
// [crispy] taken from mbfsrc/P_SETUP.C:547-707, slightly adapted
// -----------------------------------------------------------------------------

static void P_CreateBlockMap (void)
{
    int i;
    fixed_t minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;

    // First find limits of map

    for (i=0; i<numvertexes; i++)
    {
        if (vertexes[i].x >> FRACBITS < minx)
        {
            minx = vertexes[i].x >> FRACBITS;
        }
        else if (vertexes[i].x >> FRACBITS > maxx)
        {
            maxx = vertexes[i].x >> FRACBITS;
        }
        if (vertexes[i].y >> FRACBITS < miny)
        {
            miny = vertexes[i].y >> FRACBITS;
        }
        else if (vertexes[i].y >> FRACBITS > maxy)
        {
            maxy = vertexes[i].y >> FRACBITS;
        }
    }

    // Save blockmap parameters

    bmaporgx = minx << FRACBITS;
    bmaporgy = miny << FRACBITS;
    bmapwidth  = ((maxx-minx) >> MAPBTOFRAC) + 1;
    bmapheight = ((maxy-miny) >> MAPBTOFRAC) + 1;

    // Compute blockmap, which is stored as a 2d array of variable-sized lists.
    //
    // Pseudocode:
    //
    // For each linedef:
    //
    //   Map the starting and ending vertices to blocks.
    //
    //   Starting in the starting vertex's block, do:
    //
    //     Add linedef to current block's list, dynamically resizing it.
    //
    //     If current block is the same as the ending vertex's block, exit loop.
    //
    //     Move to an adjacent block by moving towards the ending block in
    //     either the x or y direction, to the block which contains the linedef.

    {
        typedef struct { int n, nalloc, *list; } bmap_t;  // blocklist structure
        unsigned tot = bmapwidth * bmapheight;            // size of blockmap
        bmap_t *bmap = calloc(sizeof *bmap, tot);         // array of blocklists
        int x, y, adx, ady, bend;

        for (i=0; i < numlines; i++)
        {
            int dx, dy, diff, b;

            // starting coordinates
            x = (lines[i].v1->x >> FRACBITS) - minx;
            y = (lines[i].v1->y >> FRACBITS) - miny;

            // x-y deltas
            adx = lines[i].dx >> FRACBITS, dx = adx < 0 ? -1 : 1;
            ady = lines[i].dy >> FRACBITS, dy = ady < 0 ? -1 : 1;

            // difference in preferring to move across y (>0) instead of x (<0)
            diff = !adx ? 1 : !ady ? -1 :
            (((x >> MAPBTOFRAC) << MAPBTOFRAC) +
            (dx > 0 ? MAPBLOCKUNITS-1 : 0) - x) * (ady = abs(ady)) * dx -
            (((y >> MAPBTOFRAC) << MAPBTOFRAC) +
            (dy > 0 ? MAPBLOCKUNITS-1 : 0) - y) * (adx = abs(adx)) * dy;

            // starting block, and pointer to its blocklist structure
            b = (y >> MAPBTOFRAC)*bmapwidth + (x >> MAPBTOFRAC);

            // ending block
            bend = (((lines[i].v2->y >> FRACBITS) - miny) >> MAPBTOFRAC)
                 * bmapwidth + (((lines[i].v2->x >> FRACBITS) - minx) >> MAPBTOFRAC);

            // delta for pointer when moving across y
            dy *= bmapwidth;

            // deltas for diff inside the loop
            adx <<= MAPBTOFRAC;
            ady <<= MAPBTOFRAC;

            // Now we simply iterate block-by-block until we reach the end block.
            while ((unsigned) b < tot)    // failsafe -- should ALWAYS be true
            {
                // Increase size of allocated list if necessary
                if (bmap[b].n >= bmap[b].nalloc)
                    bmap[b].list = I_Realloc(bmap[b].list,
                   (bmap[b].nalloc = bmap[b].nalloc ?
                    bmap[b].nalloc*2 : 8)*sizeof*bmap->list);

                // Add linedef to end of list
                bmap[b].list[bmap[b].n++] = i;

                // If we have reached the last block, exit
                if (b == bend)
                {
                    break;
                }

                // Move in either the x or y direction to the next block
                if (diff < 0)
                {
                    diff += ady, b += dx;
                }
                else
                {
                    diff -= adx, b += dy;
                }
            }
        }

        // Compute the total size of the blockmap.
        //
        // Compression of empty blocks is performed by reserving two offset words
        // at tot and tot+1.
        //
        // 4 words, unused if this routine is called, are reserved at the start.

        {
            int count = tot+6;  // we need at least 1 word per block, plus reserved's
        
            for (unsigned int t = 0; t < tot; t++)
                if (bmap[t].n)
                    count += bmap[t].n + 2; // 1 header word + 1 trailer word + blocklist
        
            // Allocate blockmap lump with computed count
            blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);
        }

        // Now compress the blockmap.
        {
            int ndx = tot += 4;         // Advance index to start of linedef lists
            bmap_t *bp = bmap;          // Start of uncompressed blockmap

            blockmaplump[ndx++] = 0;    // Store an empty blockmap list at start
            blockmaplump[ndx++] = -1;   // (Used for compression)

            for (unsigned int t = 4; t < tot; t++, bp++)
                if (bp->n)                                      // Non-empty blocklist
                {
                    blockmaplump[blockmaplump[t] = ndx++] = 0;  // Store index & header
                    do
                    blockmaplump[ndx++] = bp->list[--bp->n];    // Copy linedef list
                    while (bp->n);
                    blockmaplump[ndx++] = -1;                   // Store trailer
                    free(bp->list);                             // Free linedef list
                }
                else            // Empty blocklist: point to reserved empty blocklist
                blockmaplump[t] = tot;
        
            free(bmap);    // Free uncompressed blockmap
        }
    }

    // [crispy] copied over from P_LoadBlockMap()
    {
        int count = sizeof(*blocklinks) * bmapwidth * bmapheight;
        blocklinks = Z_Malloc(count, PU_LEVEL, 0);
        memset(blocklinks, 0, count);
        blockmap = blockmaplump+4;
    }
}


//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void P_GroupLines (void)
{
    line_t**		linebuffer;
    int			i;
    int			j;
    line_t*		li;
    sector_t*		sector;
    fixed_t		bbox[4];
    int			block;
	
    // look up sector number for each subsector
    // [JN] Fix infinite loop is subsector is not a part of any sector.
    // Written by figgi, adapted from Pr-Boom+.
    for (i=0 ; i<numsubsectors ; i++)
    {
	    seg_t *seg = &segs[subsectors[i].firstline];
	    subsectors[i].sector = NULL;

	    for (j=0 ; j<subsectors[i].numlines ; j++)
	    {
	        if(seg->sidedef)
	        {
	            subsectors[i].sector = seg->sidedef->sector;
	            break;
	        }
	        seg++;
	    }
	    if (subsectors[i].sector == NULL)
	    {
	        I_Error("P_GroupLines: Subsector %d is not a part of any sector!\n", i);
	    }
    }

    // count number of lines in each sector
    li = lines;
    totallines = 0;
    for (i=0 ; i<numlines ; i++, li++)
    {
	totallines++;
	li->frontsector->linecount++;

	if (li->backsector && li->backsector != li->frontsector)
	{
	    li->backsector->linecount++;
	    totallines++;
	}
    }

    // build line tables for each sector	
    linebuffer = Z_Malloc (totallines*sizeof(line_t *), PU_LEVEL, 0);

    for (i=0; i<numsectors; ++i)
    {
        // Assign the line buffer for this sector

        sectors[i].lines = linebuffer;
        linebuffer += sectors[i].linecount;

        // Reset linecount to zero so in the next stage we can count
        // lines into the list.

        sectors[i].linecount = 0;
    }

    // Assign lines to sectors

    for (i=0; i<numlines; ++i)
    { 
        li = &lines[i];

        if (li->frontsector != NULL)
        {
            sector = li->frontsector;

            sector->lines[sector->linecount] = li;
            ++sector->linecount;
        }

        if (li->backsector != NULL && li->frontsector != li->backsector)
        {
            sector = li->backsector;

            sector->lines[sector->linecount] = li;
            ++sector->linecount;
        }
    }
    
    // Generate bounding boxes for sectors
	
    sector = sectors;
    for (i=0 ; i<numsectors ; i++, sector++)
    {
	M_ClearBox (bbox);

	for (j=0 ; j<sector->linecount; j++)
	{
            li = sector->lines[j];

            M_AddToBox (bbox, li->v1->x, li->v1->y);
            M_AddToBox (bbox, li->v2->x, li->v2->y);
	}

	// set the degenmobj_t to the middle of the bounding box
	sector->soundorg.x = (bbox[BOXRIGHT]+bbox[BOXLEFT])/2;
	sector->soundorg.y = (bbox[BOXTOP]+bbox[BOXBOTTOM])/2;
		
	// adjust bounding box to map blocks
	block = (bbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapheight ? bmapheight-1 : block;
	sector->blockbox[BOXTOP]=block;

	block = (bbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	sector->blockbox[BOXBOTTOM]=block;

	block = (bbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapwidth ? bmapwidth-1 : block;
	sector->blockbox[BOXRIGHT]=block;

	block = (bbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	sector->blockbox[BOXLEFT]=block;
    }
	
}

// [crispy] remove slime trails
// mostly taken from Lee Killough's implementation in mbfsrc/P_SETUP.C:849-924,
// with the exception that not the actual vertex coordinates are modified,
// but separate coordinates that are *only* used in rendering,
// i.e. r_bsp.c:R_AddLine()

static void P_RemoveSlimeTrails(void)
{
    int i;

    for (i = 0; i < numsegs; i++)
    {
	const line_t *l = segs[i].linedef;
	vertex_t *v = segs[i].v1;

	// [crispy] ignore exactly vertical or horizontal linedefs
	if (l->dx && l->dy)
	{
	    do
	    {
		// [crispy] vertex wasn't already moved
		if (!v->moved)
		{
		    v->moved = true;
		    // [crispy] ignore endpoints of linedefs
		    if (v != l->v1 && v != l->v2)
		    {
			// [crispy] move the vertex towards the linedef
			// by projecting it using the law of cosines
			int64_t dx2 = (l->dx >> FRACBITS) * (l->dx >> FRACBITS);
			int64_t dy2 = (l->dy >> FRACBITS) * (l->dy >> FRACBITS);
			int64_t dxy = (l->dx >> FRACBITS) * (l->dy >> FRACBITS);
			int64_t s = dx2 + dy2;

			// [crispy] MBF actually overrides v->x and v->y here
			v->r_x = (fixed_t)((dx2 * v->x + dy2 * l->v1->x + dxy * (v->y - l->v1->y)) / s);
			v->r_y = (fixed_t)((dy2 * v->y + dx2 * l->v1->y + dxy * (v->x - l->v1->x)) / s);

			// [crispy] wait a minute... moved more than 8 map units?
			// maybe that's a linguortal then, back to the original coordinates
			if (abs(v->r_x - v->x) > 8*FRACUNIT || abs(v->r_y - v->y) > 8*FRACUNIT)
			{
			    v->r_x = v->x;
			    v->r_y = v->y;
			}
		    }
		}
	    // [crispy] if v doesn't point to the second vertex of the seg already, point it there
	    } while ((v != segs[i].v2) && (v = segs[i].v2));
	}
    }
}

// Pad the REJECT lump with extra data when the lump is too small,
// to simulate a REJECT buffer overflow in Vanilla Doom.

static void PadRejectArray(byte *array, unsigned int len)
{
    unsigned int i;
    unsigned int byte_num;
    byte *dest;
    unsigned int padvalue;

    // Values to pad the REJECT array with:

    unsigned int rejectpad[4] =
    {
        0,                                    // Size
        0,                                    // Part of z_zone block header
        50,                                   // PU_LEVEL
        0x1d4a11                              // DOOM_CONST_ZONEID
    };

    rejectpad[0] = ((totallines * 4 + 3) & ~3) + 24;

    // Copy values from rejectpad into the destination array.

    dest = array;

    for (i=0; i<len && i<sizeof(rejectpad); ++i)
    {
        byte_num = i % 4;
        *dest = (rejectpad[i / 4] >> (byte_num * 8)) & 0xff;
        ++dest;
    }

    // We only have a limited pad size.  Print a warning if the
    // REJECT lump is too small.

    if (len > sizeof(rejectpad))
    {
        fprintf(stderr, "PadRejectArray: REJECT lump too short to pad! (%u > %i)\n",
                        len, (int) sizeof(rejectpad));

        // Pad remaining space with 0 (or 0xff, if specified on command line).

        if (M_CheckParm("-reject_pad_with_ff"))
        {
            padvalue = 0xff;
        }
        else
        {
            padvalue = 0x00;
        }

        memset(array + sizeof(rejectpad), padvalue, len - sizeof(rejectpad));
    }
}

static void P_LoadReject(int lumpnum)
{
    int minlength;
    int lumplen;

    // Calculate the size that the REJECT lump *should* be.

    minlength = (numsectors * numsectors + 7) / 8;

    // If the lump meets the minimum length, it can be loaded directly.
    // Otherwise, we need to allocate a buffer of the correct size
    // and pad it with appropriate data.

    lumplen = W_LumpLength(lumpnum);

    if (lumplen >= minlength)
    {
        rejectmatrix = W_CacheLumpNum(lumpnum, PU_LEVEL);
    }
    else
    {
        rejectmatrix = Z_Malloc(minlength, PU_LEVEL, &rejectmatrix);
        W_ReadLump(lumpnum, rejectmatrix);

        PadRejectArray(rejectmatrix + lumplen, minlength - lumplen);
    }
}

//
// P_SetupLevel
//
void
P_SetupLevel
( int		episode,
  int		map)
{
    int		i;
    char	lumpname[9];
    int		lumpnum;
    boolean	crispy_validblockmap;
    mapformat_t	crispy_mapformat;
    // [JN] Indicate level loading time in console.
    const int starttime = SDL_GetTicks();
	
    totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
    wminfo.partime = 180;
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	players[i].killcount = players[i].extrakillcount 
	    = players[i].secretcount = players[i].itemcount = 0;
    }

    // Initial height of PointOfView
    // will be set by player think.
    players[consoleplayer].viewz = 1; 

    // [crispy] stop demo warp mode now
    if (demowarp == map)
    {
	demowarp = 0;
	nodrawers = false;
	singletics = false;
    }

    // Make sure all sounds are stopped before Z_FreeTags.
    S_Start ();			

    Z_FreeTags (PU_LEVEL, PU_PURGELEVEL-1);

    // UNUSED W_Profile ();
    P_InitThinkers ();

    // if working with a devlopment map, reload it
    W_Reload ();

    // find map name
    if (gamemode == commercial)
    {
        DEH_snprintf(lumpname, 9, "MAP%02d", map);
    }
    else
    {
        DEH_snprintf(lumpname, 9, "E%dM%d", episode, map);
    }

    lumpnum = W_GetNumForName (lumpname);
	
    // [JN] Checking for multiple map lump names for allowing map fixes to work.
    // Adaptaken from DOOM Retro, thanks Brad Harding!
    //  Fixes also should not work for: network game, shareware, IWAD versions below 1.9,
    //  vanilla game mode, Press Beta, Atari Jaguar, Freedoom and FreeDM.
    canmodify = (W_CheckMultipleLumps(lumpname) == 1
             && gamemode != shareware
             && gameversion >= exe_doom_1_9
             && gamevariant != freedoom && gamevariant != freedm);

    leveltime = 0;
    realleveltime = 0;
    oldleveltime = 0;
	
    // [JN] Indicate the map we are loading.
    if (gamemode == commercial)
    {
        fprintf(stderr, "P_SetupLevel: MAP%02d, ", gamemap);
    }
    else
    {
        fprintf(stderr, "P_SetupLevel: E%dM%d, ", gameepisode, gamemap);
    }

    // [crispy] check and log map and nodes format
    crispy_mapformat = P_CheckMapFormat(lumpnum);

    // note: most of this ordering is important	
    crispy_validblockmap = P_LoadBlockMap (lumpnum+ML_BLOCKMAP); // [crispy] (re-)create BLOCKMAP if necessary
    P_LoadVertexes (lumpnum+ML_VERTEXES);
    P_LoadSectors (lumpnum+ML_SECTORS);
    P_LoadSideDefs (lumpnum+ML_SIDEDEFS);

    if (crispy_mapformat & MFMT_HEXEN)
	P_LoadLineDefs_Hexen (lumpnum+ML_LINEDEFS);
    else
    P_LoadLineDefs (lumpnum+ML_LINEDEFS);
    // [crispy] (re-)create BLOCKMAP if necessary
    if (!crispy_validblockmap)
    {
	P_CreateBlockMap();
    }
    if (crispy_mapformat & (MFMT_ZDBSPX | MFMT_ZDBSPZ))
	P_LoadNodes_ZDBSP (lumpnum+ML_NODES, crispy_mapformat & MFMT_ZDBSPZ);
    else
    if (crispy_mapformat & MFMT_DEEPBSP)
    {
	P_LoadSubsectors_DeePBSP (lumpnum+ML_SSECTORS);
	P_LoadNodes_DeePBSP (lumpnum+ML_NODES);
	P_LoadSegs_DeePBSP (lumpnum+ML_SEGS);
    }
    else
    {
    P_LoadSubsectors (lumpnum+ML_SSECTORS);
    P_LoadNodes (lumpnum+ML_NODES);
    P_LoadSegs (lumpnum+ML_SEGS);
    }

    P_GroupLines ();
    P_LoadReject (lumpnum+ML_REJECT);

    // [crispy] remove slime trails
    P_RemoveSlimeTrails();
    // [crispy] fix long wall wobble
    P_SegLengths(false);
    // [crispy] blinking key or skull in the status bar
    memset(st_keyorskull, 0, sizeof(st_keyorskull));

    bodyqueslot = 0;
    deathmatch_p = deathmatchstarts;
    P_LoadThings (lumpnum+ML_THINGS);
    
    // if deathmatch, randomly spawn the active players
    if (deathmatch)
    {
	for (i=0 ; i<MAXPLAYERS ; i++)
	    if (playeringame[i])
	    {
		players[i].mo = NULL;
		G_DeathMatchSpawnPlayer (i);
	    }
			
    }

    // clear special respawning que
    iquehead = iquetail = 0;		
	
    // set up world state
    P_SpawnSpecials ();
	
    // build subsector connect matrix
    //	UNUSED P_ConnectSubsectors ();

    // preload graphics
    if (precache)
	R_PrecacheLevel ();

    // [JN] Set level name.
    P_LevelNameInit();

    // [JN] Force to disable spectator mode.
    crl_spectating = 0;

    // [JN] Print amount of level loading time.
    printf("loaded in %d ms.\n", SDL_GetTicks() - starttime);

    //printf ("free memory: 0x%x\n", Z_FreeMemory());

}



//
// P_Init
//
void P_Init (void)
{
    P_InitSwitchList ();
    P_InitPicAnims ();
    R_InitSprites (sprnames);
}



