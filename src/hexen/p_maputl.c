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


#include "h2def.h"
#include "i_system.h"
#include "m_bbox.h"
#include "m_misc.h"
#include "p_local.h"

static mobj_t *RoughBlockCheck(mobj_t * mo, int index);

/*
===================
=
= P_AproxDistance
=
= Gives an estimation of distance (not exact)
=
===================
*/

fixed_t P_AproxDistance(fixed_t dx, fixed_t dy)
{
    dx = abs(dx);
    dy = abs(dy);
    if (dx < dy)
        return dx + dy - (dx >> 1);
    return dx + dy - (dy >> 1);
}


/*
==================
=
= P_PointOnLineSide
=
= Returns 0 or 1
==================
*/

int P_PointOnLineSide(fixed_t x, fixed_t y, line_t * line)
{
    fixed_t dx, dy;
    fixed_t left, right;

    if (!line->dx)
    {
        if (x <= line->v1->x)
            return line->dy > 0;
        return line->dy < 0;
    }
    if (!line->dy)
    {
        if (y <= line->v1->y)
            return line->dx < 0;
        return line->dx > 0;
    }

    dx = (x - line->v1->x);
    dy = (y - line->v1->y);

    left = FixedMul(line->dy >> FRACBITS, dx);
    right = FixedMul(dy, line->dx >> FRACBITS);

    if (right < left)
        return 0;               // front side
    return 1;                   // back side
}


/*
=================
=
= P_BoxOnLineSide
=
= Considers the line to be infinite
= Returns side 0 or 1, -1 if box crosses the line
=================
*/

int P_BoxOnLineSide(fixed_t * tmbox, line_t * ld)
{
    int p1 = 0, p2 = 0;

    switch (ld->slopetype)
    {
        case ST_HORIZONTAL:
            p1 = tmbox[BOXTOP] > ld->v1->y;
            p2 = tmbox[BOXBOTTOM] > ld->v1->y;
            if (ld->dx < 0)
            {
                p1 ^= 1;
                p2 ^= 1;
            }
            break;
        case ST_VERTICAL:
            p1 = tmbox[BOXRIGHT] < ld->v1->x;
            p2 = tmbox[BOXLEFT] < ld->v1->x;
            if (ld->dy < 0)
            {
                p1 ^= 1;
                p2 ^= 1;
            }
            break;
        case ST_POSITIVE:
            p1 = P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXTOP], ld);
            p2 = P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld);
            break;
        case ST_NEGATIVE:
            p1 = P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXTOP], ld);
            p2 = P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld);
            break;
    }

    if (p1 == p2)
        return p1;
    return -1;
}

/*
==================
=
= P_PointOnDivlineSide
=
= Returns 0 or 1
==================
*/

int P_PointOnDivlineSide(fixed_t x, fixed_t y, const divline_t * line)
{
    fixed_t dx, dy;
    fixed_t left, right;

    if (!line->dx)
    {
        if (x <= line->x)
            return line->dy > 0;
        return line->dy < 0;
    }
    if (!line->dy)
    {
        if (y <= line->y)
            return line->dx < 0;
        return line->dx > 0;
    }

    dx = (x - line->x);
    dy = (y - line->y);

// try to quickly decide by looking at sign bits
    if ((line->dy ^ line->dx ^ dx ^ dy) & 0x80000000)
    {
        if ((line->dy ^ dx) & 0x80000000)
            return 1;           // (left is negative)
        return 0;
    }

    left = FixedMul(line->dy >> 8, dx >> 8);
    right = FixedMul(dy >> 8, line->dx >> 8);

    if (right < left)
        return 0;               // front side
    return 1;                   // back side
}



/*
==============
=
= P_MakeDivline
=
==============
*/

void P_MakeDivline(line_t * li, divline_t * dl)
{
    dl->x = li->v1->x;
    dl->y = li->v1->y;
    dl->dx = li->dx;
    dl->dy = li->dy;
}


/*
===============
=
= P_InterceptVector
=
= Returns the fractional intercept point along the first divline
=
= This is only called by the addthings and addlines traversers
===============
*/

