//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2026 Julia Nechaevskaya
// Copyright(C) 2024-2026 Polina "Aura" N.
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

#include "m_bbox.h"
#include "i_system.h"
#include "doomstat.h"
#include "p_local.h"

#include "id_vars.h"


seg_t     *curline;
side_t    *sidedef;
line_t    *linedef;
sector_t  *frontsector;
sector_t  *backsector;

// [JN] killough: New code which removes 2s linedef limit
drawseg_t *drawsegs;
drawseg_t *ds_p;
unsigned   maxdrawsegs;

// [JN] CPhipps - 
// Instead of clipsegs, let's try using an array with one entry for each column, 
// indicating whether it's blocked by a solid wall yet or not.
byte solidcol[MAXWIDTH];


// -----------------------------------------------------------------------------
// R_ClearDrawSegs
// -----------------------------------------------------------------------------

void R_ClearDrawSegs (void)
{
    ds_p = drawsegs;
}

// -----------------------------------------------------------------------------
// CPhipps - 
// R_ClipWallSegment
//
// Replaces the old R_Clip*WallSegment functions. It draws bits of walls in those 
// columns which aren't solid, and updates the solidcol[] array appropriately
// -----------------------------------------------------------------------------

static void R_ClipWallSegment (int first, const int last, const boolean solid)
{
    const byte *p;

    while (first < last)
    {
        if (solidcol[first])
        {
            if (!(p = memchr(solidcol+first, 0, last-first)))
            {
                return; // All solid
            }
            first = p - solidcol;
        }
        else
        {
            int to;

            if (!(p = memchr(solidcol+first, 1, last-first)))
            {
                to = last;
            }
            else
            {
                to = p - solidcol;
            }

            R_StoreWallRange(first, to-1);

            if (solid)
            {
                memset(solidcol+first,1,to-first);
            }
            first = to;
        }
    }
}

// -----------------------------------------------------------------------------
// R_RecalcLineFlags
// -----------------------------------------------------------------------------

static void R_RecalcLineFlags (line_t *line_def)
{
    line_def->r_validcount = gametic;

    // First decide if the line is closed, normal, or invisible */
    if (!(line_def->flags & ML_TWOSIDED)
    || backsector->interpceilingheight <= frontsector->interpfloorheight
    || backsector->interpfloorheight >= frontsector->interpceilingheight
    ||
    // if door is closed because back is shut:
    (backsector->interpceilingheight <= backsector->interpfloorheight
 
    // preserve a kind of transparent door/lift special effect:
    && (backsector->interpceilingheight >= frontsector->interpceilingheight
    || curline->sidedef->toptexture)

    && (backsector->interpfloorheight <= frontsector->interpfloorheight
    || curline->sidedef->bottomtexture)

    // properly render skies (consider door "open" if both ceilings are sky):
    && (backsector->ceilingpic !=skyflatnum || frontsector->ceilingpic!=skyflatnum)))
    {
        line_def->r_flags = RF_CLOSED;
    }
    else
    {
        // Reject empty lines used for triggers
        //  and special events.
        // Identical floor and ceiling on both sides,
        // identical light levels on both sides,
        // and no middle texture.
        if (backsector->interpceilingheight != frontsector->interpceilingheight
        || backsector->interpfloorheight != frontsector->interpfloorheight
        || curline->sidedef->midtexture
        || backsector->ceilingpic != frontsector->ceilingpic
        || backsector->floorpic != frontsector->floorpic
        || backsector->lightlevel != frontsector->lightlevel
        || backsector->lightbank != frontsector->lightbank)
        {
            line_def->r_flags = 0;
            return;
        }
        else
        {
            line_def->r_flags = RF_IGNORE;
        }
    }

    // cph - I'm too lazy to try and work with offsets in this
    if (curline->sidedef->rowoffset)
    {
        return;
    }

    // Now decide on texture tiling
    if (line_def->flags & ML_TWOSIDED)
    {
        int c;

        // Does top texture need tiling
        if ((c = frontsector->interpceilingheight - backsector->interpceilingheight) > 0
        && (textureheight[texturetranslation[curline->sidedef->toptexture]] > c))
        {
            line_def->r_flags |= RF_TOP_TILE;
        }

        // Does bottom texture need tiling
        if ((c = frontsector->interpfloorheight - backsector->interpfloorheight) > 0
        && (textureheight[texturetranslation[curline->sidedef->bottomtexture]] > c))
        {
            line_def->r_flags |= RF_BOT_TILE;
        }
    }
    else
    {
        int c;

        // Does middle texture need tiling
        if ((c = frontsector->interpceilingheight - frontsector->interpfloorheight) > 0
        && (textureheight[texturetranslation[curline->sidedef->midtexture]] > c))
        {
            line_def->r_flags |= RF_MID_TILE;
        }
    }
}

