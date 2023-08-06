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
//	Here is a core component: drawing the floors and ceilings,
//	 while maintaining a per column clipping list only.
//	Moreover, the sky areas have to be determined.
//


#include <stdio.h>
#include <stdlib.h>
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"
#include "doomstat.h"
#include "p_local.h"
#include "r_local.h"
#include "m_misc.h"

#include "id_vars.h"
#include "id_func.h"


// -----------------------------------------------------------------------------
// MAXVISPLANES is no longer a limit on the number of visplanes,
// but a limit on the number of hash slots; larger numbers mean
// better performance usually but after a point they are wasted,
// and memory and time overheads creep in.
//
// Lee Killough
// -----------------------------------------------------------------------------

#define MAXVISPLANES	128                  // must be a power of 2

static visplane_t *visplanes[MAXVISPLANES];  // [JN] killough
static visplane_t *freetail;                 // [JN] killough
static visplane_t **freehead = &freetail;    // [JN] killough
visplane_t *floorplane, *ceilingplane;

// [JN] killough -- hash function for visplanes
// Empirically verified to be fairly uniform:

#define visplane_hash(picnum, lightlevel, height) \
    ((unsigned)((picnum) * 3 + (lightlevel) + (height) * 7) & (MAXVISPLANES - 1))

// [JN] killough 8/1/98: set static number of openings to be large enough
// (a static limit is okay in this case and avoids difficulties in r_segs.c)

size_t  maxopenings;
int    *openings;     // [JN] 32-bit integer math
int    *lastopening;  // [JN] 32-bit integer math


//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
int  floorclip[MAXWIDTH];    // [JN] 32-bit integer math
int  ceilingclip[MAXWIDTH];  // [JN] 32-bit integer math

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
int			spanstart[MAXHEIGHT];
int			spanstop[MAXHEIGHT];

//
// texture mapping
//
lighttable_t**		planezlight;
fixed_t			planeheight;

fixed_t*			yslope;
fixed_t			yslopes[LOOKDIRS][MAXHEIGHT];
fixed_t			distscale[MAXWIDTH];

fixed_t			cachedheight[MAXHEIGHT];
fixed_t			cacheddistance[MAXHEIGHT];
fixed_t			cachedxstep[MAXHEIGHT];
fixed_t			cachedystep[MAXHEIGHT];


//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes (void)
{
  // Doh!
}


//
// R_MapPlane
//
// Uses global vars:
//  planeheight
//  ds_source
//  viewx
//  viewy
//
// BASIC PRIMITIVE
//
void
R_MapPlane
( int		y,
  int		x1,
  int		x2)
{
// [crispy] see below
//  angle_t	angle;
    fixed_t	distance;
//  fixed_t	length;
//  unsigned	index;
    int dx, dy;
	
#ifdef RANGECHECK
    if (x2 < x1
     || x1 < 0
     || x2 >= viewwidth
     || y > viewheight)
    {
	I_Error ("R_MapPlane: %i, %i at %i",x1,x2,y);
    }
#endif

    // [crispy] visplanes with the same flats now match up far better than before
    // adapted from prboom-plus/src/r_plane.c:191-239, translated to fixed-point math
    //
    // SoM: because centery is an actual row of pixels (and it isn't really the
    // center row because there are an even number of rows) some corrections need
    // to be made depending on where the row lies relative to the centery row.

    if (centery == y)
    {
	return;
    }

    dy = (abs(centery - y) << FRACBITS) + (y < centery ? -FRACUNIT : FRACUNIT) / 2;

    if (planeheight != cachedheight[y])
    {
	cachedheight[y] = planeheight;
	distance = cacheddistance[y] = FixedMul (planeheight, yslope[y]);
	// [FG] avoid right-shifting in FixedMul() followed by left-shifting in FixedDiv()
	ds_xstep = cachedxstep[y] = (fixed_t)((int64_t)viewsin * planeheight / dy) << detailshift;
	ds_ystep = cachedystep[y] = (fixed_t)((int64_t)viewcos * planeheight / dy) << detailshift;
    }
    else
    {
	distance = cacheddistance[y];
	ds_xstep = cachedxstep[y];
	ds_ystep = cachedystep[y];
    }

    dx = x1 - centerx;

    ds_xfrac = viewx + FixedMul(viewcos, distance) + dx * ds_xstep;
    ds_yfrac = -viewy - FixedMul(viewsin, distance) + dx * ds_ystep;

    if (fixedcolormap)
	ds_colormap[0] = ds_colormap[1] = fixedcolormap;
    else
    {
        ds_colormap[0] = planezlight[BETWEEN(0, distance >> LIGHTZSHIFT, MAXLIGHTZ - 1)];
        ds_colormap[1] = colormaps;
    }
	
    ds_y = y;
    ds_x1 = x1;
    ds_x2 = x2;

    // high or low detail
    spanfunc ();	
}


