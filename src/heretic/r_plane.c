//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2011-2017 RestlessRodent
// Copyright(C) 2018-2023 Julia Nechaevskaya
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
// R_planes.c

#include <stdlib.h>
#include "doomdef.h"
#include "deh_str.h"
#include "i_system.h"
#include "m_misc.h"
#include "r_local.h"

#include "id_vars.h"


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

static fixed_t xsmoothscrolloffset; // [crispy]
static fixed_t ysmoothscrolloffset; // [crispy]

//
// sky mapping
//
int skyflatnum;
int skytexture;
int skytexturemid;
fixed_t skyiscale;


/*
================
=
= R_InitSkyMap
=
= Called whenever the view size changes
=
================
*/

void R_InitSkyMap(void)
{
    skyflatnum = R_FlatNumForName(DEH_String("F_SKY1"));
    skytexturemid = 200 * FRACUNIT;
    skyiscale = FRACUNIT >> vid_hires;
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
    unsigned	index;
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

     // [crispy] add smooth scroll offsets
    ds_xfrac += xsmoothscrolloffset;
    ds_yfrac += ysmoothscrolloffset;

    if (fixedcolormap)
	ds_colormap[0] = ds_colormap[1] = fixedcolormap;
    else
    {
	index = distance >> LIGHTZSHIFT;
	
	if (index >= MAXLIGHTZ )
	    index = MAXLIGHTZ-1;

	ds_colormap[0] = planezlight[index];
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
  int		lightlevel,
  int		special)
{
    visplane_t *check;
    unsigned int hash;

    if (picnum == skyflatnum)
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
        && lightlevel == check->lightlevel && special == check->special)
            return check;

    check = new_visplane(hash);

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->special = special;
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
    new_pl->special = pl->special;
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
    int light;
    int x;
    int lumpnum;
    int angle;
    byte *tempSource;

    pixel_t *dest;
    int count;
    fixed_t frac, fracstep;
    static int interpfactor; // [crispy]

    extern int columnofs[MAXWIDTH];

    for (int i = 0 ; i < MAXVISPLANES ; i++)
    for (visplane_t *pl = visplanes[i] ; pl ; pl = pl->next)
    if (pl->minx <= pl->maxx)
    {
        //
        // sky flat
        //
        if (pl->picnum == skyflatnum)
        {
            dc_iscale = skyiscale;
            // [crispy] no brightmaps for sky
            // [JN] Invulnerability affects sky feature.
            if (vis_invul_sky
            && (players[displayplayer].powers[pw_invulnerability] > BLINKTHRESHOLD
            || (players[displayplayer].powers[pw_invulnerability] & 8)))
            {
                dc_colormap[0] = dc_colormap[1] = fixedcolormap;
            }
            else
            {
                // sky is allways drawn full bright
                dc_colormap[0] = dc_colormap[1] = colormaps;
            }
            dc_texturemid = skytexturemid;
            dc_texheight = textureheight[skytexture]>>FRACBITS;
            for (x = pl->minx; x <= pl->maxx; x++)
            {
                dc_yl = pl->top[x];
                dc_yh = pl->bottom[x];
                if ((unsigned) dc_yl <= dc_yh)  // [JN] 32-bit integer math
                {
                    angle = (viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT;
                    dc_x = x;
                    dc_source = R_GetColumn(skytexture, angle);

                    count = dc_yh - dc_yl;
                    if (count < 0)
                        return;

#ifdef RANGECHECK
                    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0
                        || dc_yh >= SCREENHEIGHT)
                        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh,
                                dc_x);
#endif

                    dest = ylookup[dc_yl] + columnofs[dc_x];

                    fracstep = dc_iscale;
                    frac = dc_texturemid + (dc_yl - centery) * fracstep;
                    do
                    {
                        const byte source = dc_source[frac >> FRACBITS];
                        *dest = dc_colormap[dc_brightmap[source]][source];

                        dest += SCREENWIDTH;
                        frac += fracstep;
                    }
                    while (count--);

//                                      colfunc ();
                }
            }
        }
        else
        {
            const int stop = pl->maxx + 1;
            
        //
        // regular flat
        //
        lumpnum = firstflat + flattranslation[pl->picnum];

        tempSource = W_CacheLumpNum(lumpnum, PU_STATIC);
        ds_brightmap = R_BrightmapForFlatNum(lumpnum-firstflat);

        // [crispy] Use old value of interpfactor if uncapped and paused. This
        // ensures that scrolling stops smoothly when pausing.
        if (vid_uncapped_fps && realleveltime > oldleveltime)
        {
            // [crispy] Scrolling normally advances every *other* gametic, so
            // interpolation needs to span two tics
            if (leveltime & 1)
            {
                interpfactor = (FRACUNIT + fractionaltic) >> 1;
            }
            else
            {
                interpfactor = fractionaltic >> 1;
            }
        }
        else if (!vid_uncapped_fps)
        {
            interpfactor = 0;
        }

        switch (pl->special)
        {
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:           // Scroll_North
                xsmoothscrolloffset = 0;
                ysmoothscrolloffset = 0;
                ds_source = tempSource;
                break;
            case 20:
            case 21:
            case 22:
            case 23:
            case 24:           // Scroll_East
                xsmoothscrolloffset = -(interpfactor << (pl->special - 20));
                ysmoothscrolloffset = 0;
                ds_source = tempSource + ((63 - ((leveltime >> 1) & 63)) <<
                                          (pl->special - 20) & 63);
                //ds_source = tempSource+((leveltime>>1)&63);
                break;
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:           // Scroll_South
                xsmoothscrolloffset = 0;
                ysmoothscrolloffset = 0;
                ds_source = tempSource;
                break;
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:           // Scroll_West
                xsmoothscrolloffset = 0;
                ysmoothscrolloffset = 0;
                ds_source = tempSource;
                break;
            case 4:            // Scroll_EastLavaDamage
                xsmoothscrolloffset = -(interpfactor << 3);
                ysmoothscrolloffset = 0;
                ds_source =
                    tempSource + (((63 - ((leveltime >> 1) & 63)) << 3) & 63);
                break;
            default:
                xsmoothscrolloffset = 0;
                ysmoothscrolloffset = 0;
                ds_source = tempSource;
        }
        planeheight = abs(pl->height - viewz);
        light = (pl->lightlevel >> LIGHTSEGSHIFT) + extralight;
        if (light >= LIGHTLEVELS)
            light = LIGHTLEVELS - 1;
        if (light < 0)
            light = 0;
        planezlight = zlight[light];

        pl->top[pl->minx-1] = pl->top[stop] = USHRT_MAX;

        for (x = pl->minx; x <= stop; x++)
            R_MakeSpans(x, pl->top[x - 1], pl->bottom[x - 1], pl->top[x],
                        pl->bottom[x]);

        W_ReleaseLumpNum(lumpnum);
        }
    }
}