// -----------------------------------------------------------------------------
// R_ClearClipSegs
// -----------------------------------------------------------------------------

void R_ClearClipSegs (void)
{
    memset(solidcol, 0, MAXWIDTH);
}

// -----------------------------------------------------------------------------
// R_CheckInterpolateSector
// [AM] Interpolate the passed sector, if prudent.
// -----------------------------------------------------------------------------

static void R_CheckInterpolateSector (sector_t *sector)
{
    if (vid_uncapped_fps &&
        // Only if we moved the sector last tic ...
        sector->oldgametic == gametic - 1 &&
        // ... and it has a thinker associated with it.
        sector->specialdata)
    {
        // Interpolate between current and last floor/ceiling position.
        if (sector->floorheight != sector->oldfloorheight)
        {
            sector->interpfloorheight =
                LerpFixed(sector->oldfloorheight, sector->floorheight);
        }
        else
        {
            sector->interpfloorheight = sector->floorheight;
        }
        if (sector->ceilingheight != sector->oldceilingheight)
        {
            sector->interpceilingheight =
                LerpFixed(sector->oldceilingheight, sector->ceilingheight);
        }
        else
        {
            sector->interpceilingheight = sector->ceilingheight;
        }
    }
    else
    {
        sector->interpfloorheight = sector->floorheight;
        sector->interpceilingheight = sector->ceilingheight;
    }
}

// -----------------------------------------------------------------------------
// R_FloorLightLevel
// [PN] Boom: floor light transfer (special 213).
// -----------------------------------------------------------------------------

static int R_FloorLightLevel (const sector_t *sec)
{
    const int floorlightsec = sec->floorlightsec;
    return (floorlightsec >= 0 && floorlightsec < numsectors)
         ? sectors[floorlightsec].lightlevel
         : sec->lightlevel;
}

// -----------------------------------------------------------------------------
// R_CeilingLightLevel
// [PN] Boom: ceiling light transfer (special 261).
// -----------------------------------------------------------------------------

static int R_CeilingLightLevel (const sector_t *sec)
{
    const int ceilinglightsec = sec->ceilinglightsec;
    return (ceilinglightsec >= 0 && ceilinglightsec < numsectors)
         ? sectors[ceilinglightsec].lightlevel
         : sec->lightlevel;
}

// -----------------------------------------------------------------------------
// R_FakeFlat
// [PN] Boom 242 (glass floor) rendering hack, aligned with WinMBF/Woof logic.
// -----------------------------------------------------------------------------

