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


// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <stdlib.h>
#include "h2def.h"
#include "i_system.h"
#include "i_timer.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_misc.h"
#include "i_swap.h"
#include "s_sound.h"
#include "p_local.h"

// [crispy] support maps with compressed ZDBSP nodes
// [JN] Support via MINIZ library, thanks Leonid Murin (Dasperal).
#include "miniz.h"

// MACROS ------------------------------------------------------------------

#define MAPINFO_SCRIPT_NAME "MAPINFO"
#define MCMD_SKY1 1
#define MCMD_SKY2 2
#define MCMD_LIGHTNING 3
#define MCMD_FADETABLE 4
#define MCMD_DOUBLESKY 5
#define MCMD_CLUSTER 6
#define MCMD_WARPTRANS 7
#define MCMD_NEXT 8
#define MCMD_CDTRACK 9
#define MCMD_CD_STARTTRACK 10
#define MCMD_CD_END1TRACK 11
#define MCMD_CD_END2TRACK 12
#define MCMD_CD_END3TRACK 13
#define MCMD_CD_INTERTRACK 14
#define MCMD_CD_TITLETRACK 15

#define UNKNOWN_MAP_NAME "DEVELOPMENT MAP"
#define DEFAULT_SKY_NAME "SKY1"
#define DEFAULT_SONG_LUMP "DEFSONG"
#define DEFAULT_FADE_TABLE "COLORMAP"

// TYPES -------------------------------------------------------------------

typedef struct mapInfo_s mapInfo_t;
struct mapInfo_s
{
    short cluster;
    short warpTrans;
    short nextMap;
    short cdTrack;
    char name[32];
    short sky1Texture;
    short sky2Texture;
    fixed_t sky1ScrollDelta;
    fixed_t sky2ScrollDelta;
    boolean doubleSky;
    boolean lightning;
    int fadetable;
    char songLump[10];
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void P_SpawnMapThing(mapthing_t * mthing);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean P_GetMapFadeTable(int map);
static int QualifyMap(int map);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int MapCount;
mapthing_t deathmatchstarts[MAXDEATHMATCHSTARTS], *deathmatch_p;
mapthing_t playerstarts[MAX_PLAYER_STARTS][MAXPLAYERS];
boolean playerstartsingame[MAXPLAYERS];
int numvertexes;
vertex_t *vertexes;
int numsegs;
seg_t *segs;
int numsectors;
sector_t *sectors;
int numsubsectors;
subsector_t *subsectors;
int numnodes;
node_t *nodes;
int numlines;
line_t *lines;
int numsides;
side_t *sides;
int32_t *blockmaplump;            // offsets in blockmap are from here
int32_t *blockmap;
int bmapwidth, bmapheight;      // in mapblocks
fixed_t bmaporgx, bmaporgy;     // origin of block map
mobj_t **blocklinks;            // for thing chains
byte *rejectmatrix;             // for fast sight rejection

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mapInfo_t MapInfo[99];
static const char *MapCmdNames[] = {
    "SKY1",
    "SKY2",
    "DOUBLESKY",
    "LIGHTNING",
    "FADETABLE",
    "CLUSTER",
    "WARPTRANS",
    "NEXT",
    "CDTRACK",
    "CD_START_TRACK",
    "CD_END1_TRACK",
    "CD_END2_TRACK",
    "CD_END3_TRACK",
    "CD_INTERMISSION_TRACK",
    "CD_TITLE_TRACK",
    NULL
};
static int MapCmdIDs[] = {
    MCMD_SKY1,
    MCMD_SKY2,
    MCMD_DOUBLESKY,
    MCMD_LIGHTNING,
    MCMD_FADETABLE,
    MCMD_CLUSTER,
    MCMD_WARPTRANS,
    MCMD_NEXT,
    MCMD_CDTRACK,
    MCMD_CD_STARTTRACK,
    MCMD_CD_END1TRACK,
    MCMD_CD_END2TRACK,
    MCMD_CD_END3TRACK,
    MCMD_CD_INTERTRACK,
    MCMD_CD_TITLETRACK
};

static int cd_NonLevelTracks[6];        // Non-level specific song cd track numbers 

typedef enum
{
    MFMT_DOOMBSP = 0x000,
    MFMT_DEEPBSP = 0x001,
    MFMT_ZDBSPX  = 0x002,
    MFMT_ZDBSPZ  = 0x004,
    MFMT_HEXEN   = 0x100,
} mapformat_t;

// CODE --------------------------------------------------------------------

// [crispy] recalculate seg offsets
// adapted from prboom-plus/src/p_setup.c:474-482
static fixed_t GetOffset(const vertex_t *v1, const vertex_t *v2)
{
    fixed_t dx, dy;
    fixed_t r;

    dx = (v1->x - v2->x)>>FRACBITS;
    dy = (v1->y - v2->y)>>FRACBITS;
    r = (fixed_t)(sqrt(dx*dx + dy*dy))<<FRACBITS;

    return r;
}

// [crispy] support maps with NODES in compressed or uncompressed ZDBSP
// format or DeePBSP format and/or LINEDEFS and THINGS lumps in Hexen format
static mapformat_t P_CheckMapFormat(int lumpnum)
{
    mapformat_t format = 0;
    const byte *chk_nodes = NULL;
    int b;

    if ((b = lumpnum+ML_BLOCKMAP+1) < numlumps &&
        !strncasecmp(lumpinfo[b]->name, "BEHAVIOR", 8))
    {
	fprintf(stderr, "Hexen format (");
	format |= MFMT_HEXEN;
    }
    else
	fprintf(stderr, "Doom format (");

    if (!((b = lumpnum+ML_NODES) < numlumps &&
        (chk_nodes = W_CacheLumpNum(b, PU_CACHE)) &&
        W_LumpLength(b) > 0))
	fprintf(stderr, "no nodes");
    else
    if (!memcmp(chk_nodes, "xNd4\0\0\0\0", 8))
    {
	fprintf(stderr, "DeePBSP");
	format |= MFMT_DEEPBSP;
    }
    else
    if (!memcmp(chk_nodes, "XNOD", 4))
    {
	fprintf(stderr, "ZDBSP");
	format |= MFMT_ZDBSPX;
    }
    else
    if (!memcmp(chk_nodes, "ZNOD", 4))
    {
	fprintf(stderr, "compressed ZDBSP");
	format |= MFMT_ZDBSPZ;
    }
    else
	fprintf(stderr, "BSP");

    fprintf(stderr, "), ");

    if (chk_nodes)
    {
        W_ReleaseLumpNum(b);
    }

    return format;
}

// [crispy] support maps with DeePBSP nodes
// adapted from prboom-plus/src/p_setup.c:633-752
static void P_LoadSegs_DeePBSP(int lump)
{
    int i;
    mapseg_deepbsp_t *data;

    numsegs = W_LumpLength(lump) / sizeof(mapseg_deepbsp_t);
    segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, 0);
    data = (mapseg_deepbsp_t *)W_CacheLumpNum(lump, PU_STATIC);

