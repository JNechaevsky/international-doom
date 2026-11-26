//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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


#include "i_system.h"
#include "doomdef.h"
#include "r_local.h"
#include "r_collight.h"

#include "id_func.h"


// True if any of the segs textures might be visible.
static boolean segtextured;

// False if the back side is the same plane.
static boolean markfloor;
static boolean markceiling;

static boolean maskedtexture;
static int  toptexture;
static int  bottomtexture;
static int  midtexture;

// Regular wall
angle_t         rw_normalangle;
fixed_t         rw_distance;
static angle_t  rw_centerangle;
static fixed_t  rw_x;
static fixed_t  rw_stopx;
static fixed_t  rw_offset;
static int64_t  rw_scale;
static int64_t  rw_scalestep;
static fixed_t  rw_midtexturemid;
static fixed_t  rw_toptexturemid;
static fixed_t  rw_bottomtexturemid;

static fixed_t  worldtop;
static fixed_t  worldbottom;
static fixed_t  worldhigh;
static fixed_t  worldlow;

static int64_t  pixhigh;
static int64_t  pixlow;
static int64_t  pixhighstep;
static int64_t  pixlowstep;

static int64_t  topfrac;
static int64_t  topstep;

static int64_t  bottomfrac;
static int64_t  bottomstep;

// [PN] Precise wall scaling.
#define WALLSCALEBITS   (20 - FRACBITS)
#define WALLEDGEBITS    (2 * FRACBITS)
#define WALLEDGEUNIT    (INT64_C(1) << WALLEDGEBITS)

static inline int64_t R_ToWallScale (const fixed_t scale)
{
    return (int64_t)scale * (1 << WALLSCALEBITS);
}

static inline fixed_t R_WallScaleToFixed (const int64_t scale)
{
    return (fixed_t)(scale >> WALLSCALEBITS);
}

static inline int R_WallTopToScreen (const int64_t edge)
{
    const int64_t y = (edge + WALLEDGEUNIT - 1) >> WALLEDGEBITS;

    if (y < -1)
    {
        return -1;
    }
    if (y > viewheight)
    {
        return viewheight;
    }

    return (int)y;
}

static inline int R_WallBottomToScreen (const int64_t edge)
{
    const int64_t y = edge >> WALLEDGEBITS;

    if (y < -1)
    {
        return -1;
    }
    if (y > viewheight)
    {
        return viewheight;
    }

    return (int)y;
}

lighttable_t **walllights;

int *maskedtexturecol;      // [JN] 32-bit integer math


// -----------------------------------------------------------------------------
// R_RenderMaskedSegRange
// -----------------------------------------------------------------------------