//
// R_ClearPlanes
// At begining of frame.
//
void R_ClearPlanes (void)
{
    int i;

    // opening / clipping determination
    for (i = 0 ; i < viewwidth ; i++)
    {
        floorclip[i] = viewheight;
        ceilingclip[i] = -1;
    }

    for (i = 0; i < MAXVISPLANES; i++)  // [JN] new code -- killough
        for (*freehead = visplanes[i], visplanes[i] = NULL ; *freehead ; )
            freehead = &(*freehead)->next;

    lastopening = openings;

    // texture calculation
    memset(cachedheight, 0, sizeof(cachedheight));
}

// -----------------------------------------------------------------------------
// [crispy] remove MAXVISPLANES Vanilla limit
// New function, by Lee Killough
// -----------------------------------------------------------------------------

static visplane_t *new_visplane (unsigned const int hash)
{
    visplane_t *check = freetail;

    if (!check)
    {
        check = calloc(1, sizeof(*check));
    }
    else if (!(freetail = freetail->next))
    {
        freehead = &freetail;
    }

    check->next = visplanes[hash];
    visplanes[hash] = check;

    return check;
}


//
// R_FindPlane
//
visplane_t*
R_FindPlane
( fixed_t	height,
  int		picnum,
  int		lightlevel)
{
    visplane_t *check;
    unsigned int hash;

    if (picnum == skyflatnum || picnum & PL_SKYFLAT)  // killough 10/98
    {
        lightlevel = 0;   // killough 7/19/98: most skies map together

        // haleyjd 05/06/08: but not all. If height > viewpoint.z, set height to 1
        // instead of 0, to keep ceilings mapping with ceilings, and floors mapping
        // with floors.
        if (height > viewz)
        {
            height = 1;
        }
        else
        {
            height = 0;
        }
    }

    // New visplane algorithm uses hash table -- killough
    hash = visplane_hash(picnum, lightlevel, height);

    for (check = visplanes[hash]; check; check = check->next)
        if (height == check->height && picnum == check->picnum 
        && lightlevel == check->lightlevel)
            return check;

    check = new_visplane(hash);

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->minx = SCREENWIDTH;
    check->maxx = -1;

    memset(check->top, UCHAR_MAX, viewwidth * sizeof(*check->top));

    return check;
}

// -----------------------------------------------------------------------------
// R_DupPlane
// -----------------------------------------------------------------------------

visplane_t *R_DupPlane(const visplane_t *pl, int start, int stop)
{
    visplane_t  *new_pl = new_visplane(visplane_hash(pl->picnum, pl->lightlevel, pl->height));

    new_pl->height = pl->height;
    new_pl->picnum = pl->picnum;
    new_pl->lightlevel = pl->lightlevel;
    new_pl->minx = start;
    new_pl->maxx = stop;

    memset(new_pl->top, UCHAR_MAX, viewwidth * sizeof(*new_pl->top));

    return new_pl;
}

// -----------------------------------------------------------------------------
// R_CheckPlane
// -----------------------------------------------------------------------------

visplane_t*
R_CheckPlane
( visplane_t*	pl,
  int		start,
  int		stop)
{
    int intrl, intrh, unionl, unionh, x;

    if (start < pl->minx)
    {
        intrl = pl->minx, unionl = start;
    }
    else
    {
        unionl = pl->minx, intrl = start;
    }

    if (stop  > pl->maxx)
    {
        intrh = pl->maxx, unionh = stop;
    }
    else
    {
        unionh = pl->maxx, intrh  = stop;
    }

    for (x=intrl ; x <= intrh && pl->top[x] == USHRT_MAX; x++);
    if (x > intrh)
    {
        // Can use existing plane; extend range
        pl->minx = unionl, pl->maxx = unionh;
        return pl;
    }
    else
    {
        // Cannot use existing plane; create a new one
        return R_DupPlane(pl, start, stop);
    }
}


