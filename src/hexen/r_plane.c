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

#include "h2def.h"
#include "i_system.h"
#include "r_local.h"
#include "p_spec.h"

#include "id_vars.h"
#include "id_func.h"


// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int Sky1Texture;
int Sky2Texture;
fixed_t Sky1ColumnOffset;
fixed_t Sky2ColumnOffset;
int skyflatnum;
int skytexturemid;
fixed_t skyiscale;
boolean DoubleSky;
planefunction_t floorfunc, ceilingfunc;

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


// Clip values are the solid pixel bounding the range.
// floorclip start out SCREENHEIGHT
// ceilingclip starts out -1
int floorclip[MAXWIDTH]; // [crispy] 32-bit integer math
int ceilingclip[MAXWIDTH]; // [crispy] 32-bit integer math

// spanstart holds the start of a plane span, initialized to 0
int spanstart[MAXHEIGHT];
int spanstop[MAXHEIGHT];

// Texture mapping
lighttable_t **planezlight;
fixed_t planeheight;
fixed_t *yslope;
fixed_t yslopes[LOOKDIRS][MAXHEIGHT]; // [crispy]
fixed_t distscale[MAXWIDTH];
fixed_t basexscale, baseyscale;
fixed_t cachedheight[MAXHEIGHT];
fixed_t cacheddistance[MAXHEIGHT];
fixed_t cachedxstep[MAXHEIGHT];
fixed_t cachedystep[MAXHEIGHT];

// [JN] Flowing effect for swirling liquids.
// Render-only coords:
static fixed_t swirlFlow_x;
static fixed_t swirlFlow_y;
// Actual coords, updates on game tic via P_AnimateSurfaces:
fixed_t swirlCoord_x;
fixed_t swirlCoord_y;

// PRIVATE DATA DEFINITIONS ------------------------------------------------
static fixed_t xsmoothscrolloffset; // [crispy]
static fixed_t ysmoothscrolloffset; // [crispy]

// CODE --------------------------------------------------------------------

//==========================================================================
//
// R_InitSky
//
// Called at level load.
//
//==========================================================================

void R_InitSky(int map)
{
    Sky1Texture = P_GetMapSky1Texture(map);
    Sky2Texture = P_GetMapSky2Texture(map);
    Sky1ScrollDelta = P_GetMapSky1ScrollDelta(map);
    Sky2ScrollDelta = P_GetMapSky2ScrollDelta(map);
    Sky1ColumnOffset = 0;
    Sky2ColumnOffset = 0;
    DoubleSky = P_GetMapDoubleSky(map);
}

//==========================================================================
//
// R_InitSkyMap
//
// Called whenever the view size changes.
//
//==========================================================================

void R_InitSkyMap(void)
{
    skyflatnum = R_FlatNumForName("F_SKY");
    skytexturemid = 200 * FRACUNIT;
    skyiscale = FRACUNIT;
}

//==========================================================================
//
// R_InitPlanes
//
// Called at game startup.
//
//==========================================================================

void R_InitPlanes(void)
{
}

//==========================================================================
//
// R_MapPlane
//
// Globals used: planeheight, ds_source, basexscale, baseyscale,
// viewx, viewy.
//
//==========================================================================