void R_RenderMaskedSegRange (const drawseg_t *const ds, int x1, int x2)
{
    // Calculate light table.
    // Use different light tables
    //   for horizontal / vertical / diagonal. Diagonal?
    // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
    curline = ds->curline;
    frontsector = curline->frontsector;
    backsector = curline->backsector;
    const int texnum = texturetranslation[curline->sidedef->midtexture];

    // [crispy] smooth diminishing lighting and smoother fake contrast
    const int lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT)
                       + (extralight * LIGHTBRIGHT)
                       + curline->fakecontrast;

    walllights = scalelight[BETWEEN(0, LIGHTLEVELS-1, lightnum)];

    maskedtexturecol = ds->maskedtexturecol;
    int64_t scale = R_ToWallScale(ds->scale1) + (x1 - ds->x1) * ds->scalestep;

    mfloorclip = ds->sprbottomclip;
    mceilingclip = ds->sprtopclip;

    // find positioning
    if (curline->linedef->flags & ML_DONTPEGBOTTOM)
    {
        dc_texturemid = frontsector->interpfloorheight > backsector->interpfloorheight
                      ? frontsector->interpfloorheight : backsector->interpfloorheight;
        dc_texturemid = dc_texturemid + textureheight[texnum] - viewz;
    }
    else
    {
        dc_texturemid = frontsector->interpceilingheight < backsector->interpceilingheight
                      ? frontsector->interpceilingheight : backsector->interpceilingheight;
        dc_texturemid = dc_texturemid - viewz;
    }

    dc_texturemid += curline->sidedef->rowoffset;

    if (fixedcolormap)
        dc_colormap[0] = dc_colormap[1] = fixedcolormap;

    // [JN] H+H: Translucent middle texture
    if (curline->linedef->special == 260)
        colfunc = tlcolfunc;

    // draw the columns
    for (dc_x = x1 ; dc_x <= x2 ; dc_x++, scale += ds->scalestep)
    {
        // calculate lighting
        if (maskedtexturecol[dc_x] != INT_MAX)  // [JN] 32-bit integer math
        {
            spryscale = R_WallScaleToFixed(scale);

            if (!fixedcolormap)
            {
                unsigned const int index = (spryscale / vid_resolution) >> LIGHTSCALESHIFT;

                // [crispy] brightmaps for mid-textures
                dc_brightmap = texturebrightmap[texnum];
                // [PN] Fast path keeps vanilla pointer when sector uses neutral bank 0.
                const lighttable_t *const base = walllights[MIN(index, MAXLIGHTSCALE-1)];
                dc_colormap[0] = frontsector->lightbank
                               ? R_ColLight_Apply(frontsector->lightbank, base)
                               : (lighttable_t *)base;
                dc_colormap[1] = vis_brightmaps ? colormaps : dc_colormap[0];
            }

            // [crispy] apply Killough's int64 sprtopscreen overflow fix
            // from winmbf/Source/r_segs.c:174-191
            // killough 3/2/98:
            //
            // This calculation used to overflow and cause crashes in Doom:
            //
            // sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
            //
            // This code fixes it, by using double-precision intermediate
            // arithmetic and by skipping the drawing of 2s normals whose
            // mapping to screen coordinates is totally out of range:
            {
                const int64_t t = ((int64_t) centeryfrac << FRACBITS)
                                -  (int64_t) dc_texturemid * spryscale;

                if (t + (int64_t) textureheight[texnum] * spryscale < 0
                ||  t > (int64_t) SCREENHEIGHT << FRACBITS * 2)
                {
                    continue;
                }

                sprtopscreen = (int64_t)(t >> FRACBITS); // [crispy] 64-bit integer math
            }

            dc_iscale = UINT_MAX / (unsigned)spryscale;

            // draw the texture
            const column_t *const col = (column_t *)((byte *)R_GetColumnMod(texnum,maskedtexturecol[dc_x]) -3);

            R_DrawMaskedColumn (col, -1);
            maskedtexturecol[dc_x] = INT_MAX;  // [JN] 32-bit integer math
        }
    }
    
    colfunc = basecolfunc;  // [JN] Reset column drawing function
}

// -----------------------------------------------------------------------------
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
// -----------------------------------------------------------------------------

static boolean didsolidcol;  // True if at least one column was marked solid