static sector_t *R_FakeFlat (sector_t *sec, sector_t *tempsec,
                             int *floorlightlevel, int *ceilinglightlevel,
                             const boolean back)
{
    if (floorlightlevel)
    {
        *floorlightlevel = R_FloorLightLevel(sec);
    }

    if (ceilinglightlevel)
    {
        *ceilinglightlevel = R_CeilingLightLevel(sec);
    }

    if (sec->heightsec != -1)
    {
        const sector_t *const s = &sectors[sec->heightsec];
        const int view_heightsec = viewplayer->mo->subsector->sector->heightsec;
        const boolean underwater = view_heightsec != -1
                                && viewz <= sectors[view_heightsec].floorheight;

        *tempsec = *sec;

        tempsec->floorheight = s->floorheight;
        tempsec->ceilingheight = s->ceilingheight;
        tempsec->interpfloorheight = s->interpfloorheight;
        tempsec->interpceilingheight = s->interpceilingheight;

        if (underwater)
        {
            tempsec->floorheight = sec->floorheight;
            tempsec->interpfloorheight = sec->interpfloorheight;
            tempsec->ceilingheight = s->floorheight - 1;
            tempsec->interpceilingheight = s->interpfloorheight - 1;

            if (!back)
            {
                tempsec->floorpic = s->floorpic;
                tempsec->floor_xoffs = s->floor_xoffs;
                tempsec->floor_yoffs = s->floor_yoffs;

                if (s->ceilingpic == skyflatnum)
                {
                    tempsec->floorheight = tempsec->ceilingheight + 1;
                    tempsec->interpfloorheight = tempsec->interpceilingheight + 1;
                    tempsec->ceilingpic = tempsec->floorpic;
                    tempsec->ceiling_xoffs = tempsec->floor_xoffs;
                    tempsec->ceiling_yoffs = tempsec->floor_yoffs;
                }
                else
                {
                    tempsec->ceilingpic = s->ceilingpic;
                    tempsec->ceiling_xoffs = s->ceiling_xoffs;
                    tempsec->ceiling_yoffs = s->ceiling_yoffs;
                }

                tempsec->lightlevel = s->lightlevel;

                if (floorlightlevel)
                {
                    *floorlightlevel = R_FloorLightLevel(s);
                }

                if (ceilinglightlevel)
                {
                    *ceilinglightlevel = R_CeilingLightLevel(s);
                }
            }
            else if (view_heightsec != -1
                  && viewz >= sectors[view_heightsec].ceilingheight
                  && sec->ceilingheight > s->ceilingheight)
            {
                tempsec->ceilingheight = s->ceilingheight;
                tempsec->floorheight = s->ceilingheight + 1;
                tempsec->interpceilingheight = s->interpceilingheight;
                tempsec->interpfloorheight = s->interpceilingheight + 1;

                tempsec->ceilingpic = s->ceilingpic;
                tempsec->floorpic = s->ceilingpic;
                tempsec->ceiling_xoffs = s->ceiling_xoffs;
                tempsec->ceiling_yoffs = s->ceiling_yoffs;
                tempsec->floor_xoffs = s->ceiling_xoffs;
                tempsec->floor_yoffs = s->ceiling_yoffs;

                if (s->floorpic != skyflatnum)
                {
                    tempsec->ceilingheight = sec->ceilingheight;
                    tempsec->interpceilingheight = sec->interpceilingheight;
                    tempsec->floorpic = s->floorpic;
                    tempsec->floor_xoffs = s->floor_xoffs;
                    tempsec->floor_yoffs = s->floor_yoffs;
                }

                tempsec->lightlevel = s->lightlevel;

                if (floorlightlevel)
                {
                    *floorlightlevel = R_FloorLightLevel(s);
                }

                if (ceilinglightlevel)
                {
                    *ceilinglightlevel = R_CeilingLightLevel(s);
                }
            }
        }
        else if (view_heightsec != -1
              && viewz >= sectors[view_heightsec].ceilingheight
              && sec->ceilingheight > s->ceilingheight)
        {
            tempsec->ceilingheight = s->ceilingheight;
            tempsec->floorheight = s->ceilingheight + 1;
            tempsec->interpceilingheight = s->interpceilingheight;
            tempsec->interpfloorheight = s->interpceilingheight + 1;

            tempsec->ceilingpic = s->ceilingpic;
            tempsec->floorpic = s->ceilingpic;
            tempsec->ceiling_xoffs = s->ceiling_xoffs;
            tempsec->ceiling_yoffs = s->ceiling_yoffs;
            tempsec->floor_xoffs = s->ceiling_xoffs;
            tempsec->floor_yoffs = s->ceiling_yoffs;

            if (s->floorpic != skyflatnum)
            {
                tempsec->ceilingheight = sec->ceilingheight;
                tempsec->interpceilingheight = sec->interpceilingheight;
                tempsec->floorpic = s->floorpic;
                tempsec->floor_xoffs = s->floor_xoffs;
                tempsec->floor_yoffs = s->floor_yoffs;
            }

            tempsec->lightlevel = s->lightlevel;

            if (floorlightlevel)
            {
                *floorlightlevel = R_FloorLightLevel(s);
            }

            if (ceilinglightlevel)
            {
                *ceilinglightlevel = R_CeilingLightLevel(s);
            }
        }

        sec = tempsec;
    }

    return sec;
}