//
// R_MakeSpans
//
void
R_MakeSpans
( unsigned int		x,   // [JN] 32-bit integer math
  unsigned int		t1,  // [JN] 32-bit integer math
  unsigned int		b1,  // [JN] 32-bit integer math
  unsigned int		t2,  // [JN] 32-bit integer math
  unsigned int		b2)  // [JN] 32-bit integer math
{
    for ( ; t1 < t2 && t1 <= b1 ; t1++)
    {
        R_MapPlane(t1, spanstart[t1], x-1);
    }
    for ( ; b1 > b2 && b1 >= t1 ; b1--)
    {
        R_MapPlane(b1, spanstart[b1], x-1);
    }
    while (t2 < t1 && t2 <= b2)
    {
        spanstart[t2++] = x;
    }
    while (b2 > b1 && b2 >= t2)
    {
        spanstart[b2--] = x;
    }
}



//
// R_DrawPlanes
// At the end of each frame.
//
void R_DrawPlanes (void)
{
    // [JN] CRL - openings counter.
    IDRender.numopenings = lastopening - openings;

    for (int i = 0 ; i < MAXVISPLANES ; i++)
    for (visplane_t *pl = visplanes[i] ; pl ; pl = pl->next, IDRender.numplanes++)
    if (pl->minx <= pl->maxx)
    {
        // sky flat
        // [crispy] add support for MBF sky tranfers
        if (pl->picnum == skyflatnum || pl->picnum & PL_SKYFLAT)
        {
            int texture;
            angle_t an = viewangle, flip;
            if (pl->picnum & PL_SKYFLAT)
            {
                const line_t *l = &lines[pl->picnum & ~PL_SKYFLAT];
                const side_t *s = *l->sidenum + sides;
                texture = texturetranslation[s->toptexture];
                dc_texturemid = s->rowoffset - 28*FRACUNIT;
                // [crispy] stretch sky
                if (mouse_look)
                {
                    dc_texturemid = dc_texturemid * (textureheight[texture]>>FRACBITS) / SKYSTRETCH_HEIGHT;
                }
                flip = (l->special == 272) ? 0u : ~0u;
                an += s->textureoffset;
            }
            else
            {
                texture = skytexture;
                dc_texturemid = skytexturemid;
                flip = 0;
            }
            dc_iscale = pspriteiscale >> detailshift;
            
            // Sky is allways drawn full bright, i.e. colormaps[0] is used.
            // Because of this hack, sky is not affected by INVUL inverse mapping.
            // [JN] Make optional, "Invulnerability affects sky" feature.
            // [crispy] no brightmaps for sky
            dc_colormap[0] = dc_colormap[1] = vis_invul_sky && fixedcolormap ? 
                                              fixedcolormap : colormaps;

            //dc_texturemid = skytexturemid;
            dc_texheight = textureheight[texture]>>FRACBITS;
            // [crispy] stretch sky
            if (mouse_look)
	        dc_iscale = dc_iscale * dc_texheight / SKYSTRETCH_HEIGHT;
            for (int x = pl->minx ; x <= pl->maxx ; x++)
            {
                if ((dc_yl = pl->top[x]) != USHRT_MAX && dc_yl <= (dc_yh = pl->bottom[x]))
                {
                    // [crispy] Optionally draw skies horizontally linear.
                    const int angle = ((an + (vis_linear_sky ?
                                        linearskyangle[x] : xtoviewangle[x]))^flip)>>ANGLETOSKYSHIFT;
                    dc_x = x;
                    dc_source = R_GetColumn(texture, angle);
                    colfunc ();
                }
            }
        }
        else  // regular flat
        {
            int light = (pl->lightlevel >> LIGHTSEGSHIFT) + extralight;
            const boolean swirling = (flattranslation[pl->picnum] == -1);
            const int stop = pl->maxx + 1;
            const int lumpnum = firstflat + (swirling ? pl->picnum : flattranslation[pl->picnum]);

            // [crispy] add support for SMMU swirling flats
            ds_source = swirling ? R_DistortedFlat(lumpnum) : W_CacheLumpNum(lumpnum, PU_STATIC);
            ds_brightmap = R_BrightmapForFlatNum(lumpnum-firstflat);

            planeheight = abs(pl->height-viewz);
            if (light >= LIGHTLEVELS)
            {
                light = LIGHTLEVELS-1;
            }
            if (light < 0)
            {
                light = 0;
            }
            planezlight = zlight[light];
            pl->top[pl->minx-1] = pl->top[stop] = USHRT_MAX;

            for (int x = pl->minx ; x <= stop ; x++)
            {
                R_MakeSpans(x,pl->top[x-1], pl->bottom[x-1], pl->top[x], pl->bottom[x]);
            }
        }
    }
}