static void R_RenderSegLoop (void)
{
    fixed_t texturecolumn = 0;  // [JN] Purely to shut up the compiler.

    for ( ; rw_x < rw_stopx ; rw_x++)
    {
        // mark floor / ceiling areas
        int yl = R_WallTopToScreen(topfrac);
        int yh = R_WallBottomToScreen(bottomfrac);

        // no space above wall?
        int bottom, top = ceilingclip[rw_x]+1;
      
        if (yl < top)
        {
            yl = top;
        }

        if (markceiling)
        {
            bottom = yl-1;

            if (bottom >= floorclip[rw_x])
            {
                bottom = floorclip[rw_x]-1;
            }
            if (top <= bottom)
            {
                ceilingplane->top[rw_x] = top;
                ceilingplane->bottom[rw_x] = bottom;
            }

            ceilingclip[rw_x] = bottom;
        }

        bottom = floorclip[rw_x]-1;

        if (yh > bottom)
        {
            yh = bottom;
        }

        if (markfloor)
        {
            top = yh < ceilingclip[rw_x] ? ceilingclip[rw_x] : yh;

            if (++top <= bottom)
            {
                floorplane->top[rw_x] = top;
                floorplane->bottom[rw_x] = bottom;
            }

            floorclip[rw_x] = top;
        }
        
        // texturecolumn and lighting are independent of wall tiers
        if (segtextured)
        {
            const fixed_t fracscale = R_WallScaleToFixed(rw_scale);

            // calculate texture offset
            // [PN] ASAN: Keep angle in finetangent array
            const angle_t angle = ((rw_centerangle + xtoviewangle[rw_x]) >> ANGLETOFINESHIFT) & 0xFFF;
            texturecolumn = rw_offset - FixedMul(finetangent[angle], rw_distance);
            texturecolumn >>= FRACBITS;

            // calculate lighting
            {
                int index = (fracscale / vid_resolution) >> LIGHTSCALESHIFT;

                if (index >= MAXLIGHTSCALE)
                {
                    index = MAXLIGHTSCALE-1;
                }

                // [crispy] optional brightmaps
                // [PN] Fast path keeps vanilla pointer when sector uses neutral bank 0.
                const lighttable_t *const base = walllights[index];
                dc_colormap[0] = frontsector->lightbank
                               ? R_ColLight_Apply(frontsector->lightbank, base)
                               : (lighttable_t *)base;
                dc_colormap[1] = (!fixedcolormap && vis_brightmaps) ? colormaps : dc_colormap[0];
            }

            dc_x = rw_x;
            dc_iscale = UINT_MAX / (unsigned)fracscale;
        }

        // draw the wall tiers
        if (midtexture)
        {
            // single sided line
            dc_yl = yl;
            dc_yh = yh;
            dc_texturemid = rw_midtexturemid;
            dc_source = R_GetColumn(midtexture, texturecolumn);
            dc_texheight = textureheight[midtexture] >> FRACBITS;
            dc_brightmap = texturebrightmap[midtexture];
            colfunc ();
            ceilingclip[rw_x] = viewheight;
            floorclip[rw_x] = -1;
        }
        else
        {
            // two sided line
            if (toptexture)
            {
                // top wall
                const int mid = MIN(R_WallBottomToScreen(pixhigh), floorclip[rw_x] - 1);
                pixhigh += pixhighstep;

                if (mid >= yl)
                {
                    dc_yl = yl;
                    dc_yh = mid;
                    dc_texturemid = rw_toptexturemid;
                    dc_source = R_GetColumn(toptexture,texturecolumn);
                    dc_texheight = textureheight[toptexture]>>FRACBITS;
                    dc_brightmap = texturebrightmap[toptexture];
                    colfunc ();
                    ceilingclip[rw_x] = mid;
                }
                else
                {
                    ceilingclip[rw_x] = yl-1;
                }
            }
            else
            {
                // no top wall
                if (markceiling)
                {
                    ceilingclip[rw_x] = yl-1;
                }
            }

            if (bottomtexture)
            {
                // bottom wall
                const int mid = MAX(R_WallTopToScreen(pixlow), ceilingclip[rw_x] + 1);
                pixlow += pixlowstep;

                if (mid <= yh)
                {
                    dc_yl = mid;
                    dc_yh = yh;
                    dc_texturemid = rw_bottomtexturemid;
                    dc_source = R_GetColumn(bottomtexture,texturecolumn);
                    dc_texheight = textureheight[bottomtexture]>>FRACBITS;
                    dc_brightmap = texturebrightmap[bottomtexture];
                    colfunc ();
                    floorclip[rw_x] = mid;
                }
                else
                {
                    floorclip[rw_x] = yh+1;
                }
            }
            else
            {
                // no bottom wall
                if (markfloor)
                {
                    floorclip[rw_x] = yh+1;
                }
            }

            // [JN] cph - if we completely blocked further sight through this column,
            // add this info to the solid columns array for r_bsp.c
            if ((markceiling || markfloor) && (floorclip[rw_x] <= ceilingclip[rw_x] + 1)) 
            {
                solidcol[rw_x] = 1; 
                didsolidcol = true;
            }

            if (maskedtexture)
            {
                // save texturecol for backdrawing of masked mid texture
                maskedtexturecol[rw_x] = texturecolumn;
            }
        }

        rw_scale += rw_scalestep;
        topfrac += topstep;
        bottomfrac += bottomstep;
    }
}