fixed_t P_InterceptVector(divline_t * v2, divline_t * v1)
{
#if 1
    fixed_t frac, num, den;

    den = FixedMul(v1->dy >> 8, v2->dx) - FixedMul(v1->dx >> 8, v2->dy);
    if (den == 0)
        return 0;
//              I_Error ("P_InterceptVector: parallel");
    num = FixedMul((v1->x - v2->x) >> 8, v1->dy) +
        FixedMul((v2->y - v1->y) >> 8, v1->dx);
    frac = FixedDiv(num, den);

    return frac;
#else
    float frac, num, den, v1x, v1y, v1dx, v1dy, v2x, v2y, v2dx, v2dy;

    v1x = (float) v1->x / FRACUNIT;
    v1y = (float) v1->y / FRACUNIT;
    v1dx = (float) v1->dx / FRACUNIT;
    v1dy = (float) v1->dy / FRACUNIT;
    v2x = (float) v2->x / FRACUNIT;
    v2y = (float) v2->y / FRACUNIT;
    v2dx = (float) v2->dx / FRACUNIT;
    v2dy = (float) v2->dy / FRACUNIT;

    den = v1dy * v2dx - v1dx * v2dy;
    if (den == 0)
        return 0;               // parallel
    num = (v1x - v2x) * v1dy + (v2y - v1y) * v1dx;
    frac = num / den;

    return frac * FRACUNIT;
#endif
}

/*
==================
=
= P_LineOpening
=
= Sets opentop and openbottom to the window through a two sided line
= OPTIMIZE: keep this precalculated
==================
*/

fixed_t opentop, openbottom, openrange;
fixed_t lowfloor;

void P_LineOpening(line_t * linedef)
{
    const sector_t *front;
    const sector_t *back;

    if (linedef->sidenum[1] == -1)
    {                           // single sided line
        openrange = 0;
        return;
    }

    front = linedef->frontsector;
    back = linedef->backsector;

    if (front->ceilingheight < back->ceilingheight)
        opentop = front->ceilingheight;
    else
        opentop = back->ceilingheight;
    if (front->floorheight > back->floorheight)
    {
        openbottom = front->floorheight;
        lowfloor = back->floorheight;
        tmfloorpic = front->floorpic;
    }
    else
    {
        openbottom = back->floorheight;
        lowfloor = front->floorheight;
        tmfloorpic = back->floorpic;
    }

    openrange = opentop - openbottom;
}

/*
===============================================================================

						THING POSITION SETTING

===============================================================================
*/

/*
===================
=
= P_UnsetThingPosition
=
= Unlinks a thing from block map and sectors
=
===================
*/

void P_UnsetThingPosition(mobj_t * thing)
{
    int blockx, blocky;

    if (!(thing->flags & MF_NOSECTOR))
    {                           // inert things don't need to be in blockmap
// unlink from subsector
        if (thing->snext)
            thing->snext->sprev = thing->sprev;
        if (thing->sprev)
            thing->sprev->snext = thing->snext;
        else
            thing->subsector->sector->thinglist = thing->snext;
    }

    if (!(thing->flags & MF_NOBLOCKMAP))
    {                           // inert things don't need to be in blockmap
// unlink from block map
        if (thing->bnext)
            thing->bnext->bprev = thing->bprev;
        if (thing->bprev)
            thing->bprev->bnext = thing->bnext;
        else
        {
            blockx = (thing->x - bmaporgx) >> MAPBLOCKSHIFT;
            blocky = (thing->y - bmaporgy) >> MAPBLOCKSHIFT;
            if (blockx >= 0 && blockx < bmapwidth
                && blocky >= 0 && blocky < bmapheight)
                blocklinks[blocky * bmapwidth + blockx] = thing->bnext;
        }
    }
}


/*
===================
=
= P_SetThingPosition
=
= Links a thing into both a block and a subsector based on it's x y
= Sets thing->subsector properly
=
===================
*/

void P_SetThingPosition(mobj_t * thing)
{
    subsector_t *ss;
    sector_t *sec;
    int blockx, blocky;
    mobj_t **link;

//
// link into subsector
//
    ss = R_PointInSubsector(thing->x, thing->y);
    thing->subsector = ss;
    if (!(thing->flags & MF_NOSECTOR))
    {                           // invisible things don't go into the sector links
        sec = ss->sector;

        thing->sprev = NULL;
        thing->snext = sec->thinglist;
        if (sec->thinglist)
            sec->thinglist->sprev = thing;
        sec->thinglist = thing;
    }

//
// link into blockmap
//
    if (!(thing->flags & MF_NOBLOCKMAP))
    {                           // inert things don't need to be in blockmap
        blockx = (thing->x - bmaporgx) >> MAPBLOCKSHIFT;
        blocky = (thing->y - bmaporgy) >> MAPBLOCKSHIFT;
        if (blockx >= 0 && blockx < bmapwidth && blocky >= 0
            && blocky < bmapheight)
        {
            link = &blocklinks[blocky * bmapwidth + blockx];
            thing->bprev = NULL;
            thing->bnext = *link;
            if (*link)
                (*link)->bprev = thing;
            *link = thing;
        }
        else
        {                       // thing is off the map
            thing->bnext = thing->bprev = NULL;
        }
    }
}



