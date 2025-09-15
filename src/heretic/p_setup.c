//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2015-2018 Fabian Greffrath
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

// P_main.c

#include <math.h>
#include <stdlib.h>

#include "doomdef.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_timer.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "p_local.h"
#include "s_sound.h"

// [crispy] support maps with compressed ZDBSP nodes
// [JN] Support via MINIZ library, thanks Leonid Murin (Dasperal).
#include "miniz.h"

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

typedef enum
{
    MFMT_DOOMBSP = 0x000,
    MFMT_DEEPBSP = 0x001,
    MFMT_ZDBSPX  = 0x002,
    MFMT_ZDBSPZ  = 0x004,
    MFMT_HEXEN   = 0x100,
} mapformat_t;

// -----------------------------------------------------------------------------
// GetSectorAtNullAddress
// -----------------------------------------------------------------------------

static sector_t *GetSectorAtNullAddress (void)
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

static fixed_t GetOffset (const vertex_t *restrict v1, const vertex_t *restrict v2)
{
    // Compute delta in map units
    const int32_t dx = (v1->x - v2->x) >> FRACBITS;
    const int32_t dy = (v1->y - v2->y) >> FRACBITS;

    // Euclidean distance in map units
    const double dist = sqrt((double)dx * dx + (double)dy * dy);

    // Convert back to fixed-point
    return (fixed_t)(dist * FRACUNIT);
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
	fprintf(stderr, "Heretic format (");

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

// [crispy] allow loading of Hexen-format maps
// adapted from chocolate-doom/src/hexen/p_setup.c:348-400
static void P_LoadThings_Hexen (int lump)
{
    byte *data;
    int i;
    mapthing_t spawnthing;
    mapthing_hexen_t *mt;
    int numthings;

    data = W_CacheLumpNum(lump, PU_STATIC);
    numthings = W_LumpLength(lump) / sizeof(mapthing_hexen_t);

    mt = (mapthing_hexen_t *) data;
    for (i = 0; i < numthings; i++, mt++)
    {
//	spawnthing.tid = SHORT(mt->tid);
	spawnthing.x = SHORT(mt->x);
	spawnthing.y = SHORT(mt->y);
//	spawnthing.height = SHORT(mt->height);
	spawnthing.angle = SHORT(mt->angle);
	spawnthing.type = SHORT(mt->type);
	spawnthing.options = SHORT(mt->options);

//	spawnthing.special = mt->special;
//	spawnthing.arg1 = mt->arg1;
//	spawnthing.arg2 = mt->arg2;
//	spawnthing.arg3 = mt->arg3;
//	spawnthing.arg4 = mt->arg4;
//	spawnthing.arg5 = mt->arg5;

	P_SpawnMapThing(&spawnthing);
    }

    W_ReleaseLumpNum(lump);
}

// [crispy] allow loading of Hexen-format maps
// adapted from chocolate-doom/src/hexen/p_setup.c:410-490
static void P_LoadLineDefs_Hexen (int lump)
{
    byte *data;
    int i;
    maplinedef_hexen_t *mld;
    line_t *ld;
    const vertex_t *v1;
    const vertex_t *v2;
    int warn; // [crispy] warn about unknown linedef types

    numlines = W_LumpLength(lump) / sizeof(maplinedef_hexen_t);
    lines = Z_Malloc(numlines * sizeof(line_t), PU_LEVEL, 0);
    memset(lines, 0, numlines * sizeof(line_t));
    data = W_CacheLumpNum(lump, PU_STATIC);

    mld = (maplinedef_hexen_t *) data;
    ld = lines;
    warn = 0; // [crispy] warn about unknown linedef types
    for (i = 0; i < numlines; i++, mld++, ld++)
    {
	ld->flags = (unsigned short)SHORT(mld->flags);

	ld->special = mld->special;
//	ld->arg1 = mld->arg1;
//	ld->arg2 = mld->arg2;
//	ld->arg3 = mld->arg3;
//	ld->arg4 = mld->arg4;
//	ld->arg5 = mld->arg5;

	// [crispy] warn about unknown linedef types
	if ((unsigned short) ld->special > 141)
	{
	    fprintf(stderr, "P_LoadLineDefs: Unknown special %d at line %d\n", ld->special, i);
	    warn++;
	}

	v1 = ld->v1 = &vertexes[(unsigned short)SHORT(mld->v1)];
	v2 = ld->v2 = &vertexes[(unsigned short)SHORT(mld->v2)];

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

	if (ld->sidenum[0] != NO_INDEX)
	    ld->frontsector = sides[ld->sidenum[0]].sector;
	else
	    ld->frontsector = 0;
	if (ld->sidenum[1] != NO_INDEX)
	    ld->backsector = sides[ld->sidenum[1]].sector;
	else
	    ld->backsector = 0;
    }

    // [crispy] warn about unknown linedef types
    if (warn)
    {
	fprintf(stderr, "P_LoadLineDefs: Found %d line%s with unknown linedef type.\n"
	                "THIS MAP MAY NOT WORK AS EXPECTED!\n", warn, (warn > 1) ? "s" : "");
    }

    W_ReleaseLumpNum(lump);
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
                        "P_LoadSegs: Linedef %u has two-sided flag set, but no second sidedef\n",
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
        // [PN] Initialize Z-axis sound origin with the middle of the sector height
        dst[i].soundorg.z          = (dst[i].floorheight + dst[i].ceilingheight) >> 1;
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
    // [JN] Initialize counters for playstate limits.
    int boss_count = 0, ambs_count = 0, mace_count = 0;

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

        // [JN] Print console warnings when playstate limits has been reached.
        switch (SHORT(mt->type))
        {
            // D'Sparil teleport location
            case 56:
                boss_count++;
                if (boss_count == 9)
                {
                    printf ("\n  Warning: more than 8 D'Sparil teleporters found,"
                            " this map won't work in vanilla Heretic. ");
                }
            break;
            // Sound sequences
            case 1200: case 1201: case 1202: case 1203: 
            case 1204: case 1205: case 1206: case 1207:
            case 1208: case 1209:
                ambs_count++;
                if (ambs_count == 9)
                {
                    printf ("\n  Warning: more than 8 sound sequences found,"
                            " this map won't work in vanilla Heretic. ");
                }
            break;
            // Firemace
            case 2002:
                mace_count++;
                if (mace_count == 9)
                {
                    printf ("\n  Warning: more than 8 Firemace spots found,"
                            " this map won't work in vanilla Heretic. ");
                }
            break;
        }

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

    for (int i = 0; i < count; ++i)
    {
        line_t *const ld = &dst[i];
        const maplinedef_t *const ml = &src[i];

        // Flags and special
        ld->flags = (unsigned short)SHORT(ml->flags);
        ld->special = SHORT(ml->special);

        // Tag
        ld->tag = SHORT(ml->tag);

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

        // Sound origin: midpoint
        ld->soundorg.x = (ld->bbox[BOXLEFT] + ld->bbox[BOXRIGHT]) >> 1;
        ld->soundorg.y = (ld->bbox[BOXBOTTOM] + ld->bbox[BOXTOP]) >> 1;
        ld->soundorg.z = ld->frontsector ? ((ld->frontsector->floorheight
                       + ld->frontsector->ceilingheight) >> 1) : 0;
    }

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
    fixed_t minX = INT_MAX, minY = INT_MAX;
    fixed_t maxX = INT_MIN, maxY = INT_MIN;

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

static void P_GroupLines (void)
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
        fprintf(stderr,
            "PadRejectArray: REJECT lump too short to pad! (%u > %zu)\n",
            len, sizeof(rejectpad));

        const unsigned int padStart = sizeof(rejectpad);
        const unsigned int padCount = len - padStart;
        const byte padValue = M_CheckParm("-reject_pad_with_ff") ? 0xFF : 0x00;
        memset(array + padStart, padValue, padCount);
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

void P_SetupLevel (int episode, int map, int playermask, skill_t skill)
{
    // Indicate level loading start time
    const int starttime = I_GetTimeMS();

    // Reset level stats
    totalkills = totalitems = totalsecret = 0;
    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].killcount      = 0;
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
    snprintf(lumpname, sizeof lumpname, "E%dM%d", episode, map);

    const int lumpnum = W_GetNumForName(lumpname);

    // Reset timers
    leveltime = realleveltime = oldleveltime = 0;

    // Log loading
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

    bodyqueslot = 0;
    deathmatch_p = deathmatchstarts;

    P_InitAmbientSound();
    P_InitMonsters();
    P_OpenWeapons();
    if (fmt & MFMT_HEXEN)
    {
        P_LoadThings_Hexen(lumpnum + ML_THINGS);
    }
    else
    {
        P_LoadThings(lumpnum + ML_THINGS);
    }
    P_CloseWeapons();

    // If deathmatch, randomly spawn the active players
    TimerGame = 0;
    if (deathmatch)
    {
        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (playeringame[i])
            {                   // must give a player spot before deathmatchspawn
                mobj_t *mobj = P_SpawnMobj(playerstarts[i].x << 16,
                               playerstarts[i].y << 16, 0, MT_PLAYER);
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

        const int parm = M_CheckParmWithArgs("-timer", 1);
        if (parm)
        {
            TimerGame = atoi(myargv[parm + 1]) * 35 * 60;
        }
    }

    // Clear respawn queue and spawn specials
    P_SpawnSpecials();

    // Preload graphics and finalize
    if (precache)
        R_PrecacheLevel();

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
    P_InitTerrainTypes();
    P_InitLava();
    R_InitSprites(sprnames);
}