// -----------------------------------------------------------------------------
// R_ScaleFromGlobalAngle
// -----------------------------------------------------------------------------

static fixed_t R_ScaleFromGlobalAngle (angle_t visangle)
{
    const angle_t anglea = ANG90 + (visangle - viewangle);
    const angle_t angleb = ANG90 + (visangle - rw_normalangle);
    const fixed_t den = FixedMul(rw_distance, finesine[anglea >> ANGLETOFINESHIFT]);
    const fixed_t num = FixedMul(projection, finesine[angleb >> ANGLETOFINESHIFT]) << detailshift;

    return MAX(256, FixedDiv(num, den));
}

// -----------------------------------------------------------------------------
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
// -----------------------------------------------------------------------------

void R_StoreWallRange (int start, int stop)
{
    IDRender.numsegs++;

    // [crispy] remove MAXDRAWSEGS Vanilla limit
    if (ds_p == drawsegs+maxdrawsegs)
    {
        unsigned newmax = maxdrawsegs ? maxdrawsegs*2 : 128; // killough
        drawsegs = I_Realloc(drawsegs,newmax*sizeof(*drawsegs));
        ds_p = drawsegs+maxdrawsegs;
        maxdrawsegs = newmax;
    }

#ifdef RANGECHECK
    if (start >= viewwidth || start > stop)
        I_Error("Bad R_RenderWallRange: %i to %i", start , stop);
#endif

    sidedef = curline->sidedef;
    linedef = curline->linedef;

    // mark the segment as visible for auto map
    linedef->flags |= ML_MAPPED;

    // [crispy] (flags & ML_MAPPED) is all we need to know for automap
    if (automapactive && !automap_overlay)
    {
        return;
    }

    // calculate rw_distance for scale calculation
    rw_normalangle = curline->r_angle + ANG90; // [crispy] use recalculated angle

    // [crispy] fix long wall wobble
    // thank you very much Linguica, Andrey Budko and kb1
    // http://www.doomworld.com/vb/post/1340718
    // shift right to avoid possibility of int64 overflow in rw_distance calculation
    const uint32_t len = curline->length;
    const int64_t dx = ((int64_t)curline->v2->r_x - curline->v1->r_x) >> 1;
    const int64_t dy = ((int64_t)curline->v2->r_y - curline->v1->r_y) >> 1;
    const int64_t dx1 = ((int64_t)viewx - curline->v1->r_x) >> 1;
    const int64_t dy1 = ((int64_t)viewy - curline->v1->r_y) >> 1;
    const int64_t dist = ((dy * dx1 - dx * dy1) / len) << 1;
    rw_distance = (fixed_t)BETWEEN(INT_MIN, INT_MAX, dist);

    ds_p->x1 = rw_x = start;
    ds_p->x2 = stop;
    ds_p->curline = curline;
    rw_stopx = stop+1;

    // [JN] killough 1/6/98, 2/1/98: remove limit on openings
    {     
        const size_t pos = lastopening - openings;
        const size_t need = (rw_stopx - start)*sizeof(*lastopening) + pos;

        if (need > maxopenings)
        {
            drawseg_t *ds;                // jff 8/9/98 needed for fix from ZDoom
            const int *const oldopenings = openings;  // dropoff overflow
            const int *const oldlast = lastopening;   // dropoff overflow

            do
            {
                maxopenings = maxopenings ? maxopenings*2 : 16384;
            } while (need > maxopenings);

            openings = I_Realloc(openings, maxopenings * sizeof(*openings));
            lastopening = openings + pos;

            // jff 8/9/98 borrowed fix for openings from ZDOOM1.14
            // [RH] We also need to adjust the openings pointers that
            //    were already stored in drawsegs.
            for (ds = drawsegs; ds < ds_p; ds++)
            {
#define ADJUST(p) if (ds->p + ds->x1 >= oldopenings && ds->p + ds->x1 <= oldlast)\
            ds->p = ds->p - oldopenings + openings;
                ADJUST (maskedtexturecol);
                ADJUST (sprtopclip);
                ADJUST (sprbottomclip);
            }
#undef ADJUST
        }
    }

    // calculate scale at both ends and step
    const int steps = MAX(1, stop - start);

    ds_p->scale1 = R_ScaleFromGlobalAngle (viewangle + xtoviewangle[start]);
    ds_p->scale2 = (stop > start) ? R_ScaleFromGlobalAngle (viewangle + xtoviewangle[stop])
                                  : ds_p->scale1;
    rw_scale = R_ToWallScale(ds_p->scale1);
    ds_p->scalestep = rw_scalestep = R_ToWallScale(ds_p->scale2 - ds_p->scale1) / steps;

    // calculate texture boundaries
    //  and decide if floor / ceiling marks are needed
    worldtop = frontsector->interpceilingheight - viewz;
    worldbottom = frontsector->interpfloorheight - viewz;

    midtexture = toptexture = bottomtexture = maskedtexture = 0;
    ds_p->maskedtexturecol = NULL;

    if (!backsector)
    {
        // single sided line
        midtexture = texturetranslation[sidedef->midtexture];
        // a single sided line is terminal, so it must mark ends
        markfloor = markceiling = true;
        if (linedef->flags & ML_DONTPEGBOTTOM)
        {
            const fixed_t vtop = frontsector->interpfloorheight
                               + textureheight[sidedef->midtexture];
            // bottom of texture at bottom
            rw_midtexturemid = vtop - viewz;
        }
        else
        {
            // top of texture at top
            rw_midtexturemid = worldtop;
        }

        rw_midtexturemid += sidedef->rowoffset;

        ds_p->silhouette = SIL_BOTH;
        ds_p->sprtopclip = screenheightarray;
        ds_p->sprbottomclip = negonearray;
        ds_p->bsilheight = INT_MAX;
        ds_p->tsilheight = INT_MIN;
    }
    else
    {
        // two sided line
        ds_p->sprtopclip = ds_p->sprbottomclip = NULL;
        ds_p->silhouette = 0;

        // [JN] cph - closed 2S line e.g. door
        if (linedef->r_flags & RF_CLOSED)
        {
            // cph - killough's (outdated) comment follows - this deals with both 
            // "automap fixes", his and mine
            // killough 1/17/98: this test is required if the fix
            // for the automap bug (r_bsp.c) is used, or else some
            // sprites will be displayed behind closed doors. That
            // fix prevents lines behind closed doors with dropoffs
            // from being displayed on the automap.

            ds_p->silhouette = SIL_BOTH;
            ds_p->sprbottomclip = negonearray;
            ds_p->bsilheight = INT_MAX;
            ds_p->sprtopclip = screenheightarray;
            ds_p->tsilheight = INT_MIN;
        }
        else 
        {
            // [JN] Not solid - old code.
            if (frontsector->interpfloorheight > backsector->interpfloorheight)
            {
                ds_p->silhouette = SIL_BOTTOM;
                ds_p->bsilheight = frontsector->interpfloorheight;
            }
            else if (backsector->interpfloorheight > viewz)
            {
                ds_p->silhouette = SIL_BOTTOM;
                ds_p->bsilheight = INT_MAX;
            }

            if (frontsector->interpceilingheight < backsector->interpceilingheight)
            {
                ds_p->silhouette |= SIL_TOP;
                ds_p->tsilheight = frontsector->interpceilingheight;
            }
            else if (backsector->interpceilingheight < viewz)
            {
                ds_p->silhouette |= SIL_TOP;
                ds_p->tsilheight = INT_MIN;
    
            }
        }

        worldhigh = backsector->interpceilingheight - viewz;
        worldlow = backsector->interpfloorheight - viewz;

        // hack to allow height changes in outdoor areas
        if (frontsector->ceilingpic == skyflatnum && backsector->ceilingpic == skyflatnum)
        {
            worldtop = worldhigh;
        }

        if (worldlow != worldbottom 
        ||  backsector->floorpic != frontsector->floorpic
        ||  backsector->lightlevel != frontsector->lightlevel
        ||  backsector->lightbank != frontsector->lightbank
        ||  backsector->special != frontsector->special)
        {
            markfloor = true;
        }
        else
        {
            // same plane on both sides
            markfloor = false;
        }

        if (worldhigh != worldtop
        ||  backsector->ceilingpic != frontsector->ceilingpic
        ||  backsector->lightlevel != frontsector->lightlevel
        ||  backsector->lightbank != frontsector->lightbank)
        {
            markceiling = true;
        }
        else
        {
            // same plane on both sides
            markceiling = false;
        }

        if (backsector->interpceilingheight <= frontsector->interpfloorheight
        ||  backsector->interpfloorheight >= frontsector->interpceilingheight)
        {
            // closed door
            markceiling = markfloor = true;
        }

        if (worldhigh < worldtop)
        {
            // top texture
            toptexture = texturetranslation[sidedef->toptexture];
            if (linedef->flags & ML_DONTPEGTOP)
            {
                // top of texture at top
                rw_toptexturemid = worldtop;
            }
            else
            {
                const fixed_t vtop = backsector->interpceilingheight
                                   + textureheight[sidedef->toptexture];

                // bottom of texture
                rw_toptexturemid = vtop - viewz;	
            }
        }
        if (worldlow > worldbottom)
        {
            // bottom texture
            bottomtexture = texturetranslation[sidedef->bottomtexture];

            if (linedef->flags & ML_DONTPEGBOTTOM )
            {
                // bottom of texture at bottom
                // top of texture at top
                rw_bottomtexturemid = worldtop;
            }
            else    // top of texture at top
            rw_bottomtexturemid = worldlow;
        }

        rw_toptexturemid += sidedef->rowoffset;
        rw_bottomtexturemid += sidedef->rowoffset;

        // allocate space for masked texture tables
        if (sidedef->midtexture)
        {
            // masked midtexture
            maskedtexture = true;
            ds_p->maskedtexturecol = maskedtexturecol = lastopening - rw_x;
            lastopening += rw_stopx - rw_x;
        }
    }

    // calculate rw_offset (only needed for textured lines)
    segtextured = midtexture | toptexture | bottomtexture | maskedtexture;

    if (segtextured)
    {
        // [crispy] fix long wall wobble
        rw_offset = (fixed_t)(((dx*dx1 + dy*dy1) / len) << 1);
        rw_offset += sidedef->textureoffset + curline->offset;
        rw_centerangle = ANG90 + viewangle - rw_normalangle;

        // calculate light table use different light tables
        //  for horizontal / vertical / diagonal
        // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
        if (!fixedcolormap)
        {
            // [crispy] smooth diminishing lighting and smoother fake contrast
            const int lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT)
                               + (extralight * LIGHTBRIGHT)
                               + curline->fakecontrast;

            walllights = scalelight[BETWEEN(0, LIGHTLEVELS - 1, lightnum)];
        }
    }

    // if a floor / ceiling plane is on the wrong side
    //  of the view plane, it is definitely invisible
    //  and doesn't need to be marked.

    if (frontsector->interpfloorheight >= viewz)
    {
        // above view plane
        markfloor = false;
    }

    if (frontsector->interpceilingheight <= viewz 
    &&  frontsector->ceilingpic != skyflatnum)
    {
        // below view plane
        markceiling = false;
    }

    // calculate incremental stepping values for texture edges
    const int64_t screenmid = (int64_t)centery * WALLEDGEUNIT;
    const int64_t scaledelta = (int64_t)ds_p->scale1 - ds_p->scale2;

    topfrac = screenmid - (int64_t)worldtop * ds_p->scale1;
    topstep = (int64_t)worldtop * scaledelta / steps;

    bottomfrac = screenmid - (int64_t)worldbottom * ds_p->scale1;
    bottomstep = (int64_t)worldbottom * scaledelta / steps;

    if (backsector)
    {
        if (worldhigh < worldtop)
        {
            pixhigh = screenmid - (int64_t)worldhigh * ds_p->scale1;
            pixhighstep = (int64_t)worldhigh * scaledelta / steps;
        }

        if (worldlow > worldbottom)
        {
            pixlow = screenmid - (int64_t)worldlow * ds_p->scale1;
            pixlowstep = (int64_t)worldlow * scaledelta / steps;
        }
    }

    // render it
    if (markceiling)
    {
        if (ceilingplane)  // [JN] killough 4/11/98: add NULL ptr checks
        {
            ceilingplane = R_CheckPlane (ceilingplane, rw_x, rw_stopx-1);
        }
        else
        {
            markceiling = 0;
        }
    }

    if (markfloor)
    {
        if (floorplane)  // [JN] killough 4/11/98: add NULL ptr checks
        // [JN] cph 2003/04/18  - ceilingplane and floorplane might be the same
        // visplane (e.g. if both skies); R_CheckPlane doesn't know about
        // modifications to the plane that might happen in parallel with the check
        // being made, so we have to override it and split them anyway if that is
        // a possibility, otherwise the floor marking would overwrite the ceiling
        // marking, resulting in HOM.
        if (markceiling && ceilingplane == floorplane)
        {
            floorplane = R_DupPlane (floorplane, rw_x, rw_stopx-1);
        }
        else
        {
            floorplane = R_CheckPlane (floorplane, rw_x, rw_stopx-1);
        }
        else
        {
            markfloor = 0;
        }
    }

    didsolidcol = false;
    R_RenderSegLoop ();

    // [JN] cph - if a column was made solid by this wall,
    // we _must_ save full clipping info.
    if (backsector && didsolidcol)
    {
        if (!(ds_p->silhouette & SIL_BOTTOM))
        {
            ds_p->silhouette |= SIL_BOTTOM;
            ds_p->bsilheight = backsector->floorheight;
        }
        if (!(ds_p->silhouette & SIL_TOP))
        {
            ds_p->silhouette |= SIL_TOP;
            ds_p->tsilheight = backsector->ceilingheight;
        }
    }

    // save sprite clipping info
    if ( ((ds_p->silhouette & SIL_TOP) || maskedtexture) && !ds_p->sprtopclip)
    {
        memcpy (lastopening, ceilingclip+start, sizeof(*lastopening)*(rw_stopx-start));
        ds_p->sprtopclip = lastopening - start;
        lastopening += rw_stopx - start;
    }

    if ( ((ds_p->silhouette & SIL_BOTTOM) || maskedtexture) && !ds_p->sprbottomclip)
    {
        memcpy (lastopening, floorclip+start, sizeof(*lastopening)*(rw_stopx-start));
        ds_p->sprbottomclip = lastopening - start;
        lastopening += rw_stopx - start;
    }

    if (maskedtexture && !(ds_p->silhouette&SIL_TOP))
    {
        ds_p->silhouette |= SIL_TOP;
        ds_p->tsilheight = INT_MIN;
    }
    if (maskedtexture && !(ds_p->silhouette&SIL_BOTTOM))
    {
        ds_p->silhouette |= SIL_BOTTOM;
        ds_p->bsilheight = INT_MAX;
    }
    ds_p++;
}