// -----------------------------------------------------------------------------
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
// -----------------------------------------------------------------------------

static void R_AddLine (seg_t *line)
{
    int			x1;
    int			x2;
    angle_t		angle1;
    angle_t		angle2;
    angle_t		span;
    angle_t		tspan;
    static sector_t tempsec;
    
    curline = line;

    // OPTIMIZE: quickly reject orthogonal back sides.
    // [crispy] remove slime trails
    angle1 = R_PointToAngleCrispy (line->v1->r_x, line->v1->r_y);
    angle2 = R_PointToAngleCrispy (line->v2->r_x, line->v2->r_y);

    // Clip to view edges.
    span = angle1 - angle2;

    // Back side? I.e. backface culling?
    if (span >= ANG180)
    {
        return;
    }

    // Global angle needed by segcalc.
    angle1 -= viewangle;
    angle2 -= viewangle;

    tspan = angle1 + clipangle;
    if (tspan > 2*clipangle)
    {
        tspan -= 2*clipangle;

        // Totally off the left edge?
        if (tspan >= span)
        {
            return;
        }
	    angle1 = clipangle;
    }

    tspan = clipangle - angle2;
    if (tspan > 2*clipangle)
    {
        tspan -= 2*clipangle;

        // Totally off the left edge?
        if (tspan >= span)
        {
            return;
        }
        angle2 = 0-clipangle;
    }

    // The seg is in the view range,
    // but not necessarily visible.
    angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
    angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;
    x1 = viewangletox[angle1];
    x2 = viewangletox[angle2];

    // Does not cross a pixel?
    if (x1 >= x2)  // [JN] killough 1/31/98 -- change == to >= for robustness
    {
        return;
    }

    backsector = line->backsector;
    const sector_t *const realback = backsector;

    // Single sided line?
    if (backsector)
    {
        // [AM] Interpolate sector movement before
        //      running clipping tests.  Frontsector
        //      should already be interpolated.
        R_CheckInterpolateSector(backsector);

        if (backsector->heightsec != -1)
        {
            R_CheckInterpolateSector(&sectors[backsector->heightsec]);
        }

        // [PN] Boom 242: use fake back sector for clipping decisions.
        backsector = R_FakeFlat(backsector, &tempsec, NULL, NULL, true);
    }
    else
    {
        // [JN] If no backsector is present, 
        // just clip the line as a solid segment.
        R_ClipWallSegment (x1, x2, true);
        return;
    }

    linedef = curline->linedef;

    // [PN] Boom 242 path: use WinMBF/Woof-style direct clipping checks.
    // Cached line flags are view/context dependent with fake sectors.
    if (frontsector->heightsec != -1 || (realback && realback->heightsec != -1))
    {
        if (backsector->interpceilingheight <= frontsector->interpfloorheight
         || backsector->interpfloorheight >= frontsector->interpceilingheight
         || (backsector->interpceilingheight <= backsector->interpfloorheight
          && (backsector->interpceilingheight >= frontsector->interpceilingheight
           || curline->sidedef->toptexture)
          && (backsector->interpfloorheight <= frontsector->interpfloorheight
           || curline->sidedef->bottomtexture)
          && (backsector->ceilingpic != skyflatnum
           || frontsector->ceilingpic != skyflatnum)))
        {
            R_ClipWallSegment(x1, x2, true);
            return;
        }

        if (backsector->interpceilingheight != frontsector->interpceilingheight
         || backsector->interpfloorheight != frontsector->interpfloorheight)
        {
            R_ClipWallSegment(x1, x2, false);
            return;
        }

        if (backsector->ceilingpic == frontsector->ceilingpic
         && backsector->floorpic == frontsector->floorpic
         && backsector->lightlevel == frontsector->lightlevel
         && backsector->lightbank == frontsector->lightbank
         && backsector->floor_xoffs == frontsector->floor_xoffs
         && backsector->floor_yoffs == frontsector->floor_yoffs
         && backsector->ceiling_xoffs == frontsector->ceiling_xoffs
         && backsector->ceiling_yoffs == frontsector->ceiling_yoffs
         && backsector->floorlightsec == frontsector->floorlightsec
         && backsector->ceilinglightsec == frontsector->ceilinglightsec
         && curline->sidedef->midtexture == 0)
        {
            return;
        }

        R_ClipWallSegment(x1, x2, false);
        return;
    }

    // [JN] cph - roll up linedef properties in flags
    if (linedef->r_validcount != gametic)
    {
        R_RecalcLineFlags(linedef);
    }

    if (!(linedef->r_flags & RF_IGNORE))
    {
        R_ClipWallSegment (x1, x2, linedef->r_flags & RF_CLOSED);
    }
}