void R_MapPlane(int y, int x1, int x2)
{
    fixed_t distance;
    int dx, dy;

#ifdef RANGECHECK
    if (x2 < x1 || x1 < 0 || x2 >= viewwidth || (unsigned) y > viewheight)
    {
        I_Error("R_MapPlane: %i, %i at %i", x1, x2, y);
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
        distance = cacheddistance[y] = FixedMul(planeheight, yslope[y]);
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

     // [crispy]
    ds_xfrac += xsmoothscrolloffset;
    ds_yfrac += ysmoothscrolloffset;

    // [JN] Add flowing offsets.
    ds_xfrac += swirlFlow_x;
    ds_yfrac += swirlFlow_y;

    if (fixedcolormap)
    {
        ds_colormap = fixedcolormap;
    }
    else
    {
        unsigned int index = distance >> LIGHTZSHIFT;
        if (index >= MAXLIGHTZ)
        {
            index = MAXLIGHTZ - 1;
        }
        ds_colormap = planezlight[index];
    }

    ds_y = y;
    ds_x1 = x1;
    ds_x2 = x2;

    spanfunc();                 // High or low detail
}

//==========================================================================
//
// R_ClearPlanes
//
// Called at the beginning of each frame.
//
//==========================================================================

void R_ClearPlanes(void)
{
    int i;

    // opening / clipping determination
    for (i = 0 ; i < viewwidth ; i++)
    {
        floorclip[i] = viewheight;
        ceilingclip[i] = -1;
    }

    // [PN] Optimize loop by avoiding unnecessary assignments and checks.
    // Only process non-null visplanes and simplify inner loop performance.
    for (i = 0; i < MAXVISPLANES; i++)
    {
        if (visplanes[i] != NULL)
        {
            *freehead = visplanes[i];
            visplanes[i] = NULL;
            while (*freehead)
            {
                freehead = &(*freehead)->next;
            }
        }
    }

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

// -----------------------------------------------------------------------------
// R_FindPlane
// -----------------------------------------------------------------------------

visplane_t *R_FindPlane(fixed_t height, int picnum,
                        int lightlevel, int special)
{
    visplane_t *check;
    unsigned int hash;

    if (special < 150)
    {                           // Don't let low specials affect search
        special = 0;
    }

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

visplane_t *R_CheckPlane(visplane_t * pl, int start, int stop)
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

//==========================================================================
//
// R_MakeSpans
//
//==========================================================================

// [crispy] 32-bit integer math
void R_MakeSpans(int x, unsigned int t1, unsigned int b1, unsigned int t2, unsigned int b2)
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

//==========================================================================
//
// R_DrawPlanes
//
//==========================================================================

#define SKYTEXTUREMIDSHIFTED 200
#define FLATSCROLL(X) \
    ((interpfactor << (X)) - (((63 - ((leveltime >> 1) & 63)) << (X) & 63) * FRACUNIT))

void R_DrawPlanes(void)
{
    int x;
    byte *source;
    byte *source2;
    pixel_t *dest;
    int count;
    int offset;
    int skyTexture;
    int offset2;
    int skyTexture2;
    int frac;
    int fracstep = FRACUNIT / vid_resolution;
    static int interpfactor; // [crispy]
    int heightmask; // [crispy]
    static int prev_skyTexture, prev_skyTexture2, skyheight; // [crispy]
    int smoothDelta1 = 0, smoothDelta2 = 0; // [JN] Smooth sky scrolling.

    IDRender.numopenings = lastopening - openings;

    for (int i = 0 ; i < MAXVISPLANES ; i++)
    for (visplane_t *pl = visplanes[i] ; pl ; pl = pl->next, IDRender.numplanes++)
    if (pl->minx <= pl->maxx)
    {
        if (pl->picnum == skyflatnum)
        {                       // Sky flat
            if (DoubleSky)
            {                   // Render 2 layers, sky 1 in front
                skyTexture = texturetranslation[Sky1Texture];
                skyTexture2 = texturetranslation[Sky2Texture];

                if (vid_uncapped_fps)
                {
                    offset = 0;
                    offset2 = 0;
                    smoothDelta1 = Sky1ColumnOffset << 6;
                    smoothDelta2 = Sky2ColumnOffset << 6;
                }
                else
                {
                    offset = Sky1ColumnOffset >> 16;
                    offset2 = Sky2ColumnOffset >> 16;
                }
                
                if (skyTexture != prev_skyTexture ||
                    skyTexture2 != prev_skyTexture2)
                {
                    skyheight = MIN(R_GetPatchHeight(skyTexture, 0),
                                    R_GetPatchHeight(skyTexture2, 0));
                    prev_skyTexture = skyTexture;
                    prev_skyTexture2 = skyTexture2;
                }

                for (x = pl->minx; x <= pl->maxx; x++)
                {
                    dc_yl = pl->top[x];
                    dc_yh = pl->bottom[x];
                    if ((unsigned) dc_yl <= dc_yh) // [crispy] 32-bit integer math
                    {
                        // [crispy] Optionally draw skies horizontally linear.
                        const int angle = ((viewangle + smoothDelta1 + (vis_linear_sky ? 
                                        linearskyangle[x] : xtoviewangle[x])) ^ gp_flip_levels) >> ANGLETOSKYSHIFT;
                        const int angle2 = ((viewangle + smoothDelta2 + (vis_linear_sky ? 
                                         linearskyangle[x] : xtoviewangle[x])) ^ gp_flip_levels) >> ANGLETOSKYSHIFT;

                        count = dc_yh - dc_yl;
                        if (count < 0)
                        {
                            return;
                        }
                        source = R_GetColumn(skyTexture, angle + offset);
                        source2 = R_GetColumn(skyTexture2, angle2 + offset2);
                        dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
                        frac = SKYTEXTUREMIDSHIFTED * FRACUNIT + (dc_yl - centery) * fracstep;

                        // not a power of 2 -- killough
                        if (skyheight & (skyheight - 1))
                        {
                            heightmask = skyheight << FRACBITS;

                            if (frac < 0)
                                while ((frac += heightmask) < 0);
                            else
                                while (frac >= heightmask)
                                    frac -= heightmask;

                            do
                            {
                                if (source[frac >> FRACBITS])
                                {
#ifndef CRISPY_TRUECOLOR
                                    *dest = source[frac >> FRACBITS];
#else
                                    *dest = pal_color[source[frac >> FRACBITS]];
#endif
                                }
                                else
                                {
#ifndef CRISPY_TRUECOLOR
                                    *dest = source2[frac >> FRACBITS];
#else
                                    *dest = pal_color[source2[frac >> FRACBITS]];
#endif
                                }
                                dest += SCREENWIDTH;
                                if ((frac += fracstep) >= heightmask)
                                {
                                    frac -= heightmask;
                                }
                            } while (count--);
                        }
                        // texture height is a power of 2 -- killough
                        else
                        {
                            heightmask = skyheight - 1;

                            do
                            {
                                if (source[(frac >> FRACBITS) & heightmask])
                                {
#ifndef CRISPY_TRUECOLOR
                                    *dest = source[(frac >> FRACBITS) & heightmask];
#else
                                    *dest = pal_color[source[(frac >> FRACBITS) & heightmask]];
#endif
                                }
                                else
                                {
#ifndef CRISPY_TRUECOLOR
                                    *dest = source2[(frac >> FRACBITS) & heightmask];
#else
                                    *dest = pal_color[source2[(frac >> FRACBITS) & heightmask]];
#endif
                                }

                                dest += SCREENWIDTH;
                                frac += fracstep;
                            } while (count--);
                        }
                    }
                }
            }
            else
            {                   // Render single layer
                if (pl->special == 200)
                {               // Use sky 2
                    offset = Sky2ColumnOffset >> 16;
                    skyTexture = texturetranslation[Sky2Texture];
                }
                else
                {               // Use sky 1
                    if (vid_uncapped_fps)
                    {
                        offset = 0;
                        smoothDelta1 = Sky1ColumnOffset << 6;
                    }
                    else
                    {
                        offset = Sky1ColumnOffset >> 16;
                    }
                    skyTexture = texturetranslation[Sky1Texture];
                }

                if (skyTexture != prev_skyTexture)
                {
                    skyheight = R_GetPatchHeight(skyTexture, 0);
                    prev_skyTexture = skyTexture;
                }

                for (x = pl->minx; x <= pl->maxx; x++)
                {
                    dc_yl = pl->top[x];
                    dc_yh = pl->bottom[x];
                    if ((unsigned) dc_yl <= dc_yh) // [crispy] 32-bit integer math
                    {
                        // [crispy] Optionally draw skies horizontally linear.
                        const int angle = ((viewangle + smoothDelta1 + (vis_linear_sky ? 
                                        linearskyangle[x] : xtoviewangle[x])) ^ gp_flip_levels) >> ANGLETOSKYSHIFT;

                        count = dc_yh - dc_yl;
                        if (count < 0)
                        {
                            return;
                        }
                        source = R_GetColumn(skyTexture, angle + offset);
                        dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
                        frac = SKYTEXTUREMIDSHIFTED * FRACUNIT + (dc_yl - centery) * fracstep;
                        // not a power of 2 -- killough
                        if (skyheight & (skyheight - 1))
                        {
                            heightmask = skyheight << FRACBITS;

                            if (frac < 0)
                                while ((frac += heightmask) < 0);
                            else
                                while (frac >= heightmask)
                                    frac -= heightmask;

                            do
                            {
#ifndef CRISPY_TRUECOLOR
                                *dest = source[frac >> FRACBITS];
#else
                                *dest = pal_color[source[frac >> FRACBITS]];
#endif
                                dest += SCREENWIDTH;

                                if ((frac += fracstep) >= heightmask)
                                {
                                    frac -= heightmask;
                                }
                            } while (count--);
                        }
                        // texture height is a power of 2 -- killough
                        else
                        {
                            heightmask = skyheight - 1;

                            do
                            {
#ifndef CRISPY_TRUECOLOR
                                *dest = source[(frac >> FRACBITS) & heightmask];
#else
                                *dest = pal_color[source[(frac >> FRACBITS) & heightmask]];
#endif
                                dest += SCREENWIDTH;
                                frac += fracstep;
                            } while (count--);
                        }
                    }
                }
            }
        }
        else  // regular flat
        {
            const int swirling = (flattranslation[pl->picnum] == -1) ? 1 :
                                 (flattranslation[pl->picnum] == -2) ? 2 :
                                 (flattranslation[pl->picnum] == -3) ? 3 :
                                 (flattranslation[pl->picnum] == -4) ? 4 : 0;
            const int stop = pl->maxx + 1;
            const int lumpnum = firstflat + (swirling ? pl->picnum : flattranslation[pl->picnum]);

            // [crispy] adapt swirl from src/doom to src/hexen
            ds_source = swirling == 1 ? R_SwirlingFlat(lumpnum) :
                        swirling == 2 ? R_WarpingFlat1(lumpnum) :
                        swirling == 3 ? R_WarpingFlat2(lumpnum) :
                        swirling == 4 ? R_WarpingFlat3(lumpnum) :
                                        W_CacheLumpNum(lumpnum, PU_STATIC);

            // [JN] Apply flowing effect to swirling liquids.
            if (swirling == 1)
            {
                swirlFlow_x = swirlCoord_x;
                swirlFlow_y = swirlCoord_y;
            }
            else // Less amplitude for sludge and lava (/4).
            if (swirling > 1)
            {
                swirlFlow_x = swirlCoord_x >> 2;
                swirlFlow_y = swirlCoord_y >> 2;
            }
            else
            {
                swirlFlow_x = 0;
                swirlFlow_y = 0;
            }

            // [crispy] Use old value of interpfactor if uncapped and paused. This
            // ensures that scrolling stops smoothly when pausing.
            if (vid_uncapped_fps && realleveltime > oldleveltime && !crl_freeze)
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

            //[crispy] use smoothscrolloffsets to unconditonally animate all scrolling floors
            switch (pl->special)
            {                       // Handle scrolling flats
                case 201:
                case 202:
                case 203:          // Scroll_North_xxx
                    xsmoothscrolloffset = 0;
                    ysmoothscrolloffset = FLATSCROLL(pl->special - 201);
                    break;
                case 204:
                case 205:
                case 206:          // Scroll_East_xxx
                    xsmoothscrolloffset = -FLATSCROLL(pl->special - 204);
                    ysmoothscrolloffset = 0;
                    break;
                case 207:
                case 208:
                case 209:          // Scroll_South_xxx
                    xsmoothscrolloffset = 0;
                    ysmoothscrolloffset = -FLATSCROLL(pl->special - 207);
                    break;
                case 210:
                case 211:
                case 212:          // Scroll_West_xxx
                    xsmoothscrolloffset = FLATSCROLL(pl->special - 210);
                    ysmoothscrolloffset = 0;
                    break;
                case 213:
                case 214:
                case 215:          // Scroll_NorthWest_xxx
                    xsmoothscrolloffset = FLATSCROLL(pl->special - 213);
                    ysmoothscrolloffset = FLATSCROLL(pl->special - 213);
                    break;
                case 216:
                case 217:
                case 218:          // Scroll_NorthEast_xxx
                    xsmoothscrolloffset = -FLATSCROLL(pl->special - 216);
                    ysmoothscrolloffset = FLATSCROLL(pl->special - 216);
                    break;
                case 219:
                case 220:
                case 221:          // Scroll_SouthEast_xxx
                    xsmoothscrolloffset = -FLATSCROLL(pl->special - 219);
                    ysmoothscrolloffset = -FLATSCROLL(pl->special - 219);
                    break;
                case 222:
                case 223:
                case 224:          // Scroll_SouthWest_xxx
                    xsmoothscrolloffset = FLATSCROLL(pl->special - 222);
                    ysmoothscrolloffset = -FLATSCROLL(pl->special - 222);
                    break;
                default:
                    xsmoothscrolloffset = 0;
                    ysmoothscrolloffset = 0;
                    break;
            }
        
            planeheight = abs(pl->height-viewz);
            // [PN] Ensure 'light' is within the range [0, LIGHTLEVELS - 1] inclusively.
            const int light = BETWEEN(0, LIGHTLEVELS-1, (pl->lightlevel >> LIGHTSEGSHIFT) + (extralight * LIGHTBRIGHT));
            planezlight = zlight[light];
            pl->top[pl->minx-1] = pl->top[stop] = USHRT_MAX;

            for (int x = pl->minx ; x <= stop ; x++)
            {
                R_MakeSpans(x,pl->top[x-1], pl->bottom[x-1], pl->top[x], pl->bottom[x]);
            }

            if (!swirling)
            {
                W_ReleaseLumpNum(lumpnum);
            }
        }
    }
}