    for (i = 0; i < numsegs; i++)
    {
        seg_t *li = segs + i;
        mapseg_deepbsp_t *ml = data + i;
        int side, line_def;
        line_t *ldef;
        int vn1, vn2;

        // [MB] 2020-04-30: Fix endianess for DeePBSDP V4 nodes
        vn1 = LONG(ml->v1);
        vn2 = LONG(ml->v2);

        li->v1 = &vertexes[vn1];
        li->v2 = &vertexes[vn2];

        li->angle = (SHORT(ml->angle))<<FRACBITS;

    //  li->offset = (SHORT(ml->offset))<<FRACBITS; // [crispy] recalculated below
        line_def = (unsigned short)SHORT(ml->linedef);
        ldef = &lines[line_def];
        li->linedef = ldef;
        side = SHORT(ml->side);

        // e6y: check for wrong indexes
        if ((unsigned)line_def >= (unsigned)numlines)
        {
            I_Error("P_LoadSegs: seg %d references a non-existent linedef %d",
                i, (unsigned)line_def);
        }
        if ((unsigned)ldef->sidenum[side] >= (unsigned)numsides)
        {
            I_Error("P_LoadSegs: linedef %d for seg %d references a non-existent sidedef %d",
                line_def, i, (unsigned)ldef->sidenum[side]);
        }

        li->sidedef = &sides[ldef->sidenum[side]];
        li->frontsector = sides[ldef->sidenum[side]].sector;
        // [crispy] recalculate
        li->offset = GetOffset(li->v1, (ml->side ? ldef->v2 : ldef->v1));

        if (ldef->flags & ML_TWOSIDED)
            li->backsector = sides[ldef->sidenum[side ^ 1]].sector;
        else
            li->backsector = 0;
    }

    W_ReleaseLumpNum(lump);
}

// [crispy] support maps with DeePBSP nodes
// adapted from prboom-plus/src/p_setup.c:843-863
static void P_LoadSubsectors_DeePBSP(int lump)
{
    mapsubsector_deepbsp_t *data;
    int i;

    numsubsectors = W_LumpLength(lump) / sizeof(mapsubsector_deepbsp_t);
    subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_LEVEL, 0);
    data = (mapsubsector_deepbsp_t *)W_CacheLumpNum(lump, PU_STATIC);

    // [crispy] fail on missing subsectors
    if (!data || !numsubsectors)
        I_Error("P_LoadSubsectors: No subsectors in map!");

    for (i = 0; i < numsubsectors; i++)
    {
        // [MB] 2020-04-30: Fix endianess for DeePBSDP V4 nodes
        subsectors[i].numlines = (unsigned short)SHORT(data[i].numsegs);
        subsectors[i].firstline = LONG(data[i].firstseg);
    }

    W_ReleaseLumpNum(lump);
}
// [crispy] support maps with DeePBSP nodes
// adapted from prboom-plus/src/p_setup.c:995-1038
static void P_LoadNodes_DeePBSP(int lump)
{
    const byte *data;
    int i;

    numnodes = (W_LumpLength (lump) - 8) / sizeof(mapnode_deepbsp_t);
    nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, 0);
    data = W_CacheLumpNum (lump, PU_STATIC);

    // [crispy] warn about missing nodes
    if (!data || !numnodes)
    {
        if (numsubsectors == 1)
            fprintf(stderr, "P_LoadNodes: No nodes in map, but only one subsector.\n");
        else
            I_Error("P_LoadNodes: No nodes in map!");
    }

    // skip header
    data += 8;

    for (i = 0; i < numnodes; i++)
    {
        node_t *no = nodes + i;
        const mapnode_deepbsp_t *mn = (const mapnode_deepbsp_t *) data + i;
        int j;

        no->x = SHORT(mn->x)<<FRACBITS;
        no->y = SHORT(mn->y)<<FRACBITS;
        no->dx = SHORT(mn->dx)<<FRACBITS;
        no->dy = SHORT(mn->dy)<<FRACBITS;

        for (j = 0; j < 2; j++)
        {
            int k;
            // [MB] 2020-04-30: Fix endianess for DeePBSDP V4 nodes
            no->children[j] = LONG(mn->children[j]);

            for (k = 0; k < 4; k++)
                no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
        }
    }

  W_ReleaseLumpNum(lump);
}