// -----------------------------------------------------------------------------
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true if some part of the bbox might be visible.
// -----------------------------------------------------------------------------

static const int checkcoord[12][4] =
{
    {3,0,2,1},
    {3,0,2,0},
    {3,1,2,0},
    {0},
    {2,0,2,1},
    {0,0,0,0},
    {3,1,3,0},
    {0},
    {2,0,3,1},
    {2,1,3,1},
    {2,1,3,0}
};

// -----------------------------------------------------------------------------
// R_CheckBBox
// -----------------------------------------------------------------------------

static boolean R_CheckBBox (const fixed_t *bspcoord)
{
    angle_t    angle1, angle2;
    int        boxpos;
    const int *check;

    // [PN] Expand bounding boxes by MAXRADIUS to keep wide sprites from
    // disappearing when their centers are just behind a solid wall.
    fixed_t expanded[4];
    expanded[BOXLEFT]   = bspcoord[BOXLEFT]   - MAXRADIUS;
    expanded[BOXRIGHT]  = bspcoord[BOXRIGHT]  + MAXRADIUS;
    expanded[BOXTOP]    = bspcoord[BOXTOP]    + MAXRADIUS;
    expanded[BOXBOTTOM] = bspcoord[BOXBOTTOM] - MAXRADIUS;
    // [PN] Use expanded bbox without touching the original code below.
    #define bspcoord expanded

    // Find the corners of the box that define the edges from current viewpoint.
    boxpos = (viewx <= bspcoord[BOXLEFT] ? 0 : viewx < bspcoord[BOXRIGHT ] ? 1 : 2) +
             (viewy >= bspcoord[BOXTOP ] ? 0 : viewy > bspcoord[BOXBOTTOM] ? 4 : 8);

    if (boxpos == 5)
    {
        return true;
    }

    check = checkcoord[boxpos];

    angle1 = R_PointToAngleCrispy (bspcoord[check[0]], bspcoord[check[1]]) - viewangle;
    angle2 = R_PointToAngleCrispy (bspcoord[check[2]], bspcoord[check[3]]) - viewangle;

    // [PN] Restore original bspcoord symbol outside this block.
    #undef bspcoord

    // [JN] cph - replaced old code, which was unclear and badly commented
    // Much more efficient code now
    if ((signed)angle1 < (signed)angle2)
    { 
        // it's "behind" us
        // Either angle1 or angle2 is behind us, so it doesn't matter if we 
        // change it to the corect sign
        if ((angle1 >= ANG180) && (angle1 < ANG270))
        {
            angle1 = INT_MAX; // which is ANG180-1
        }
        else
        {
            angle2 = INT_MIN;
        }
    }

    if ((signed)angle2 >= (signed)clipangle) return false; // Both off left edge
    if ((signed)angle1 <= -(signed)clipangle) return false; // Both off right edge
    if ((signed)angle1 >= (signed)clipangle) angle1 = clipangle; // Clip at left edge
    if ((signed)angle2 <= -(signed)clipangle) angle2 = 0-clipangle; // Clip at right edge

    // Find the first clippost that touches the 
    // source post (adjacent pixels are touching).
    angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
    angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;
    {
        int sx1 = viewangletox[angle1];
        int sx2 = viewangletox[angle2];

        // Does not cross a pixel.
        if (sx1 == sx2)
        {
            return false;
        }

        if (!memchr(solidcol+sx1, 0, sx2-sx1)) return false; 
        // All columns it covers are already solidly covered
    }

    return true;
}