/*
===============================================================================

						BLOCK MAP ITERATORS

For each line/thing in the given mapblock, call the passed function.
If the function returns false, exit with false without checking anything else.

===============================================================================
*/

/*
==================
=
= P_BlockLinesIterator
=
= The validcount flags are used to avoid checking lines
= that are marked in multiple mapblocks, so increment validcount before
= the first call to P_BlockLinesIterator, then make one or more calls to it
===================
*/

boolean P_BlockLinesIterator(int x, int y, boolean(*func) (line_t *))
{
    int offset;
    short *list;
    line_t *ld;

    int i;
    polyblock_t *polyLink;
    seg_t **tempSeg;

    if (x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
        return true;
    offset = y * bmapwidth + x;

    polyLink = PolyBlockMap[offset];
    while (polyLink)
    {
        if (polyLink->polyobj)
        {
            if (polyLink->polyobj->validcount != validcount)
            {
                polyLink->polyobj->validcount = validcount;
                tempSeg = polyLink->polyobj->segs;
                for (i = 0; i < polyLink->polyobj->numsegs; i++, tempSeg++)
                {
                    if ((*tempSeg)->linedef->validcount == validcount)
                    {
                        continue;
                    }
                    (*tempSeg)->linedef->validcount = validcount;
                    if (!func((*tempSeg)->linedef))
                    {
                        return false;
                    }
                }
            }
        }
        polyLink = polyLink->next;
    }

    offset = *(blockmap + offset);

    for (list = blockmaplump + offset; *list != -1; list++)
    {
        ld = &lines[*list];
        if (ld->validcount == validcount)
            continue;           // line has already been checked
        ld->validcount = validcount;

        if (!func(ld))
            return false;
    }

    return true;                // everything was checked
}


/*
==================
=
= P_BlockThingsIterator
=
==================
*/

boolean P_BlockThingsIterator(int x, int y, boolean(*func) (mobj_t *))
{
    mobj_t *mobj;

    if (x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
        return true;

    LINKED_LIST_CHECK_NO_CYCLE(mobj_t, blocklinks[y*bmapwidth+x], bnext);

    for (mobj = blocklinks[y * bmapwidth + x]; mobj; mobj = mobj->bnext)
        if (!func(mobj))
            return false;

    return true;
}

/*
===============================================================================

					INTERCEPT ROUTINES

===============================================================================
*/

intercept_t *intercepts, *intercept_p; // [crispy] remove INTERCEPTS limit

// [crispy] remove INTERCEPTS limit
void check_intercept (void)
{
    static size_t num_intercepts;
    const size_t offset = intercept_p - intercepts;

    if (offset >= num_intercepts)
    {
        num_intercepts = num_intercepts ? num_intercepts * 2 : MAXINTERCEPTS;
        intercepts = I_Realloc(intercepts, sizeof(*intercepts) * num_intercepts);
        intercept_p = intercepts + offset;
    }
}

divline_t trace;
static boolean earlyout;

/*
==================
=
= PIT_AddLineIntercepts
=
= Looks for lines in the given block that intercept the given trace
= to add to the intercepts list
= A line is crossed if its endpoints are on opposite sides of the trace
= Returns true if earlyout and a solid line hit
==================
*/

static boolean PIT_AddLineIntercepts(line_t * ld)
{
    int s1, s2;
    fixed_t frac;
    divline_t dl;

// avoid precision problems with two routines
    if (trace.dx > FRACUNIT * 16 || trace.dy > FRACUNIT * 16
        || trace.dx < -FRACUNIT * 16 || trace.dy < -FRACUNIT * 16)
    {
        s1 = P_PointOnDivlineSide(ld->v1->x, ld->v1->y, &trace);
        s2 = P_PointOnDivlineSide(ld->v2->x, ld->v2->y, &trace);
    }
    else
    {
        s1 = P_PointOnLineSide(trace.x, trace.y, ld);
        s2 = P_PointOnLineSide(trace.x + trace.dx, trace.y + trace.dy, ld);
    }
    if (s1 == s2)
        return true;            // line isn't crossed

//
// hit the line
//
    P_MakeDivline(ld, &dl);
    frac = P_InterceptVector(&trace, &dl);
    if (frac < 0)
        return true;            // behind source

// try to early out the check
    if (earlyout && frac < FRACUNIT && !ld->backsector)
        return false;           // stop checking

    check_intercept(); // [crispy] remove INTERCEPTS limit
    intercept_p->frac = frac;
    intercept_p->isaline = true;
    intercept_p->d.line = ld;
    intercept_p++;

    return true;                // continue
}



/*
==================
=
= PIT_AddThingIntercepts
=
==================
*/

static boolean PIT_AddThingIntercepts(mobj_t * thing)
{
    fixed_t x1, y1, x2, y2;
    int s1, s2;
    boolean tracepositive;
    divline_t dl;
    fixed_t frac;

    tracepositive = (trace.dx ^ trace.dy) > 0;

    // check a corner to corner crossection for hit

    if (tracepositive)
    {
        x1 = thing->x - thing->radius;
        y1 = thing->y + thing->radius;

        x2 = thing->x + thing->radius;
        y2 = thing->y - thing->radius;
    }
    else
    {
        x1 = thing->x - thing->radius;
        y1 = thing->y - thing->radius;

        x2 = thing->x + thing->radius;
        y2 = thing->y + thing->radius;
    }
    s1 = P_PointOnDivlineSide(x1, y1, &trace);
    s2 = P_PointOnDivlineSide(x2, y2, &trace);
    if (s1 == s2)
        return true;            // line isn't crossed

    dl.x = x1;
    dl.y = y1;
    dl.dx = x2 - x1;
    dl.dy = y2 - y1;
    frac = P_InterceptVector(&trace, &dl);
    if (frac < 0)
        return true;            // behind source
    check_intercept(); // [crispy] remove INTERCEPTS limit
    intercept_p->frac = frac;
    intercept_p->isaline = false;
    intercept_p->d.thing = thing;
    intercept_p++;

    return true;                // keep going
}


/*
====================
=
= P_TraverseIntercepts
=
= Returns true if the traverser function returns true for all lines
====================
*/

static boolean P_TraverseIntercepts(traverser_t func, fixed_t maxfrac)
{
    int count;
    fixed_t dist;
    intercept_t *scan, *in;

    count = intercept_p - intercepts;
    in = 0;                     // shut up compiler warning

    while (count--)
    {
        dist = INT_MAX;
        for (scan = intercepts; scan < intercept_p; scan++)
            if (scan->frac < dist)
            {
                dist = scan->frac;
                in = scan;
            }

        if (dist > maxfrac)
            return true;        // checked everything in range
#if 0
        {                       // don't check these yet, ther may be others inserted
            in = scan = intercepts;
            for (scan = intercepts; scan < intercept_p; scan++)
                if (scan->frac > maxfrac)
                    *in++ = *scan;
            intercept_p = in;
            return false;
        }
#endif

        if (!func(in))
            return false;       // don't bother going farther
        in->frac = INT_MAX;
    }

    return true;                // everything was traversed
}



/*
==================
=
= P_PathTraverse
=
= Traces a line from x1,y1 to x2,y2, calling the traverser function for each
= Returns true if the traverser function returns true for all lines
==================
*/

boolean P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                       int flags, boolean(*trav) (intercept_t *))
{
    fixed_t xt1, yt1, xt2, yt2;
    fixed_t xstep, ystep;
    fixed_t partial;
    fixed_t xintercept, yintercept;
    int mapx, mapy, mapxstep, mapystep;
    int count;

    earlyout = (flags & PT_EARLYOUT) != 0;

    validcount++;
    intercept_p = intercepts;

    if (((x1 - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0)
        x1 += FRACUNIT;         // don't side exactly on a line
    if (((y1 - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0)
        y1 += FRACUNIT;         // don't side exactly on a line
    trace.x = x1;
    trace.y = y1;
    trace.dx = x2 - x1;
    trace.dy = y2 - y1;

    x1 -= bmaporgx;
    y1 -= bmaporgy;
    xt1 = x1 >> MAPBLOCKSHIFT;
    yt1 = y1 >> MAPBLOCKSHIFT;

    x2 -= bmaporgx;
    y2 -= bmaporgy;
    xt2 = x2 >> MAPBLOCKSHIFT;
    yt2 = y2 >> MAPBLOCKSHIFT;

    if (xt2 > xt1)
    {
        mapxstep = 1;
        partial = FRACUNIT - ((x1 >> MAPBTOFRAC) & (FRACUNIT - 1));
        ystep = FixedDiv(y2 - y1, abs(x2 - x1));
    }
    else if (xt2 < xt1)
    {
        mapxstep = -1;
        partial = (x1 >> MAPBTOFRAC) & (FRACUNIT - 1);
        ystep = FixedDiv(y2 - y1, abs(x2 - x1));
    }
    else
    {
        mapxstep = 0;
        partial = FRACUNIT;
        ystep = 256 * FRACUNIT;
    }
    yintercept = (y1 >> MAPBTOFRAC) + FixedMul(partial, ystep);


    if (yt2 > yt1)
    {
        mapystep = 1;
        partial = FRACUNIT - ((y1 >> MAPBTOFRAC) & (FRACUNIT - 1));
        xstep = FixedDiv(x2 - x1, abs(y2 - y1));
    }
    else if (yt2 < yt1)
    {
        mapystep = -1;
        partial = (y1 >> MAPBTOFRAC) & (FRACUNIT - 1);
        xstep = FixedDiv(x2 - x1, abs(y2 - y1));
    }
    else
    {
        mapystep = 0;
        partial = FRACUNIT;
        xstep = 256 * FRACUNIT;
    }
    xintercept = (x1 >> MAPBTOFRAC) + FixedMul(partial, xstep);


//
// step through map blocks
// Count is present to prevent a round off error from skipping the break
    mapx = xt1;
    mapy = yt1;

    for (count = 0; count < 64; count++)
    {
        if (flags & PT_ADDLINES)
        {
            if (!P_BlockLinesIterator(mapx, mapy, PIT_AddLineIntercepts))
                return false;   // early out
        }
        if (flags & PT_ADDTHINGS)
        {
            if (!P_BlockThingsIterator(mapx, mapy, PIT_AddThingIntercepts))
                return false;   // early out
        }

        if (mapx == xt2 && mapy == yt2)
            break;

        if ((yintercept >> FRACBITS) == mapy)
        {
            yintercept += ystep;
            mapx += mapxstep;
        }
        else if ((xintercept >> FRACBITS) == mapx)
        {
            xintercept += xstep;
            mapy += mapystep;
        }

    }


//
// go through the sorted list
//
    return P_TraverseIntercepts(trav, FRACUNIT);
}

//===========================================================================
//
// P_RoughMonsterSearch
//
// Searches though the surrounding mapblocks for monsters/players
//              distance is in MAPBLOCKUNITS
//===========================================================================

mobj_t *P_RoughMonsterSearch(mobj_t * mo, int distance)
{
    int blockX;
    int blockY;
    int startX, startY;
    int blockIndex;
    int firstStop;
    int secondStop;
    int thirdStop;
    int finalStop;
    int count;
    mobj_t *target;

    startX = (mo->x - bmaporgx) >> MAPBLOCKSHIFT;
    startY = (mo->y - bmaporgy) >> MAPBLOCKSHIFT;

    if (startX >= 0 && startX < bmapwidth && startY >= 0
        && startY < bmapheight)
    {
        target = RoughBlockCheck(mo, startY * bmapwidth + startX);
        if (target != NULL)
        {                       // found a target right away
            return target;
        }
    }
    for (count = 1; count <= distance; count++)
    {
        blockX = startX - count;
        blockY = startY - count;

        if (blockY < 0)
        {
            blockY = 0;
        }
        else if (blockY >= bmapheight)
        {
            blockY = bmapheight - 1;
        }
        if (blockX < 0)
        {
            blockX = 0;
        }
        else if (blockX >= bmapwidth)
        {
            blockX = bmapwidth - 1;
        }
        blockIndex = blockY * bmapwidth + blockX;
        firstStop = startX + count;
        if (firstStop < 0)
        {
            continue;
        }
        if (firstStop >= bmapwidth)
        {
            firstStop = bmapwidth - 1;
        }
        secondStop = startY + count;
        if (secondStop < 0)
        {
            continue;
        }
        if (secondStop >= bmapheight)
        {
            secondStop = bmapheight - 1;
        }
        thirdStop = secondStop * bmapwidth + blockX;
        secondStop = secondStop * bmapwidth + firstStop;
        firstStop += blockY * bmapwidth;
        finalStop = blockIndex;

        // Trace the first block section (along the top)
        for (; blockIndex <= firstStop; blockIndex++)
        {
            target = RoughBlockCheck(mo, blockIndex);
            if (target != NULL)
            {
                return target;
            }
        }
        // Trace the second block section (right edge)
        for (blockIndex--; blockIndex <= secondStop; blockIndex += bmapwidth)
        {
            target = RoughBlockCheck(mo, blockIndex);
            if (target != NULL)
            {
                return target;
            }
        }
        // Trace the third block section (bottom edge)
        for (blockIndex -= bmapwidth; blockIndex >= thirdStop; blockIndex--)
        {
            target = RoughBlockCheck(mo, blockIndex);
            if (target != NULL)
            {
                return target;
            }
        }
        // Trace the final block section (left edge)
        for (blockIndex++; blockIndex > finalStop; blockIndex -= bmapwidth)
        {
            target = RoughBlockCheck(mo, blockIndex);
            if (target != NULL)
            {
                return target;
            }
        }
    }
    return NULL;
}

//===========================================================================
//
// RoughBlockCheck
//
//===========================================================================

static mobj_t *RoughBlockCheck(mobj_t * mo, int index)
{
    mobj_t *link;
    mobj_t *master;
    angle_t angle;

    link = blocklinks[index];
    while (link)
    {
        if (mo->player)         // Minotaur looking around player
        {
            if ((link->flags & MF_COUNTKILL) ||
                (link->player && (link != mo)))
            {
                if (!(link->flags & MF_SHOOTABLE))
                {
                    link = link->bnext;
                    continue;
                }
                if (link->flags2 & MF2_DORMANT)
                {
                    link = link->bnext;
                    continue;
                }
                if ((link->type == MT_MINOTAUR) &&
                    (link->special1.m == mo))
                {
                    link = link->bnext;
                    continue;
                }
                if (netgame && !deathmatch && link->player)
                {
                    link = link->bnext;
                    continue;
                }
                if (P_CheckSight(mo, link))
                {
                    return link;
                }
            }
            link = link->bnext;
        }
        else if (mo->type == MT_MINOTAUR)       // looking around minotaur
        {
            master = mo->special1.m;
            if ((link->flags & MF_COUNTKILL) ||
                (link->player && (link != master)))
            {
                if (!(link->flags & MF_SHOOTABLE))
                {
                    link = link->bnext;
                    continue;
                }
                if (link->flags2 & MF2_DORMANT)
                {
                    link = link->bnext;
                    continue;
                }
                if ((link->type == MT_MINOTAUR) &&
                    (link->special1.m == mo->special1.m))
                {
                    link = link->bnext;
                    continue;
                }
                if (netgame && !deathmatch && link->player)
                {
                    link = link->bnext;
                    continue;
                }
                if (P_CheckSight(mo, link))
                {
                    return link;
                }
            }
            link = link->bnext;
        }
        else if (mo->type == MT_MSTAFF_FX2)     // bloodscourge
        {
            if ((link->flags & MF_COUNTKILL ||
                 (link->player && link != mo->target))
                && !(link->flags2 & MF2_DORMANT))
            {
                if (!(link->flags & MF_SHOOTABLE))
                {
                    link = link->bnext;
                    continue;
                }
                if (netgame && !deathmatch && link->player)
                {
                    link = link->bnext;
                    continue;
                }
                else if (P_CheckSight(mo, link))
                {
                    master = mo->target;
                    angle = R_PointToAngle2(master->x, master->y,
                                            link->x, link->y) - master->angle;
                    angle >>= 24;
                    if (angle > 226 || angle < 30)
                    {
                        return link;
                    }
                }
            }
            link = link->bnext;
        }
        else                    // spirits
        {
            if ((link->flags & MF_COUNTKILL ||
                 (link->player && link != mo->target))
                && !(link->flags2 & MF2_DORMANT))
            {
                if (!(link->flags & MF_SHOOTABLE))
                {
                    link = link->bnext;
                    continue;
                }
                if (netgame && !deathmatch && link->player)
                {
                    link = link->bnext;
                    continue;
                }
                if (link == mo->target)
                {
                    link = link->bnext;
                    continue;
                }
                else if (P_CheckSight(mo, link))
                {
                    return link;
                }
            }
            link = link->bnext;
        }
    }
    return NULL;
}
