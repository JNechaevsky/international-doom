//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2024 Julia Nechaevskaya
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


#include <stdlib.h>
#include "h2def.h"
#include "i_system.h"
#include "i_video.h"
#include "r_local.h"
#include "v_video.h"
#include "v_trans.h" // [crispy] blending functions

/*

All drawing to the view buffer is accomplished in this file.  The other refresh
files only know about ccordinates, not the architecture of the frame buffer.

*/

byte *viewimage;
int viewwidth, scaledviewwidth, viewheight, viewwindowx, viewwindowy;
pixel_t *ylookup[MAXHEIGHT];
int columnofs[MAXWIDTH];
//byte translations[3][256]; // color tables for different players

// Backing buffer containing the bezel drawn around the screen and 
// surrounding background.
static pixel_t *background_buffer = NULL;

/*
==================
=
= R_DrawColumn
=
= Source is the top of the column to scale
=
==================
*/

lighttable_t *dc_colormap[2]; // [crispy] brightmaps
int dc_x;
int dc_yl;
int dc_yh;
fixed_t dc_iscale;
fixed_t dc_texturemid;
int dc_texheight; // [crispy]
byte *dc_source;                // first pixel in a column (possibly virtual)

// -----------------------------------------------------------------------------
// R_DrawColumn
//
// [crispy] replace R_DrawColumn() with Lee Killough's implementation
// found in MBF to fix Tutti-Frutti, taken from mbfsrc/R_DRAW.C:99-1979
//
// [PN] Optimized to use local pointers for global arrays, replaced
// do/while with for loops, and simplified arithmetic operations.
// -----------------------------------------------------------------------------

void R_DrawColumn(void)
{
    int count;
    pixel_t *dest;
    fixed_t frac;
    fixed_t fracstep;
    int heightmask;
    int i;
    const byte *sourcebase;
    const byte *brightmap;
    const pixel_t *colormap0;
    const pixel_t *colormap1;
    int texheightmask;

    count = dc_yh - dc_yl;

    // [PN] If no pixels to draw, return immediately
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
    }