// [crispy] support maps with compressed or uncompressed ZDBSP nodes
// adapted from prboom-plus/src/p_setup.c:1040-1331
// heavily modified, condensed and simplyfied
// - removed most paranoid checks, brought in line with Vanilla P_LoadNodes()
// - removed const type punning pointers
// - inlined P_LoadZSegs()
// - added support for compressed ZDBSP nodes
// - added support for flipped levels
// [MB] 2020-04-30: Fix endianess for ZDoom extended nodes
static void P_LoadNodes_ZDBSP (int lump, boolean compressed)
{
    byte *data;
    unsigned int i;
    byte *output;

    unsigned int orgVerts, newVerts;
    unsigned int numSubs, currSeg;
    unsigned int numSegs;
    unsigned int numNodes;
    vertex_t *newvertarray = NULL;

    data = W_CacheLumpNum(lump, PU_LEVEL);

    // 0. Uncompress nodes lump (or simply skip header)

    if (compressed)
    {
        const int len =  W_LumpLength(lump);
        int outlen, err;
        z_stream *zstream;

        // first estimate for compression rate:
        // output buffer size == 2.5 * input size
        outlen = 2.5 * len;
        output = Z_Malloc(outlen, PU_STATIC, 0);

        // initialize stream state for decompression
        zstream = malloc(sizeof(*zstream));
        memset(zstream, 0, sizeof(*zstream));
        zstream->next_in = data + 4;
        zstream->avail_in = len - 4;
        zstream->next_out = output;
        zstream->avail_out = outlen;

        if (inflateInit(zstream) != Z_OK)
            I_Error("P_LoadNodes: Error during ZDBSP nodes decompression initialization!");

        // resize if output buffer runs full
        while ((err = inflate(zstream, Z_SYNC_FLUSH)) == Z_OK)
        {
            int outlen_old = outlen;
            outlen = 2 * outlen_old;
            output = I_Realloc(output, outlen);
            zstream->next_out = output + outlen_old;
            zstream->avail_out = outlen - outlen_old;
        }

        if (err != Z_STREAM_END)
            I_Error("P_LoadNodes: Error during ZDBSP nodes decompression!");

        fprintf(stderr, "P_LoadNodes: ZDBSP nodes compression ratio %.3f\n",
                (float)zstream->total_out/zstream->total_in);

        data = output;

        if (inflateEnd(zstream) != Z_OK)
            I_Error("P_LoadNodes: Error during ZDBSP nodes decompression shut-down!");

        // release the original data lump
        W_ReleaseLumpNum(lump);
        free(zstream);
    }
    else
    {
        // skip header
        data += 4;
	// [JN] Shut up compiler warning.
	output = 0;
    }

    // 1. Load new vertices added during node building

    orgVerts = LONG(*((unsigned int*)data));
    data += sizeof(orgVerts);

    newVerts = LONG(*((unsigned int*)data));
    data += sizeof(newVerts);

    if (orgVerts + newVerts == (unsigned int)numvertexes)
    {
        newvertarray = vertexes;
    }
    else
    {
        newvertarray = Z_Malloc((orgVerts + newVerts) * sizeof(vertex_t), PU_LEVEL, 0);
        memcpy(newvertarray, vertexes, orgVerts * sizeof(vertex_t));
        memset(newvertarray + orgVerts, 0, newVerts * sizeof(vertex_t));
    }

    for (i = 0; i < newVerts; i++)
    {
        newvertarray[i + orgVerts].r_x =
        newvertarray[i + orgVerts].x = LONG(*((unsigned int*)data));
        data += sizeof(newvertarray[0].x);

        newvertarray[i + orgVerts].r_y =
        newvertarray[i + orgVerts].y = LONG(*((unsigned int*)data));
        data += sizeof(newvertarray[0].y);
    }

    if (vertexes != newvertarray)
    {
        for (i = 0; i < (unsigned int)numlines; i++)
        {
            lines[i].v1 = lines[i].v1 - vertexes + newvertarray;
            lines[i].v2 = lines[i].v2 - vertexes + newvertarray;
        }

        Z_Free(vertexes);
        vertexes = newvertarray;
        numvertexes = orgVerts + newVerts;
    }

    // 2. Load subsectors

    numSubs = LONG(*((unsigned int*)data));
    data += sizeof(numSubs);

    if (numSubs < 1)
        I_Error("P_LoadNodes: No subsectors in map!");

    numsubsectors = numSubs;
    subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_LEVEL, 0);

    for (i = currSeg = 0; i < numsubsectors; i++)
    {
        mapsubsector_zdbsp_t *mseg = (mapsubsector_zdbsp_t*) data + i;

        subsectors[i].firstline = currSeg;
        subsectors[i].numlines = LONG(mseg->numsegs);
        subsectors[i].poly = NULL;
        currSeg += LONG(mseg->numsegs);
    }

    data += numsubsectors * sizeof(mapsubsector_zdbsp_t);

    // 3. Load segs

    numSegs = LONG(*((unsigned int*)data));
    data += sizeof(numSegs);

    // The number of stored segs should match the number of segs used by subsectors
    if (numSegs != currSeg)
    {
        I_Error("P_LoadNodes: Incorrect number of segs in ZDBSP nodes!");
    }

    numsegs = numSegs;
    segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, 0);

    for (i = 0; i < numsegs; i++)
    {
        line_t *ldef;
        unsigned int line_def;
        unsigned char side;
        seg_t *li = segs + i;
        mapseg_zdbsp_t *ml = (mapseg_zdbsp_t *) data + i;
        unsigned int v1, v2;

        v1 = LONG(ml->v1);
        v2 = LONG(ml->v2);
        li->v1 = &vertexes[v1];
        li->v2 = &vertexes[v2];

        line_def = (unsigned short)SHORT(ml->linedef);
        ldef = &lines[line_def];
        li->linedef = ldef;
        side = ml->side;

        // e6y: check for wrong indexes
        if ((unsigned)line_def >= (unsigned)numlines)
        {
            I_Error("P_LoadSegs: seg %d references a non-existent linedef %d",
                i, (unsigned)line_def);
        }
        if ((unsigned)ldef->sidenum[side] >= (unsigned)numsides)
        {
            I_Error("P_LoadSegs: linedef %d for seg %d references a non-existent sidedef %d",
                line_def, i, (unsigned)ldef->sidenum[side]);
        }

        li->sidedef = &sides[ldef->sidenum[side]];
        li->frontsector = sides[ldef->sidenum[side]].sector;

        // seg angle and offset are not included
        li->angle = R_PointToAngle2(segs[i].v1->x, segs[i].v1->y, segs[i].v2->x, segs[i].v2->y);
        li->offset = GetOffset(li->v1, (ml->side ? ldef->v2 : ldef->v1));

        if (ldef->flags & ML_TWOSIDED)
            li->backsector = sides[ldef->sidenum[side ^ 1]].sector;
        else
            li->backsector = 0;
    }

    data += numsegs * sizeof(mapseg_zdbsp_t);

    // 4. Load nodes

    numNodes = LONG(*((unsigned int*)data));
    data += sizeof(numNodes);

    numnodes = numNodes;
    nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, 0);

    for (i = 0; i < numnodes; i++)
    {
        int j, k;
        node_t *no = nodes + i;
        mapnode_zdbsp_t *mn = (mapnode_zdbsp_t *) data + i;

        no->x = SHORT(mn->x)<<FRACBITS;
        no->y = SHORT(mn->y)<<FRACBITS;
        no->dx = SHORT(mn->dx)<<FRACBITS;
        no->dy = SHORT(mn->dy)<<FRACBITS;

        for (j = 0; j < 2; j++)
        {
            no->children[j] = LONG(mn->children[j]);

            for (k = 0; k < 4; k++)
                no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
        }
    }

    if (compressed)
    {
        Z_Free(output);
    }
    else
    {
        W_ReleaseLumpNum(lump);
    }
}