// -----------------------------------------------------------------------------
// R_Subsector
// Determine floor/ceiling planes. Add sprites of things in sector.
// Draw one or more line segments.
//
// [JN] killough 1/31/98 -- made static, polished
// -----------------------------------------------------------------------------

static void R_Subsector (int num)
{
    subsector_t *sub = &subsectors[num];
    seg_t       *line = &segs[sub->firstline];
    int   count = sub->numlines;
    sector_t tempsec;
    int floorlightlevel;
    int ceilinglightlevel;

#ifdef RANGECHECK
    if (num >= numsubsectors)
	I_Error ("R_Subsector: ss %i with numss = %i", num, numsubsectors);
#endif

    frontsector = sub->sector;

    // [AM] Interpolate sector movement.  Usually only needed
    //      when you're standing inside the sector.
    R_CheckInterpolateSector(frontsector);

    if (frontsector->heightsec != -1)
    {
        R_CheckInterpolateSector(&sectors[frontsector->heightsec]);
    }

    // [PN] Boom 242: fake floor/ceiling rendering in tagged sectors.
    frontsector = R_FakeFlat(frontsector, &tempsec, &floorlightlevel, &ceilinglightlevel, false);

    floorplane = (frontsector->interpfloorheight < viewz
               || (frontsector->heightsec != -1
                && sectors[frontsector->heightsec].ceilingpic == skyflatnum)) ?
                 R_FindPlane (frontsector->interpfloorheight,
                              // [crispy] add support for MBF sky transfers
                              frontsector->floorpic == skyflatnum &&
                              frontsector->sky & PL_SKYFLAT ? frontsector->sky :
                              frontsector->floorpic,
                              floorlightlevel,
                              frontsector->lightbank,
                              frontsector->floor_xoffs,
                              frontsector->floor_yoffs) : NULL;

    ceilingplane = (frontsector->interpceilingheight > viewz
                 || frontsector->ceilingpic == skyflatnum
                 || (frontsector->heightsec != -1
                  && sectors[frontsector->heightsec].floorpic == skyflatnum)) ?
                   R_FindPlane (frontsector->interpceilingheight,
                                // [crispy] add support for MBF sky transfers
                                frontsector->ceilingpic == skyflatnum &&
                                frontsector->sky & PL_SKYFLAT ? frontsector->sky :
                                frontsector->ceilingpic,
                                ceilinglightlevel,
                                frontsector->lightbank,
                                frontsector->ceiling_xoffs,
                                frontsector->ceiling_yoffs) : NULL;

    // BSP is traversed by subsector.
    // A sector might have been split into several 
    //  subsectors during BSP building.
    // Thus we check whether its already added.
    if (sub->sector->validcount != validcount && (!automapactive || automap_overlay))
    {
        sub->sector->validcount = validcount;
        // [PN] Boom 242: pass real sector; fake one would break visited tracking.
        R_AddSprites (sub->sector);
    }

    while (count--)
    {
        R_AddLine (line++);
    }
}

// -----------------------------------------------------------------------------
// RenderBSPNode
// Renders all subsectors below a given node, traversing subtree recursively.
// Just call with BSP root.
//
// [JN] killough 5/2/98: reformatted, removed tail recursion
// -----------------------------------------------------------------------------

void R_RenderBSPNode (int bspnum)
{
    while (!(bspnum & NF_SUBSECTOR))  // Found a subsector?
    {
        const node_t *bsp = &nodes[bspnum];

        // Decide which side the view point is on.
        int side = R_PointOnSide(viewx, viewy, bsp);

        // Recursively divide front space.
        R_RenderBSPNode(bsp->children[side]);

        // Possibly divide back space.
        if (!R_CheckBBox(bsp->bbox[side^1]))
        {
            return;
        }

        bspnum = bsp->children[side^1];
    }

    R_Subsector(bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}