#endif

    // [PN] Destination pointer calculation
    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // [PN] Setup scaling
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to speed up access
    sourcebase = dc_source;
    brightmap = dc_brightmap;
    colormap0 = dc_colormap[0];
    colormap1 = dc_colormap[1];

    heightmask = dc_texheight - 1;
    texheightmask = dc_texheight;

    // [PN] Check if texture height is non-power of two
    if (dc_texheight & heightmask)
    {
        // [PN] For non-power-of-two textures, we use modulo operations.
        // Recalculate frac to ensure it's within texture bounds
        heightmask = (texheightmask << FRACBITS);
        frac = ((frac % heightmask) + heightmask) % heightmask;

        // [PN] Loop over all pixels
        for (i = 0; i <= count; i++)
        {
            unsigned s;
            unsigned index;

            s = sourcebase[frac >> FRACBITS];
            index = brightmap[s] ? colormap1[s] : colormap0[s];

            *dest = index;
            dest += SCREENWIDTH;

            // [PN] Update frac with modulo to wrap around texture height
            frac = (frac + fracstep) % heightmask;
        }
    }
    else
    {
        // [PN] For power-of-two textures, we can use bitmask &heightmask.
        // heightmask is dc_texheight-1, ensuring wrap with &heightmask
        for (i = 0; i <= count; i++)
        {
            unsigned s;
            unsigned index;

            s = sourcebase[(frac >> FRACBITS) & heightmask];
            index = brightmap[s] ? colormap1[s] : colormap0[s];

            *dest = index;
            dest += SCREENWIDTH;
            frac += fracstep;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawColumnLow
// [PN] Optimized to use local pointers for global arrays, replaced
// do/while with for loops, and simplified arithmetic operations.
// -----------------------------------------------------------------------------

void R_DrawColumnLow(void)
{
    int count;
    pixel_t *dest, *dest2;
    fixed_t frac, fracstep;
    int x;
    int heightmask;
    int i;
    const byte *sourcebase;
    const byte *brightmap;
    const pixel_t *colormap0;
    const pixel_t *colormap1;
    int texheightmask;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
    }
#endif

    // [PN] Blocky mode: double the x coordinate
    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers for faster access to global arrays
    sourcebase = dc_source;
    brightmap = dc_brightmap;
    colormap0 = dc_colormap[0];
    colormap1 = dc_colormap[1];

    heightmask = dc_texheight - 1;
    texheightmask = dc_texheight;

    // [PN] Check if texture height is non-power-of-two
    if (dc_texheight & heightmask)
    {
        // [PN] Non-power-of-two: use modulo to wrap frac
        heightmask = (texheightmask << FRACBITS);
        frac = ((frac % heightmask) + heightmask) % heightmask;

        for (i = 0; i <= count; i++)
        {
            unsigned s;
            unsigned index;

            s = sourcebase[frac >> FRACBITS];
            index = (brightmap[s] ? colormap1[s] : colormap0[s]);
            *dest = index;
            *dest2 = index;

            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;
            frac = (frac + fracstep) % heightmask;
        }
    }
    else
    {
        // [PN] Power-of-two texture height: use bitmask for fast wrapping
        for (i = 0; i <= count; i++)
        {
            unsigned s;
            unsigned index;

            s = sourcebase[(frac >> FRACBITS) & heightmask];
            index = (brightmap[s] ? colormap1[s] : colormap0[s]);
            *dest = index;
            *dest2 = index;

            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;
            frac += fracstep;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLColumn
// [PN] Draw translucent column, overlay blending. High detail.
// -----------------------------------------------------------------------------

void R_DrawTLColumn (void)
{
    int count;
    pixel_t *dest;
    fixed_t frac;
    fixed_t fracstep;
    int i;
    const byte *sourcebase;
    const byte *brightmap;
    const pixel_t *colormap0;
    const pixel_t *colormap1;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Use local pointers to speed up access to global arrays
    sourcebase = dc_source;
    brightmap = dc_brightmap;

    // [PN] dc_colormap is an array of two pointers to arrays of pixel_t[256]
    colormap0 = dc_colormap[0];
    colormap1 = dc_colormap[1];

    // [PN] Using a for loop for clarity
    for (i = 0; i <= count; i++)
    {
        unsigned s;
        s = sourcebase[frac >> FRACBITS];

        {
            pixel_t destrgb;
            // [PN] Select the correct colormap based on brightmap[s] and blend
            destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);
            *dest = I_BlendOver(*dest, destrgb, TINTTAB_ALPHA);
        }

        dest += SCREENWIDTH;
        frac += fracstep;
    }
}

//============================================================================
//
// R_DrawAltTLColumn
//
//============================================================================

void R_DrawAltTLColumn(void)
{
    int count;
    pixel_t *dest;
    fixed_t frac;
    fixed_t fracstep;
    int i;
    const byte *sourcebase;
    const byte *brightmap;
    const pixel_t *colormap0;
    const pixel_t *colormap1;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Use local pointers to speed up access to global arrays
    sourcebase = dc_source;
    brightmap = dc_brightmap;

    // [PN] dc_colormap is an array of two pointers to arrays of pixel_t[256]
    colormap0 = dc_colormap[0];
    colormap1 = dc_colormap[1];

    // [PN] Using a for loop for clarity
    for (i = 0; i <= count; i++)
    {
        unsigned s;
        s = sourcebase[frac >> FRACBITS];

        {
            pixel_t destrgb;
            // [PN] Select the correct colormap based on brightmap[s] and blend
            destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);
            *dest = I_BlendOver(*dest, destrgb, TINTTAB_ALPHA_ALT);
        }

        dest += SCREENWIDTH;
        frac += fracstep;
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLAddColumn
// [PN] Draw translucent column, additive blending. High detail.
// -----------------------------------------------------------------------------

void R_DrawTLAddColumn(void)
{
    int count;
    pixel_t *dest;
    fixed_t frac, fracstep;
    int i;
    const byte *sourcebase;
    const byte *brightmap;
    const pixel_t *colormap0;
    const pixel_t *colormap1;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to reduce global lookups
    sourcebase = dc_source;
    brightmap = dc_brightmap;
    colormap0 = dc_colormap[0];
    colormap1 = dc_colormap[1];

    {
        int iterations = count + 1; // converting do/while to for
        for (i = 0; i < iterations; i++)
        {
            unsigned s;
            pixel_t destrgb;

            s = sourcebase[frac >> FRACBITS];
            destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);

            *dest = I_BlendAdd(*dest, destrgb);

            dest += SCREENWIDTH;
            frac += fracstep;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLAddColumn
// [PN] Draw translucent column, additive blending. Low detail.
// -----------------------------------------------------------------------------

void R_DrawTLAddColumnLow(void)
{
    int count, x;
    fixed_t frac, fracstep;
    pixel_t *dest1, *dest2;
    int i;
    const byte *sourcebase;
    const byte *brightmap;
    const pixel_t *colormap0;
    const pixel_t *colormap1;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

    x = dc_x << 1;

    dest1 = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to reduce global lookups
    sourcebase = dc_source;
    brightmap = dc_brightmap;
    colormap0 = dc_colormap[0];
    colormap1 = dc_colormap[1];

    {
        int iterations = count + 1; // converting do/while to for
        for (i = 0; i < iterations; i++)
        {
            unsigned s;
            pixel_t destrgb;

            s = sourcebase[frac >> FRACBITS];
            destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);

            *dest1 = I_BlendAdd(*dest1, destrgb);
            *dest2 = I_BlendAdd(*dest2, destrgb);

            dest1 += SCREENWIDTH;
            dest2 += SCREENWIDTH;
            frac += fracstep;
        }
    }
}

/*
========================
=
= R_DrawTranslatedColumn
=
========================
*/

byte *dc_translation;
byte *translationtables;

void R_DrawTranslatedColumn(void)
{
    int count;
    pixel_t *dest;
    fixed_t frac;
    fixed_t fracstep;
    int i;
    const byte *sourcebase;
    const byte *brightmap;
    const byte *translation;
    const pixel_t *colormap0;
    const pixel_t *colormap1;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers for global arrays
    sourcebase = dc_source;
    brightmap = dc_brightmap;
    translation = dc_translation;
    colormap0 = dc_colormap[0];
    colormap1 = dc_colormap[1];

    {
        int iterations = count + 1;
        for (i = 0; i < iterations; i++)
        {
            unsigned s;
            unsigned t;
            unsigned index;

            s = sourcebase[frac >> FRACBITS];
            t = translation[s];
            // [PN] Choose correct colormap based on brightmap[s]
            index = (brightmap[s] ? colormap1[t] : colormap0[t]);

            *dest = index;
            dest += SCREENWIDTH;
            frac += fracstep;
        }
    }
}

//============================================================================
//
// R_DrawTranslatedTLColumn
//
//============================================================================

void R_DrawTranslatedTLColumn(void)
{
    int count;
    pixel_t *dest;
    fixed_t frac, fracstep;
    int i;
    const byte *sourcebase;
    const byte *brightmap;
    const byte *translation;
    const pixel_t *colormap0;
    const pixel_t *colormap1;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to global arrays
    sourcebase = dc_source;
    brightmap = dc_brightmap;
    translation = dc_translation;
    colormap0 = dc_colormap[0];
    colormap1 = dc_colormap[1];

    {
        int iterations = count + 1; // converting do/while to for
        for (i = 0; i < iterations; i++)
        {
            unsigned s;
            unsigned t;
            byte src;

            s = sourcebase[frac >> FRACBITS];
            t = translation[s];
            src = t;

            {
                pixel_t destrgb = (brightmap[src] ? colormap1[src] : colormap0[src]);
                *dest = I_BlendOver(*dest, destrgb, TINTTAB_ALPHA);
            }

            dest += SCREENWIDTH;
            frac += fracstep;
        }
    }
}

//============================================================================
//
// R_DrawTranslatedAltTLColumn
//
//============================================================================

/*
void R_DrawTranslatedAltTLColumn (void)
{
	int			count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((unsigned)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x];
	
	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	do
	{
		*dest = tinttable[*dest
			+(dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]]<<8)];
		dest += SCREENWIDTH;
		frac += fracstep;
	} while (count--);
}
*/

// -----------------------------------------------------------------------------
// R_DrawExtraTLColumn
// [JN] Extra translucent column.
// -----------------------------------------------------------------------------

void R_DrawExtraTLColumn(void)
{
    int count;
    pixel_t *dest;
    fixed_t frac, fracstep;
    int heightmask;
    int i;
    int iterations;
    const byte *sourcebase;
    const byte *brightmap;
    const pixel_t *colormap0;
    const pixel_t *colormap1;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawExtraTLColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers
    sourcebase = dc_source;
    brightmap = dc_brightmap;
    colormap0 = dc_colormap[0];
    colormap1 = dc_colormap[1];

    heightmask = dc_texheight - 1;

    // [PN] Check if texture height is non-power-of-two
    if (dc_texheight & heightmask) 
    {
        // Non-power-of-two path
        int fullmask;
        fullmask = (heightmask + 1) << FRACBITS;

        // Normalize frac within bounds of heightmask
        if (frac < 0)
        {
            while ((frac += fullmask) < 0)
                ; // just loop
        }
        else
        {
            while (frac >= fullmask)
                frac -= fullmask;
        }

        iterations = count + 1; // converting do/while to for
        for (i = 0; i < iterations; i++)
        {
            unsigned s;
            byte source;

            s = sourcebase[frac >> FRACBITS];
            source = s;

            {
                pixel_t destrgb = (brightmap[source] ? colormap1[source] : colormap0[source]);
                *dest = I_BlendOver(*dest, destrgb, EXTRATL_ALPHA);
            }

            dest += SCREENWIDTH;
            frac += fracstep;
            if (frac >= fullmask)
                frac -= fullmask;
        }
    }
    else
    {
        // Power-of-two path
        iterations = count + 1; // converting do/while to for
        for (i = 0; i < iterations; i++)
        {
            unsigned s;
            byte source;

            s = sourcebase[(frac >> FRACBITS) & heightmask];
            source = s;

            {
                pixel_t destrgb = (brightmap[source] ? colormap1[source] : colormap0[source]);
                *dest = I_BlendOver(*dest, destrgb, EXTRATL_ALPHA);
            }

            dest += SCREENWIDTH;
            frac += fracstep;
        }
    }
}

//
// Low detail mode version.
//

void R_DrawExtraTLColumnLow(void)
{
    int count;
    pixel_t *dest, *dest2;
    fixed_t frac, fracstep;
    int x;
    int heightmask = dc_texheight - 1; // [crispy]

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawExtraTLColumnLow: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    // Blocky mode, need to multiply by 2.
    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x+1]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    if (dc_texheight & heightmask) // not a power of 2 -- killough
    {
        heightmask++;
        heightmask <<= FRACBITS;

        if (frac < 0)
            while ((frac += heightmask) < 0);
        else
            while (frac >= heightmask)
                frac -= heightmask;

        do
        {
            // [crispy] brightmaps
            const byte source = dc_source[frac >> FRACBITS];
#ifndef CRISPY_TRUECOLOR
            // [JN] Draw full bright sprites with different functions, depending on user's choice.
            *dest2 = *dest = blendfunc[((*dest) << 8) + dc_colormap[dc_brightmap[source]][source]];
#else
            const pixel_t destrgb = dc_colormap[dc_brightmap[source]][source];
            *dest = I_BlendOver(*dest, destrgb, EXTRATL_ALPHA);
            *dest2 = I_BlendOver(*dest2, destrgb, EXTRATL_ALPHA);
#endif
            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;
            if ((frac += fracstep) >= heightmask)
                frac -= heightmask;
        } while (count--);
    }
    else // texture height is a power of 2 -- killough
    {
        do
        {
            // [crispy] brightmaps
            const byte source = dc_source[(frac >> FRACBITS) & heightmask];
#ifndef CRISPY_TRUECOLOR
            // [JN] Draw full bright sprites with different functions, depending on user's choice.
            *dest2 = *dest = blendfunc[((*dest) << 8) + dc_colormap[dc_brightmap[source]][source]];
#else
            const pixel_t destrgb = dc_colormap[dc_brightmap[source]][source];
            *dest = I_BlendOver(*dest, destrgb, EXTRATL_ALPHA);
            *dest2 = I_BlendOver(*dest2, destrgb, EXTRATL_ALPHA);
#endif

            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;
            frac += fracstep;
        }
        while (count--);
    }
}

//--------------------------------------------------------------------------
//
// PROC R_InitTranslationTables
//
//--------------------------------------------------------------------------

void R_InitTranslationTables(void)
{
    int i;
    byte *transLump;
    int lumpnum;

    // Allocate translation tables
    translationtables = Z_Malloc(256 * 3 * (maxplayers - 1), PU_STATIC, 0);

    for (i = 0; i < 3 * (maxplayers - 1); i++)
    {
        lumpnum = W_GetNumForName("trantbl0") + i;
        transLump = W_CacheLumpNum(lumpnum, PU_STATIC);
        memcpy(translationtables + i * 256, transLump, 256);
        W_ReleaseLumpNum(lumpnum);
    }
}

// -----------------------------------------------------------------------------
// R_DrawSpan
// Draws a horizontal span of pixels.
//
// [PN] Uses a different approach depending on whether mirrored levels are enabled.
// Optimized by introducing local pointers for ds_source, ds_brightmap, ds_colormap,
// and converting do/while loops into for loops. This approach can improve readability and may
// allow better compiler optimizations, reducing overhead from repeated global lookups.
// The loop unrolling by four is retained for performance reasons.
// -----------------------------------------------------------------------------

int ds_y;
int ds_x1;
int ds_x2;
lighttable_t *ds_colormap;
fixed_t ds_xfrac;
fixed_t ds_yfrac;
fixed_t ds_xstep;
fixed_t ds_ystep;
byte *ds_source;                // start of a 64*64 tile image

int dscount;                    // just for profiling

void R_DrawSpan(void)
{
    unsigned int xtemp, ytemp;
    pixel_t *dest;
    int count;
    int spot;
    byte source;
    const byte *sourcebase;
    const pixel_t *colormap;
    int i;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH || ds_y > SCREENHEIGHT)
    {
        I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
    }
#endif

    // Calculate the span length.
    count = ds_x2 - ds_x1 + 1;

    // [PN] Local pointers to global arrays
    sourcebase = ds_source;
    // ds_colormap is of type lighttable_t * (which is unsigned int *), and pixel_t is unsigned int.
    // They are compatible, so we cast ds_colormap to const pixel_t*.
    colormap = (const pixel_t *)ds_colormap;

    if (!gp_flip_levels)
    {
        // Precompute the destination pointer for normal levels
        dest = ylookup[ds_y] + columnofs[ds_x1];

        // Process in chunks of four pixels
        while (count >= 4)
        {
            // 1st pixel
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[0] = colormap[source];
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // 2nd pixel
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[1] = colormap[source];
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // 3rd pixel
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[2] = colormap[source];
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // 4th pixel
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[3] = colormap[source];
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            dest += 4;
            count -= 4;
        }

        // Render remaining pixels if any
        for (i = 0; i < count; i++)
        {
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;

            source = sourcebase[spot];
            *dest++ = colormap[source];

            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;
        }
    }
    else
    {
        // Flipped levels
        // Convert do/while(count--) to a for loop
        int iterations = count;
        for (i = 0; i < iterations; i++)
        {
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;

            source = sourcebase[spot];
            dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = colormap[source];

            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;
        }
    }
}

void R_DrawSpanLow(void)
{
    unsigned int xtemp, ytemp;
    pixel_t *dest;
    int count;
    int spot;
    byte source;
    const byte *sourcebase;
    const pixel_t *colormap;
    int i;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH || ds_y > SCREENHEIGHT)
    {
        I_Error("R_DrawSpanLow: %i to %i at %i", ds_x1, ds_x2, ds_y);
    }
#endif

    // Calculate the span length.
    count = ds_x2 - ds_x1 + 1;

    // Blocky mode, multiply by 2
    ds_x1 <<= 1;
    ds_x2 <<= 1;

    // [PN] Local pointers to global arrays
    sourcebase = ds_source;
    colormap = (const pixel_t *)ds_colormap;

    if (!gp_flip_levels)
    {
        // [PN] Precompute destination pointer for normal levels
        dest = ylookup[ds_y] + columnofs[ds_x1];

        // Process in chunks of four sets of two pixels each
        while (count >= 4)
        {
            // First pair
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[0] = colormap[source];
            dest[1] = colormap[source];
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Second pair
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[0] = colormap[source];
            dest[1] = colormap[source];
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Third pair
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[0] = colormap[source];
            dest[1] = colormap[source];
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Fourth pair
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[0] = colormap[source];
            dest[1] = colormap[source];
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            count -= 4;
        }

        // Render remaining pixels one by one if any
        for (i = 0; i < count; i++)
        {
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;

            source = sourcebase[spot];
            *dest = colormap[source]; // first pixel
            dest++;
            *dest = colormap[source]; // second pixel
            dest++;

            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;
        }
    }
    else
    {
        // Flipped levels in blocky mode
        // Convert do/while(count--) into a for loop
        {
            int iterations = count;
            for (i = 0; i < iterations; i++)
            {
                ytemp = (ds_yfrac >> 10) & 0x0fc0;
                xtemp = (ds_xfrac >> 16) & 0x3f;
                spot = xtemp | ytemp;

                source = sourcebase[spot];

                // First pixel
                dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
                *dest = colormap[source];

                // Second pixel
                dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
                *dest = colormap[source];

                ds_xfrac += ds_xstep;
                ds_yfrac += ds_ystep;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// R_InitBuffer 
// Initializes the buffer for a given view width and height.
// Handles border offsets and row/column calculations.
// [PN] Simplified logic and improved readability for better understanding.
// -----------------------------------------------------------------------------

void R_InitBuffer(int width, int height)
{
    int i;

    // [PN] Handle resize: calculate horizontal offset (viewwindowx).
    viewwindowx = (SCREENWIDTH - width) >> 1;

    // [PN] Calculate column offsets (columnofs).
    for (i = 0; i < width; i++) 
    {
        columnofs[i] = viewwindowx + i;
    }

    // [PN] Calculate vertical offset (viewwindowy).
    // Simplified using ternary operator.
    viewwindowy = (width == SCREENWIDTH) ? 0 : (SCREENHEIGHT - SBARHEIGHT - height) >> 1;

    // [crispy] make sure viewwindowy is always an even number
    viewwindowy &= ~1;

    // [PN] Precalculate row offsets (ylookup) for each row.
    for (i = 0; i < height; i++) 
    {
        ylookup[i] = I_VideoBuffer + (i + viewwindowy) * SCREENWIDTH;
    }

    // [PN] Free the background buffer if it exists.
    if (background_buffer != NULL)
    {
        free(background_buffer);
        background_buffer = NULL;
    }
}

// -----------------------------------------------------------------------------
// [JN] Replaced Hexen's original R_DrawViewBorder and R_DrawTopBorder
// functions with Doom's implementation to improve performance and avoid
// precision problems when drawing beveled edges on smaller screen sizes.
// [PN] Optimized for readability and reduced code duplication.
// Pre-cache patches and precompute commonly used values to improve efficiency.
// -----------------------------------------------------------------------------

void R_FillBackScreen (void) 
{ 
    byte *src;
    int x, y;
    int yy;
    int view_x, view_y, view_w, view_h, delta;
	
	// If we are running full screen, there is no need to do any of this,
	// and the background buffer can be freed if it was previously in use.
	if (scaledviewwidth == SCREENWIDTH)
	{
		return;
	}
	
	// Allocate the background buffer if necessary
	if (background_buffer == NULL)
	{
		const int size = SCREENWIDTH * (SCREENHEIGHT - SBARHEIGHT);
		background_buffer = malloc(size * sizeof(*background_buffer));
	}

	// [JN] Attempt to round up precision problem on lower screen sizes.
    // Round up precision problem on lower screen sizes
    yy = (dp_screen_size < 6) ? 1 : 0;

	// Draw screen and bezel; this is done to a separate screen buffer.
	V_UseBuffer(background_buffer);

    // [PN] Get the source flat for the background
	src = W_CacheLumpName("F_022", PU_CACHE);
	
	// [crispy] use unified flat filling function
	V_FillFlat(0, SCREENHEIGHT-SBARHEIGHT, 0, SCREENWIDTH, src, background_buffer);

    // [PN] Calculate patch positions
    view_x = viewwindowx / vid_resolution;
    view_y = viewwindowy / vid_resolution;
    view_w = scaledviewwidth / vid_resolution;
    view_h = viewheight / vid_resolution;
    delta = WIDESCREENDELTA;

    // [PN] Draw the borders (top and bottom)
    for (x = view_x; x < view_x + view_w; x += 16)
    {
        V_DrawPatch(x - delta, view_y - 4 + yy, W_CacheLumpName("bordt", PU_CACHE));
        V_DrawPatch(x - delta, view_y + view_h, W_CacheLumpName("bordb", PU_CACHE));
    }

    // [PN] Draw the borders (left and right)
    for (y = view_y; y < view_y + view_h; y += 16)
    {
        V_DrawPatch(view_x - 4 - delta, y, W_CacheLumpName("bordl", PU_CACHE));
        V_DrawPatch(view_x + view_w - delta, y, W_CacheLumpName("bordr", PU_CACHE));
    }

    // [PN] Draw the corners
    V_DrawPatch(view_x - 4 - delta, view_y - 4 + yy, W_CacheLumpName("bordtl", PU_CACHE));
    V_DrawPatch(view_x + view_w - delta, view_y - 4 + yy, W_CacheLumpName("bordtr", PU_CACHE));
    V_DrawPatch(view_x + view_w - delta, view_y + view_h, W_CacheLumpName("bordbr", PU_CACHE));
    V_DrawPatch(view_x - 4 - delta, view_y + view_h, W_CacheLumpName("bordbl", PU_CACHE));

    // [PN] Restore the original buffer
    V_RestoreBuffer();
}


static void R_VideoErase (unsigned ofs, int count)
{ 
	if (background_buffer != NULL)
	{
		memcpy(I_VideoBuffer + ofs, background_buffer + ofs, count * sizeof(*I_VideoBuffer));
	}
} 

void R_DrawViewBorder (void) 
{ 
	int top, top2, top3;
	int side;
	int ofs;
	int i; 
	// [JN] Attempt to round up precision problem.
	int yy2 = 3, yy3 = 2;
    
	if (scaledviewwidth == SCREENWIDTH || background_buffer == NULL)
	{
		return;
	}

	top = ((SCREENHEIGHT - SBARHEIGHT) - viewheight) / 2;
	top2 = ((SCREENHEIGHT - SBARHEIGHT) - viewheight + yy2) / 2;
	top3 = (((SCREENHEIGHT - SBARHEIGHT) - viewheight) - yy3) / 2;
	side = (SCREENWIDTH - scaledviewwidth) / 2;

	// copy top and one line of left side
	R_VideoErase(0, top * SCREENWIDTH + side);
 
	// copy one line of right side and bottom 
	ofs = (viewheight + top3) * SCREENWIDTH - side;
	R_VideoErase(ofs, top2 * SCREENWIDTH + side);

	// copy sides using wraparound
	ofs = top * SCREENWIDTH + SCREENWIDTH - side;
	side <<= 1;

	for (i = 1 ; i < viewheight ; i++)
	{
		R_VideoErase (ofs, side);
		ofs += SCREENWIDTH;
	} 

	// ?
	V_MarkRect(0, 0, SCREENWIDTH, SCREENHEIGHT - SBARHEIGHT);
}