/*
=================
=
= P_LoadVertexes
=
=================
*/

static void P_LoadVertexes(int lump)
{
    byte *data;
    int i;
    mapvertex_t *ml;
    vertex_t *li;

    numvertexes = W_LumpLength(lump) / sizeof(mapvertex_t);
    vertexes = Z_Malloc(numvertexes * sizeof(vertex_t), PU_LEVEL, 0);
    data = W_CacheLumpNum(lump, PU_STATIC);

    ml = (mapvertex_t *) data;
    li = vertexes;
    for (i = 0; i < numvertexes; i++, li++, ml++)
    {
        li->x = SHORT(ml->x) << FRACBITS;
        li->y = SHORT(ml->y) << FRACBITS;

        // [crispy] initialize vertex coordinates *only* used in rendering
        li->r_x = li->x;
        li->r_y = li->y;
        li->moved = false;
    }

    W_ReleaseLumpNum(lump);
}


/*
=================
=
= P_LoadSegs
=
=================
*/

static void P_LoadSegs(int lump)
{
    byte *data;
    int i;
    mapseg_t *ml;
    seg_t *li;
    line_t *ldef;
    int line_def, side;

    numsegs = W_LumpLength(lump) / sizeof(mapseg_t);
    segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, 0);
    memset(segs, 0, numsegs * sizeof(seg_t));
    data = W_CacheLumpNum(lump, PU_STATIC);

    ml = (mapseg_t *) data;
    li = segs;
    for (i = 0; i < numsegs; i++, li++, ml++)
    {
        li->v1 = &vertexes[(unsigned short)SHORT(ml->v1)]; // [crispy] extended nodes
        li->v2 = &vertexes[(unsigned short)SHORT(ml->v2)]; // [crispy] extended nodes

        li->angle = (SHORT(ml->angle)) << 16;
        li->offset = (SHORT(ml->offset)) << 16;
        line_def = (unsigned short)SHORT(ml->linedef); // [crispy] extended nodes
        ldef = &lines[line_def];
        li->linedef = ldef;
        side = SHORT(ml->side);
        li->sidedef = &sides[ldef->sidenum[side]];
        li->frontsector = sides[ldef->sidenum[side]].sector;
        if (ldef->flags & ML_TWOSIDED)
            li->backsector = sides[ldef->sidenum[side ^ 1]].sector;
        else
            li->backsector = 0;
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

void P_SegLengths (void)
{
    int i;

    for (i = 0; i < numsegs; i++)
    {
        seg_t *const li = &segs[i];
        int64_t dx, dy;

        dx = li->v2->x - li->v1->x;
        dy = li->v2->y - li->v1->y;

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
}

/*
=================
=
= P_LoadSubsectors
=
=================
*/

static void P_LoadSubsectors(int lump)
{
    byte *data;
    int i;
    mapsubsector_t *ms;
    subsector_t *ss;

    numsubsectors = W_LumpLength(lump) / sizeof(mapsubsector_t);
    subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_LEVEL, 0);
    data = W_CacheLumpNum(lump, PU_STATIC);

    ms = (mapsubsector_t *) data;
    memset(subsectors, 0, numsubsectors * sizeof(subsector_t));
    ss = subsectors;
    for (i = 0; i < numsubsectors; i++, ss++, ms++)
    {
        ss->numlines = (unsigned short)SHORT(ms->numsegs); // [crispy] extended nodes
        ss->firstline = (unsigned short)SHORT(ms->firstseg); // [crispy] extended nodes
    }

    W_ReleaseLumpNum(lump);
}


/*
=================
=
= P_LoadSectors
=
=================
*/

static void P_LoadSectors(int lump)
{
    byte *data;
    int i;
    mapsector_t *ms;
    sector_t *ss;

    numsectors = W_LumpLength(lump) / sizeof(mapsector_t);
    sectors = Z_Malloc(numsectors * sizeof(sector_t), PU_LEVEL, 0);
    memset(sectors, 0, numsectors * sizeof(sector_t));
    data = W_CacheLumpNum(lump, PU_STATIC);

    ms = (mapsector_t *) data;
    ss = sectors;

    for (i = 0; i < numsectors; i++, ss++, ms++)
    {
        ss->floorheight = SHORT(ms->floorheight) << FRACBITS;
        ss->ceilingheight = SHORT(ms->ceilingheight) << FRACBITS;
        ss->floorpic = R_FlatNumForName(ms->floorpic);
        ss->ceilingpic = R_FlatNumForName(ms->ceilingpic);
        ss->lightlevel = SHORT(ms->lightlevel);
        // [JN] Initialize temp sector brightness variable.
        ss->lightlevel_unlit = SHORT(ms->lightlevel);
        ss->special = SHORT(ms->special);
        ss->tag = SHORT(ms->tag);
        ss->thinglist = NULL;
        ss->seqType = SEQTYPE_STONE;    // default seqType

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


/*
=================
=
= P_LoadNodes
=
=================
*/

static void P_LoadNodes(int lump)
{
    byte *data;
    int i, j, k;
    mapnode_t *mn;
    node_t *no;

    numnodes = W_LumpLength(lump) / sizeof(mapnode_t);
    nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, 0);
    data = W_CacheLumpNum(lump, PU_STATIC);

    mn = (mapnode_t *) data;
    no = nodes;
    for (i = 0; i < numnodes; i++, no++, mn++)
    {
        no->x = SHORT(mn->x) << FRACBITS;
        no->y = SHORT(mn->y) << FRACBITS;
        no->dx = SHORT(mn->dx) << FRACBITS;
        no->dy = SHORT(mn->dy) << FRACBITS;
        for (j = 0; j < 2; j++)
        {
            no->children[j] = (unsigned short)SHORT(mn->children[j]); // [crispy] extended nodes
            
            // [crispy] support for extended nodes
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
            for (k = 0; k < 4; k++)
                no->bbox[j][k] = SHORT(mn->bbox[j][k]) << FRACBITS;
        }
    }
    W_ReleaseLumpNum(lump);
}

//==========================================================================
//
// P_LoadThings
//
//==========================================================================

static void P_LoadThings(int lump)
{
    byte *data;
    int i;
    mapthing_t spawnthing;
    mapthing_t *mt;
    int numthings;
    int playerCount;
    int deathSpotsCount;

    data = W_CacheLumpNum(lump, PU_STATIC);
    numthings = W_LumpLength(lump) / sizeof(mapthing_t);

    mt = (mapthing_t *) data;
    for (i = 0; i < numthings; i++, mt++)
    {
        spawnthing.tid = SHORT(mt->tid);
        spawnthing.x = SHORT(mt->x);
        spawnthing.y = SHORT(mt->y);
        spawnthing.height = SHORT(mt->height);
        spawnthing.angle = SHORT(mt->angle);
        spawnthing.type = SHORT(mt->type);
        spawnthing.options = SHORT(mt->options);

        spawnthing.special = mt->special;
        spawnthing.arg1 = mt->arg1;
        spawnthing.arg2 = mt->arg2;
        spawnthing.arg3 = mt->arg3;
        spawnthing.arg4 = mt->arg4;
        spawnthing.arg5 = mt->arg5;

        P_SpawnMapThing(&spawnthing);
    }
    P_CreateTIDList();
    P_InitCreatureCorpseQueue(false);   // false = do NOT scan for corpses
    W_ReleaseLumpNum(lump);

    if (!deathmatch)
    {                           // Don't need to check deathmatch spots
        for (i = 0; i < 4 /*MAXPLAYERS*/; i++)
        {
            // [JN] Inform about missing player 1-4 starts.
            if (playeringame[i] && !playerstartsingame[i])
            {
                I_Error("P_LoadThings: Player %d start missing (vanilla crashes here)", i + 1);
            }
        }
        return;
    }
    playerCount = 0;
    for (i = 0; i < maxplayers; i++)
    {
        playerCount += playeringame[i];
    }
    deathSpotsCount = deathmatch_p - deathmatchstarts;
    if (deathSpotsCount < playerCount)
    {
        I_Error("P_LoadThings: Player count (%d) exceeds deathmatch "
                "spots (%d)", playerCount, deathSpotsCount);
    }
}

/*
=================
=
= P_LoadLineDefs
=
=================
*/

static void P_LoadLineDefs(int lump)
{
    byte *data;
    int i;
    maplinedef_t *mld;
    line_t *ld;
    const vertex_t *v1;
    const vertex_t *v2;

    numlines = W_LumpLength(lump) / sizeof(maplinedef_t);
    lines = Z_Malloc(numlines * sizeof(line_t), PU_LEVEL, 0);
    memset(lines, 0, numlines * sizeof(line_t));
    data = W_CacheLumpNum(lump, PU_STATIC);

    mld = (maplinedef_t *) data;
    ld = lines;
    for (i = 0; i < numlines; i++, mld++, ld++)
    {
        ld->flags = (unsigned short)SHORT(mld->flags); // [crispy] extended nodes

        // Old line special info ...
        //ld->special = SHORT(mld->special);
        //ld->tag = SHORT(mld->tag);

        // New line special info ...
        ld->special = mld->special;
        ld->arg1 = mld->arg1;
        ld->arg2 = mld->arg2;
        ld->arg3 = mld->arg3;
        ld->arg4 = mld->arg4;
        ld->arg5 = mld->arg5;

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
            if (FixedDiv(ld->dy, ld->dx) > 0)
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
        ld->sidenum[0] = SHORT(mld->sidenum[0]);
        ld->sidenum[1] = SHORT(mld->sidenum[1]);
        if (ld->sidenum[0] != NO_INDEX) // [crispy] extended nodes
            ld->frontsector = sides[ld->sidenum[0]].sector;
        else
            ld->frontsector = 0;
        if (ld->sidenum[1] != NO_INDEX) // [crispy] extended nodes
            ld->backsector = sides[ld->sidenum[1]].sector;
        else
            ld->backsector = 0;
    }

    W_ReleaseLumpNum(lump);
}


/*
=================
=
= P_LoadSideDefs
=
=================
*/

static void P_LoadSideDefs(int lump)
{
    byte *data;
    int i;
    mapsidedef_t *msd;
    side_t *sd;

    numsides = W_LumpLength(lump) / sizeof(mapsidedef_t);
    sides = Z_Malloc(numsides * sizeof(side_t), PU_LEVEL, 0);
    memset(sides, 0, numsides * sizeof(side_t));
    data = W_CacheLumpNum(lump, PU_STATIC);

    msd = (mapsidedef_t *) data;
    sd = sides;

    for (i = 0; i < numsides; i++, msd++, sd++)
    {
        sd->textureoffset = SHORT(msd->textureoffset) << FRACBITS;
        sd->rowoffset = SHORT(msd->rowoffset) << FRACBITS;
        sd->toptexture = R_TextureNumForName(msd->toptexture);
        sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
        sd->midtexture = R_TextureNumForName(msd->midtexture);
        sd->sector = &sectors[SHORT(msd->sector)];
    }
    W_ReleaseLumpNum(lump);
}

/*
=================
=
= P_LoadBlockMap
=
=================
*/

static boolean P_LoadBlockMap(int lump)
{
    int i, count;
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
    wadblockmaplump = Z_Malloc(lumplen, PU_LEVEL, NULL);
    W_ReadLump(lump, wadblockmaplump);
    blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, NULL);
    blockmap = blockmaplump + 4;

    blockmaplump[0] = SHORT(wadblockmaplump[0]);
    blockmaplump[1] = SHORT(wadblockmaplump[1]);
    blockmaplump[2] = (int32_t)(SHORT(wadblockmaplump[2])) & 0xffff;
    blockmaplump[3] = (int32_t)(SHORT(wadblockmaplump[3])) & 0xffff;

    // Swap all short integers to native byte ordering:

    // count = lumplen / 2; // [crispy] moved up
    for (i = 4; i < count; i++)
    {
        short t = SHORT(wadblockmaplump[i]);
        blockmaplump[i] = (t == -1) ? -1l : (int32_t) t & 0xffff;
    }

    Z_Free(wadblockmaplump);

    bmaporgx = blockmaplump[0] << FRACBITS;
    bmaporgy = blockmaplump[1] << FRACBITS;
    bmapwidth = blockmaplump[2];
    bmapheight = blockmaplump[3];

    // clear out mobj chains

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


/*
=================
=
= P_GroupLines
=
= Builds sector line lists and subsector sector numbers
= Finds block bounding boxes for sectors
= [crispy] updated old Doom 1.2 code with actual implementation of P_GroupLines
= from Chocolate Doom for better handling and faster loading of complex levels
=================
*/

static void P_GroupLines(void)
{
    line_t **linebuffer;
    int i, j, total;
    line_t *li;
    sector_t *sector;
    subsector_t *ss;
    seg_t *seg;
    fixed_t bbox[4];
    int block;

// look up sector number for each subsector
    ss = subsectors;
    for (i = 0; i < numsubsectors; i++, ss++)
    {
        seg = &segs[ss->firstline];
        ss->sector = seg->sidedef->sector;
    }

// count number of lines in each sector
    li = lines;
    total = 0;
    for (i = 0; i < numlines; i++, li++)
    {
        if (li->frontsector)  // [JN] H+H: Fix possible line miscounting.
        {
            li->frontsector->linecount++;
            total++;
        }
        if (li->backsector && li->backsector != li->frontsector)
        {
            li->backsector->linecount++;
            total++;
        }
    }

// build line tables for each sector
    linebuffer = Z_Malloc(total * sizeof(line_t *), PU_LEVEL, 0);
    for (i = 0; i < numsectors; ++i)
    {
        // Assign the line buffer for this sector
        sectors[i].lines = linebuffer;
        linebuffer += sectors[i].linecount;

        // Reset linecount to zero so in the next stage we can count
        // lines into the list.
        sectors[i].linecount = 0;
    }

// [crispy] assign lines to sectors
    for (i = 0; i < numlines; ++i)
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

// [crispy] generate bounding boxes for sectors
    sector = sectors;
    for (i = 0; i < numsectors; i++, sector++)
    {
        M_ClearBox(bbox);
        for (j = 0; j < sector->linecount; j++)
        {
                li = sector->lines[j];
        
                M_AddToBox(bbox, li->v1->x, li->v1->y);
                M_AddToBox(bbox, li->v2->x, li->v2->y);
        }

        // set the degenmobj_t to the middle of the bounding box
        // Andrey Budko: fix sound origin for large levels
        sector->soundorg.x = bbox[BOXRIGHT] / 2 + bbox[BOXLEFT] / 2;
        sector->soundorg.y = bbox[BOXTOP] / 2 + bbox[BOXBOTTOM] / 2;

        // adjust bounding box to map blocks
        block = (bbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block >= bmapheight ? bmapheight - 1 : block;
        sector->blockbox[BOXTOP] = block;

        block = (bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sector->blockbox[BOXBOTTOM] = block;

        block = (bbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block >= bmapwidth ? bmapwidth - 1 : block;
        sector->blockbox[BOXRIGHT] = block;

        block = (bbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sector->blockbox[BOXLEFT] = block;
    }

}

//=============================================================================

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

lumpinfo_t *maplumpinfo;

// -----------------------------------------------------------------------------
// P_SaveSectorBrightness / P_RestoreSectorBrightness
// [JN] Saves/restores correct sector brightness, changed by lightning effect.
// -----------------------------------------------------------------------------

static void P_SaveSectorBrightness (void)
{
    sector_t *brightSec = sectors;

    for (int i = 0; i < numsectors; i++, brightSec++)
    {
        if (brightSec->ceilingpic == skyflatnum
        ||  brightSec->special == 198     // LIGHTNING_SPECIAL
        ||  brightSec->special == 199)    // LIGHTNING_SPECIAL2
        {
            brightSec->lightlevel_unlit = brightSec->lightlevel;
        }
    }
}

void P_RestoreSectorBrightness (void)
{
    sector_t *brightSec = sectors;

    for (int i = 0; i < numsectors; i++, brightSec++)
    {
        if (brightSec->ceilingpic == skyflatnum
        ||  brightSec->special == 198     // LIGHTNING_SPECIAL
        ||  brightSec->special == 199)    // LIGHTNING_SPECIAL2
        {
            brightSec->lightlevel = brightSec->lightlevel_unlit;
        }
    }
}

/*
=================
=
= P_SetupLevel
=
=================
*/

void P_SetupLevel(int episode, int map, int playermask, skill_t skill)
{
    int i;
    int parm;
    char lumpname[9];
    int lumpnum;
    mobj_t *mobj;
    boolean	crispy_validblockmap;
    mapformat_t crispy_mapformat;
    // [JN] CRL - indicate level loading time in console.
    const int starttime = I_GetTimeMS();

    for (i = 0; i < maxplayers; i++)
    {
        players[i].killcount = players[i].secretcount
            = players[i].itemcount = 0;
    }
    players[consoleplayer].viewz = 1;   // will be set by player think

    // [crispy] stop demo warp mode now
    if (demowarp == map)
    {
        demowarp = 0;
        nodrawers = false;
        singletics = false;
    }

    // Waiting-for-level-load song; not played if playing music from CD
    // (the seek time will be so long it will just make loading take
    // longer)
    // [JN] TODO - is it really needed? Loading is fast enough nowadays.
    /*
    if (!cdmusic)
    {
        S_StartSongName("chess", true);
    }
    */

    Z_FreeTags(PU_LEVEL, PU_PURGELEVEL - 1);

    P_InitThinkers();
    leveltime = 0;
    realleveltime = 0;
    oldleveltime = 0;  // [crispy] Track if game is running

    M_snprintf(lumpname, sizeof(lumpname), "MAP%02d", map);
    lumpnum = W_GetNumForName(lumpname);

    // [crispy] check and log map and nodes format
    crispy_mapformat = P_CheckMapFormat(lumpnum);

    maplumpinfo = lumpinfo[lumpnum];

    // [JN] Indicate the map we are loading.
    printf("P_SetupLevel: MAP%02d (\"%s\"), ", map, P_GetMapName(map));

    //
    // Begin processing map lumps
    // Note: most of this ordering is important
    //
    crispy_validblockmap = P_LoadBlockMap (lumpnum+ML_BLOCKMAP); // [crispy] (re-)create BLOCKMAP if necessary
    P_LoadVertexes(lumpnum + ML_VERTEXES);
    P_LoadSectors(lumpnum + ML_SECTORS);
    P_LoadSideDefs(lumpnum + ML_SIDEDEFS);
    P_LoadLineDefs(lumpnum + ML_LINEDEFS);

    // [crispy] (re-)create BLOCKMAP if necessary
    if (!crispy_validblockmap)
    {
        P_CreateBlockMap();
    }

    if (crispy_mapformat & (MFMT_ZDBSPX | MFMT_ZDBSPZ))
    {
        P_LoadNodes_ZDBSP(lumpnum + ML_NODES, crispy_mapformat & MFMT_ZDBSPZ);
    }
    else if (crispy_mapformat & MFMT_DEEPBSP)
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

    rejectmatrix = W_CacheLumpNum(lumpnum + ML_REJECT, PU_LEVEL);
    P_GroupLines();

    // [crispy] remove slime trails
    P_RemoveSlimeTrails();

    // [crispy] fix long wall wobble
    P_SegLengths();

    // [JN] Remember initial sector brightness, 
    // which will be changed by lightning effect.
    P_SaveSectorBrightness(); 

    bodyqueslot = 0;
    po_NumPolyobjs = 0;
    deathmatch_p = deathmatchstarts;
    P_LoadThings(lumpnum + ML_THINGS);
    PO_Init(lumpnum + ML_THINGS);       // Initialize the polyobjs
    P_LoadACScripts(lumpnum + ML_BEHAVIOR);     // ACS object code
    //
    // End of map lump processing
    //

    // If deathmatch, randomly spawn the active players
    TimerGame = 0;
    if (deathmatch)
    {
        for (i = 0; i < maxplayers; i++)
        {
            if (playeringame[i])
            {                   // must give a player spot before deathmatchspawn
                mobj = P_SpawnMobj(playerstarts[0][i].x << 16,
                                   playerstarts[0][i].y << 16, 0,
                                   MT_PLAYER_FIGHTER);
                players[i].mo = mobj;
                G_DeathMatchSpawnPlayer(i);
                P_RemoveMobj(mobj);
            }
        }

        //!
        // @arg <n>
        // @category net
        // @vanilla
        //
        // For multiplayer games: exit each level after n minutes.
        //

        parm = M_CheckParmWithArgs("-timer", 1);
        if (parm)
        {
            TimerGame = atoi(myargv[parm + 1]) * 35 * 60;
        }
    }

// set up world state
    P_SpawnSpecials();

// build subsector connect matrix
//      P_ConnectSubsectors ();

// Load colormap and set the fullbright flag
    i = P_GetMapFadeTable(gamemap);
    W_ReadLump(i, colormaps);
    if (i == W_GetNumForName("COLORMAP"))
    {
        LevelUseFullBright = true;
    }
    else
    {                           // Probably fog ... don't use fullbright sprites
        LevelUseFullBright = false;
    }

    // [crispy] If true color is compiled in but disabled as an option,
    // we still need to re-generate colormaps for proper colormaps[] array colors.
    R_InitTrueColormaps(LevelUseFullBright ? "COLORMAP" : "FOGMAP");

// preload graphics
    if (precache)
        R_PrecacheLevel();

    // [JN] Force to disable spectator mode.
    crl_spectating = 0;

    // Check if the level is a lightning level
    P_InitLightning();

    S_StopAllSound();
    SN_StopAllSequences();
    S_StartSong(gamemap, true);

    // [JN] Print amount of level loading time.
    printf("loaded in %d ms.\n", I_GetTimeMS() - starttime);

//printf ("free memory: 0x%x\n", Z_FreeMemory());

}

//==========================================================================
//
// InitMapInfo
//
//==========================================================================

static void InitMapInfo(void)
{
    int map;
    int mapMax;
    int mcmdValue;
    mapInfo_t *info;
    char songMulch[10];
    const char *default_sky_name = DEFAULT_SKY_NAME;

    mapMax = 1;

    if (gamemode == shareware)
    {
	default_sky_name = "SKY2";
    }

    // Put defaults into MapInfo[0]
    info = MapInfo;
    info->cluster = 0;
    info->warpTrans = 0;
    info->nextMap = 1;          // Always go to map 1 if not specified
    info->cdTrack = 1;
    info->sky1Texture = R_TextureNumForName(default_sky_name);
    info->sky2Texture = info->sky1Texture;
    info->sky1ScrollDelta = 0;
    info->sky2ScrollDelta = 0;
    info->doubleSky = false;
    info->lightning = false;
    info->fadetable = W_GetNumForName(DEFAULT_FADE_TABLE);
    M_StringCopy(info->name, UNKNOWN_MAP_NAME, sizeof(info->name));

//    M_StringCopy(info->songLump, DEFAULT_SONG_LUMP, sizeof(info->songLump));
    SC_Open(MAPINFO_SCRIPT_NAME);
    while (SC_GetString())
    {
        if (SC_Compare("MAP") == false)
        {
            SC_ScriptError(NULL);
        }
        SC_MustGetNumber();
        if (sc_Number < 1 || sc_Number > 99)
        {                       // 
            SC_ScriptError(NULL);
        }
        map = sc_Number;

        info = &MapInfo[map];

        // Save song lump name
        M_StringCopy(songMulch, info->songLump, sizeof(songMulch));

        // Copy defaults to current map definition
        memcpy(info, &MapInfo[0], sizeof(*info));

        // Restore song lump name
        M_StringCopy(info->songLump, songMulch, sizeof(info->songLump));

        // The warp translation defaults to the map number
        info->warpTrans = map;

        // Map name must follow the number
        SC_MustGetString();
        M_StringCopy(info->name, sc_String, sizeof(info->name));

        // Process optional tokens
        while (SC_GetString())
        {
            if (SC_Compare("MAP"))
            {                   // Start next map definition
                SC_UnGet();
                break;
            }
            mcmdValue = MapCmdIDs[SC_MustMatchString(MapCmdNames)];
            switch (mcmdValue)
            {
                case MCMD_CLUSTER:
                    SC_MustGetNumber();
                    info->cluster = sc_Number;
                    break;
                case MCMD_WARPTRANS:
                    SC_MustGetNumber();
                    info->warpTrans = sc_Number;
                    break;
                case MCMD_NEXT:
                    SC_MustGetNumber();
                    info->nextMap = sc_Number;
                    break;
                case MCMD_CDTRACK:
                    SC_MustGetNumber();
                    info->cdTrack = sc_Number;
                    break;
                case MCMD_SKY1:
                    SC_MustGetString();
                    info->sky1Texture = R_TextureNumForName(sc_String);
                    SC_MustGetNumber();
                    info->sky1ScrollDelta = sc_Number << 8;
                    break;
                case MCMD_SKY2:
                    SC_MustGetString();
                    info->sky2Texture = R_TextureNumForName(sc_String);
                    SC_MustGetNumber();
                    info->sky2ScrollDelta = sc_Number << 8;
                    break;
                case MCMD_DOUBLESKY:
                    info->doubleSky = true;
                    break;
                case MCMD_LIGHTNING:
                    info->lightning = true;
                    break;
                case MCMD_FADETABLE:
                    SC_MustGetString();
                    info->fadetable = W_GetNumForName(sc_String);
                    break;
                case MCMD_CD_STARTTRACK:
                case MCMD_CD_END1TRACK:
                case MCMD_CD_END2TRACK:
                case MCMD_CD_END3TRACK:
                case MCMD_CD_INTERTRACK:
                case MCMD_CD_TITLETRACK:
                    SC_MustGetNumber();
                    cd_NonLevelTracks[mcmdValue - MCMD_CD_STARTTRACK] =
                        sc_Number;
                    break;
            }
        }
        mapMax = map > mapMax ? map : mapMax;
    }
    SC_Close();
    MapCount = mapMax;
}

//==========================================================================
//
// P_GetMapCluster
//
//==========================================================================

int P_GetMapCluster(int map)
{
    return MapInfo[QualifyMap(map)].cluster;
}

//==========================================================================
//
// P_GetMapCDTrack
//
//==========================================================================

int P_GetMapCDTrack(int map)
{
    return MapInfo[QualifyMap(map)].cdTrack;
}

//==========================================================================
//
// P_GetMapWarpTrans
//
//==========================================================================

int P_GetMapWarpTrans(int map)
{
    return MapInfo[QualifyMap(map)].warpTrans;
}

//==========================================================================
//
// P_GetMapNextMap
//
//==========================================================================

int P_GetMapNextMap(int map)
{
    return MapInfo[QualifyMap(map)].nextMap;
}

//==========================================================================
//
// P_TranslateMap
//
// Returns the actual map number given a warp map number.
//
//==========================================================================

int P_TranslateMap(int map)
{
    int i;

    for (i = 1; i < 99; i++)    // Make this a macro
    {
        if (MapInfo[i].warpTrans == map)
        {
            return i;
        }
    }
    // Not found
    return -1;
}

//==========================================================================
//
// P_GetMapSky1Texture
//
//==========================================================================

int P_GetMapSky1Texture(int map)
{
    return MapInfo[QualifyMap(map)].sky1Texture;
}

//==========================================================================
//
// P_GetMapSky2Texture
//
//==========================================================================

int P_GetMapSky2Texture(int map)
{
    return MapInfo[QualifyMap(map)].sky2Texture;
}

//==========================================================================
//
// P_GetMapName
//
//==========================================================================

char *P_GetMapName(int map)
{
    return MapInfo[QualifyMap(map)].name;
}

//==========================================================================
//
// P_GetMapSky1ScrollDelta
//
//==========================================================================

fixed_t P_GetMapSky1ScrollDelta(int map)
{
    return MapInfo[QualifyMap(map)].sky1ScrollDelta;
}

//==========================================================================
//
// P_GetMapSky2ScrollDelta
//
//==========================================================================

fixed_t P_GetMapSky2ScrollDelta(int map)
{
    return MapInfo[QualifyMap(map)].sky2ScrollDelta;
}

//==========================================================================
//
// P_GetMapDoubleSky
//
//==========================================================================

boolean P_GetMapDoubleSky(int map)
{
    return MapInfo[QualifyMap(map)].doubleSky;
}

//==========================================================================
//
// P_GetMapLightning
//
//==========================================================================

boolean P_GetMapLightning(int map)
{
    return MapInfo[QualifyMap(map)].lightning;
}

//==========================================================================
//
// P_GetMapFadeTable
//
//==========================================================================

static boolean P_GetMapFadeTable(int map)
{
    return MapInfo[QualifyMap(map)].fadetable;
}

//==========================================================================
//
// P_GetMapSongLump
//
//==========================================================================

char *P_GetMapSongLump(int map)
{
    if (!strcasecmp(MapInfo[QualifyMap(map)].songLump, DEFAULT_SONG_LUMP))
    {
        return NULL;
    }
    else
    {
        return MapInfo[QualifyMap(map)].songLump;
    }
}

//==========================================================================
//
// P_PutMapSongLump
//
//==========================================================================

void P_PutMapSongLump(int map, char *lumpName)
{
    if (map < 1 || map > MapCount)
    {
        return;
    }
    M_StringCopy(MapInfo[map].songLump, lumpName,
                 sizeof(MapInfo[map].songLump));
}

//==========================================================================
//
// P_GetCDStartTrack
//
//==========================================================================

int P_GetCDStartTrack(void)
{
    return cd_NonLevelTracks[MCMD_CD_STARTTRACK - MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDEnd1Track
//
//==========================================================================

int P_GetCDEnd1Track(void)
{
    return cd_NonLevelTracks[MCMD_CD_END1TRACK - MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDEnd2Track
//
//==========================================================================

int P_GetCDEnd2Track(void)
{
    return cd_NonLevelTracks[MCMD_CD_END2TRACK - MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDEnd3Track
//
//==========================================================================

int P_GetCDEnd3Track(void)
{
    return cd_NonLevelTracks[MCMD_CD_END3TRACK - MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDIntermissionTrack
//
//==========================================================================

int P_GetCDIntermissionTrack(void)
{
    return cd_NonLevelTracks[MCMD_CD_INTERTRACK - MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// P_GetCDTitleTrack
//
//==========================================================================

int P_GetCDTitleTrack(void)
{
    return cd_NonLevelTracks[MCMD_CD_TITLETRACK - MCMD_CD_STARTTRACK];
}

//==========================================================================
//
// QualifyMap
//
//==========================================================================

static int QualifyMap(int map)
{
    return (map < 1 || map > MapCount) ? 0 : map;
}

//==========================================================================
//
// P_Init
//
//==========================================================================

void P_Init(void)
{
    InitMapInfo();
    P_InitSwitchList();
    P_InitFTAnims();            // Init flat and texture animations
    P_InitTerrainTypes();
    P_InitLava();
    R_InitSprites(sprnames);
}


// Special early initializer needed to start sound before R_Init()
void InitMapMusicInfo(void)
{
    int i;

    for (i = 0; i < 99; i++)
    {
        M_StringCopy(MapInfo[i].songLump, DEFAULT_SONG_LUMP,
                     sizeof(MapInfo[i].songLump));
    }
    MapCount = 98;
}

/*
void My_Debug(void)
{
	int i;

	printf("My debug stuff ----------------------\n");
	printf("gamemap=%d\n",gamemap);
	for (i=0; i<10; i++)
	{
		printf("i=%d  songlump=%s\n",i,MapInfo[i].songLump);
	}
}
*/
