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
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
// 
// [crispy] replace R_DrawColumn() with Lee Killough's implementation
// found in MBF to fix Tutti-Frutti, taken from mbfsrc/R_DRAW.C:99-1979
//
// [PN] Optimized to use local pointers for global arrays, replaced
// do/while with for loops, and simplified arithmetic operations.
// -----------------------------------------------------------------------------

void R_DrawColumn(void)
{
    const int count = dc_yh - dc_yl;

    // [PN] If no pixels to draw, return immediately
    if (count < 0)
        return;

    // [PN] Destination pointer calculation
    pixel_t *dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // [PN] Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to speed up access
    const byte *const sourcebase = dc_source;
    const byte *const brightmap = dc_brightmap;
    const pixel_t *const colormap0 = dc_colormap[0];
    const pixel_t *const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    int heightmask = dc_texheight - 1;
    const int texheightmask = dc_texheight;

    // [PN] Check if texture height is non-power of two
    if (dc_texheight & heightmask)
    {
        // [PN] For non-power-of-two textures, we use modulo operations.
        // Recalculate frac to ensure it's within texture bounds
        heightmask = (texheightmask << FRACBITS);
        frac = ((frac % heightmask) + heightmask) % heightmask;

        // [PN] Loop over all pixels
        for (int i = 0; i <= count; i++)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const unsigned index = brightmap[s] ? colormap1[s] : colormap0[s];

            *dest = index;
            dest += screenwidth;

            // [PN] Update frac with modulo to wrap around texture height
            frac = (frac + fracstep) % heightmask;
        }
    }
    else
    {
        // [PN] For power-of-two textures, we can use bitmask &heightmask.
        // heightmask is dc_texheight-1, ensuring wrap with &heightmask
        for (int i = 0; i <= count; i++)
        {
            const unsigned s = sourcebase[(frac >> FRACBITS) & heightmask];
            const unsigned index = brightmap[s] ? colormap1[s] : colormap0[s];

            *dest = index;
            dest += screenwidth;
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
    const int count = dc_yh - dc_yl;

    // [PN] If no pixels to draw, return immediately
    if (count < 0)
        return;

    // [PN] Blocky mode: double the x coordinate
    const int x = dc_x << 1;

    // [PN] Destination pointer calculation
    pixel_t *dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    pixel_t *dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    // [PN] Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers for faster access to global arrays
    const byte *const sourcebase = dc_source;
    const byte *const brightmap = dc_brightmap;
    const pixel_t *const colormap0 = dc_colormap[0];
    const pixel_t *const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    int heightmask = dc_texheight - 1;
    const int texheightmask = dc_texheight;

    // [PN] Check if texture height is non-power-of-two
    if (dc_texheight & heightmask)
    {
        // [PN] Non-power-of-two: use modulo to wrap frac
        heightmask = (texheightmask << FRACBITS);
        frac = ((frac % heightmask) + heightmask) % heightmask;

        for (int i = 0; i <= count; i++)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const unsigned index = (brightmap[s] ? colormap1[s] : colormap0[s]);
            *dest = index;
            *dest2 = index;

            dest += screenwidth;
            dest2 += screenwidth;
            frac = (frac + fracstep) % heightmask;
        }
    }
    else
    {
        // [PN] Power-of-two texture height: use bitmask for fast wrapping
        for (int i = 0; i <= count; i++)
        {
            const unsigned s = sourcebase[(frac >> FRACBITS) & heightmask];
            const unsigned index = (brightmap[s] ? colormap1[s] : colormap0[s]);
            *dest = index;
            *dest2 = index;

            dest += screenwidth;
            dest2 += screenwidth;
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
    const int count = dc_yh - dc_yl;

    if (count < 0)
        return;

    // [PN] Destination pointer calculation
    pixel_t *dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // [PN] Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to speed up access
    const byte *const sourcebase = dc_source;
    const byte *const brightmap = dc_brightmap;
    const pixel_t *const colormap0 = dc_colormap[0];
    const pixel_t *const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    for (int i = 0; i <= count; i++)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);

        *dest = I_BlendOver(*dest, destrgb, TRANMAP_ALPHA);

        dest += screenwidth;
        frac += fracstep;
    }
}

//============================================================================
//
// R_DrawAltTLColumn
//
//============================================================================

void R_DrawAltTLColumn (void)
{
    const int count = dc_yh - dc_yl;

    if (count < 0)
        return;

    // [PN] Destination pointer calculation
    pixel_t *dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // [PN] Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to speed up access
    const byte *const sourcebase = dc_source;
    const byte *const brightmap = dc_brightmap;
    const pixel_t *const colormap0 = dc_colormap[0];
    const pixel_t *const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    for (int i = 0; i <= count; i++)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);

        *dest = I_BlendOver(*dest, destrgb, TINTTAB_ALPHA_ALT);

        dest += screenwidth;
        frac += fracstep;
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLAddColumn
// [PN] Draw translucent column, additive blending. High detail.
// -----------------------------------------------------------------------------

void R_DrawTLAddColumn(void)
{
    const int count = dc_yh - dc_yl;

    if (count < 0)
        return;

    // [PN] Destination pointer calculation
    pixel_t *dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // [PN] Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to speed up access
    const byte *const sourcebase = dc_source;
    const byte *const brightmap = dc_brightmap;
    const pixel_t *const colormap0 = dc_colormap[0];
    const pixel_t *const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    const int iterations = count + 1;
    for (int i = 0; i < iterations; i++)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);

        *dest = I_BlendAdd(*dest, destrgb);

        dest += screenwidth;
        frac += fracstep;
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLAddColumn
// [PN] Draw translucent column, additive blending. Low detail.
// -----------------------------------------------------------------------------

void R_DrawTLAddColumnLow(void)
{
    const int count = dc_yh - dc_yl;

    if (count < 0)
        return;

    // [PN] Blocky mode: double the x coordinate
    const int x = dc_x << 1;

    // [PN] Destination pointer calculation
    pixel_t *dest1 = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    pixel_t *dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    // [PN] Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to speed up access
    const byte *const sourcebase = dc_source;
    const byte *const brightmap = dc_brightmap;
    const pixel_t *const colormap0 = dc_colormap[0];
    const pixel_t *const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    const int iterations = count + 1;
    for (int i = 0; i < iterations; i++)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);

        *dest1 = I_BlendAdd(*dest1, destrgb);
        *dest2 = I_BlendAdd(*dest2, destrgb);

        dest1 += screenwidth;
        dest2 += screenwidth;
        frac += fracstep;
    }
}

// -----------------------------------------------------------------------------
// R_DrawTranslatedColumn
// Used to draw player sprites with the green colorramp mapped to others.
// Could be used with different translation tables, e.g. the lighter colored
// version of the BaronOfHell, the HellKnight, uses identical sprites,
// kinda brightened up.
//
// [PN] Optimized to use local pointers for global arrays, replaced
// do/while with for loops, and simplified arithmetic operations.
// -----------------------------------------------------------------------------

byte *dc_translation;
byte *translationtables;

void R_DrawTranslatedColumn(void)
{
    const int count = dc_yh - dc_yl;

    if (count < 0)
        return;

    // [PN] Destination pointer calculation
    pixel_t *dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // [PN] Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to speed up access
    const byte *const sourcebase = dc_source;
    const byte *const brightmap = dc_brightmap;
    const byte *const translation = dc_translation;
    const pixel_t *const colormap0 = dc_colormap[0];
    const pixel_t *const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    const int iterations = count + 1;
    for (int i = 0; i < iterations; i++)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const unsigned t = translation[s];
        const unsigned index = (brightmap[s] ? colormap1[t] : colormap0[t]);

        *dest = index;
        dest += screenwidth;
        frac += fracstep;
    }
}

// -----------------------------------------------------------------------------
// R_DrawTranslatedTLColumn
//
// [PN] Optimized to use local pointers for global arrays, replaced
// do/while with for loops, and simplified arithmetic operations.
// -----------------------------------------------------------------------------

void R_DrawTranslatedTLColumn(void)
{
    const int count = dc_yh - dc_yl;

    if (count < 0)
        return;

    // [PN] Destination pointer calculation
    pixel_t *dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // [PN] Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to speed up access
    const byte *const sourcebase = dc_source;
    const byte *const brightmap = dc_brightmap;
    const byte *const translation = dc_translation;
    const pixel_t *const colormap0 = dc_colormap[0];
    const pixel_t *const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    for (int i = 0; i <= count; i++)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const unsigned t = translation[s];

        // [PN] In truecolor mode, blend with I_BlendOver
        const pixel_t destrgb = (brightmap[t] ? colormap1[t] : colormap0[t]);
        *dest = I_BlendOver(*dest, destrgb, TINTTAB_ALPHA);

        dest += screenwidth;
        frac += fracstep;
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
// [PN/JN] Extra translucent column.
// -----------------------------------------------------------------------------

void R_DrawExtraTLColumn(void)
{
    const int count = dc_yh - dc_yl;

    if (count < 0)
        return;

    // [PN] Destination pointer calculation
    pixel_t *dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // [PN] Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to speed up access
    const byte *const sourcebase = dc_source;
    const byte *const brightmap = dc_brightmap;
    const pixel_t *const colormap0 = dc_colormap[0];
    const pixel_t *const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    const int heightmask = dc_texheight - 1;

    // [PN] Check if texture height is non-power-of-two
    if (dc_texheight & heightmask) 
    {
        // Non-power-of-two path
        const int fullmask = (heightmask + 1) << FRACBITS;

        // Normalize frac within bounds of heightmask
        if (frac < 0)
        {
            while ((frac += fullmask) < 0);
        }
        else
        {
            while (frac >= fullmask)
                frac -= fullmask;
        }

        for (int i = 0; i <= count; i++)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);
            *dest = I_BlendOver(*dest, destrgb, EXTRATL_ALPHA);

            dest += screenwidth;
            frac += fracstep;
            if (frac >= fullmask)
                frac -= fullmask;
        }
    }
    else
    {
        // Power-of-two path
        for (int i = 0; i <= count; i++)
        {
            const unsigned s = sourcebase[(frac >> FRACBITS) & heightmask];
            const pixel_t destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);
            *dest = I_BlendOver(*dest, destrgb, EXTRATL_ALPHA);

            dest += screenwidth;
            frac += fracstep;
        }
    }
}

//
// Low detail mode version.
//

void R_DrawExtraTLColumnLow(void)
{
    const int count = dc_yh - dc_yl;

    if (count < 0)
        return;

    // [PN] Blocky mode: multiply x by 2
    const int x = dc_x << 1;

    // [PN] Destination pointer calculation
    pixel_t *dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    pixel_t *dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    // [PN] Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers for faster access to global arrays
    const byte *const sourcebase = dc_source;
    const byte *const brightmap = dc_brightmap;
    const pixel_t *const colormap0 = dc_colormap[0];
    const pixel_t *const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    const int heightmask = dc_texheight - 1;

    if (dc_texheight & heightmask)  // non-power-of-two
    {
        const int fullmask = (heightmask + 1) << FRACBITS;

        // Normalize frac
        if (frac < 0)
        {
            while ((frac += fullmask) < 0);
        }
        else
        {
            while (frac >= fullmask)
                frac -= fullmask;
        }

        for (int i = 0; i <= count; i++)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);
            *dest = I_BlendOver(*dest, destrgb, EXTRATL_ALPHA);
            *dest2 = I_BlendOver(*dest2, destrgb, EXTRATL_ALPHA);

            dest += screenwidth;
            dest2 += screenwidth;

            frac += fracstep;
            if (frac >= fullmask)
                frac -= fullmask;
        }
    }
    else
    {
        // power-of-two height
        for (int i = 0; i <= count; i++)
        {
            const unsigned s = sourcebase[(frac >> FRACBITS) & heightmask];
            const pixel_t destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);
            *dest = I_BlendOver(*dest, destrgb, EXTRATL_ALPHA);
            *dest2 = I_BlendOver(*dest2, destrgb, EXTRATL_ALPHA);

            dest += screenwidth;
            dest2 += screenwidth;
            frac += fracstep;
        }
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

void R_DrawSpan(void)
{
    // Calculate the span length.
    const int count = ds_x2 - ds_x1 + 1;

    // [PN] Local pointers to global arrays
    const byte *sourcebase = ds_source;
    const pixel_t *colormap = (const pixel_t *)ds_colormap; // Cast to match types
    const fixed_t xstep = ds_xstep;
    const fixed_t ystep = ds_ystep;

    if (!gp_flip_levels)
    {
        // [PN] Precompute the destination pointer for normal levels
        pixel_t *dest = ylookup[ds_y] + columnofs[ds_x1];

        // Process in chunks of four pixels
        int remaining = count;
        while (remaining >= 4)
        {
            for (int j = 0; j < 4; ++j)
            {
                const unsigned int ytemp = (ds_yfrac >> 10) & 0x0fc0;
                const unsigned int xtemp = (ds_xfrac >> 16) & 0x3f;
                const int spot = xtemp | ytemp;
                const byte source = sourcebase[spot];
                dest[j] = colormap[source];
                ds_xfrac += xstep;
                ds_yfrac += ystep;
            }

            dest += 4;
            remaining -= 4;
        }

        // Render remaining pixels if any
        for (int i = 0; i < remaining; i++)
        {
            const unsigned int ytemp = (ds_yfrac >> 10) & 0x0fc0;
            const unsigned int xtemp = (ds_xfrac >> 16) & 0x3f;
            const int spot = xtemp | ytemp;
            const byte source = sourcebase[spot];
            *dest++ = colormap[source];

            ds_xfrac += xstep;
            ds_yfrac += ystep;
        }
    }
    else
    {
        // Flipped levels
        for (int i = 0; i < count; i++)
        {
            const unsigned int ytemp = (ds_yfrac >> 10) & 0x0fc0;
            const unsigned int xtemp = (ds_xfrac >> 16) & 0x3f;
            const int spot = xtemp | ytemp;
            const byte source = sourcebase[spot];

            pixel_t *dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = colormap[source];

            ds_xfrac += xstep;
            ds_yfrac += ystep;
        }
    }
}


void R_DrawSpanLow(void)
{
    // Calculate the span length.
    const int count = ds_x2 - ds_x1 + 1;

    // Blocky mode, multiply by 2
    ds_x1 <<= 1;
    ds_x2 <<= 1;

    // [PN] Local pointers to global arrays
    const byte *sourcebase = ds_source;
    const pixel_t *colormap = (const pixel_t *)ds_colormap;
    const fixed_t xstep = ds_xstep;
    const fixed_t ystep = ds_ystep;

    if (!gp_flip_levels)
    {
        // [PN] Precompute destination pointer for normal levels
        pixel_t *dest = ylookup[ds_y] + columnofs[ds_x1];

        // Process in chunks of four sets of two pixels each
        int remaining = count;
        while (remaining >= 4)
        {
            for (int j = 0; j < 4; ++j)
            {
                const unsigned int ytemp = (ds_yfrac >> 10) & 0x0fc0;
                const unsigned int xtemp = (ds_xfrac >> 16) & 0x3f;
                const int spot = xtemp | ytemp;
                const byte source = sourcebase[spot];
                dest[0] = colormap[source];
                dest[1] = colormap[source];
                dest += 2;
                ds_xfrac += xstep;
                ds_yfrac += ystep;
            }

            remaining -= 4;
        }

        // Render remaining pixels one by one if any
        for (int i = 0; i < remaining; i++)
        {
            const unsigned int ytemp = (ds_yfrac >> 10) & 0x0fc0;
            const unsigned int xtemp = (ds_xfrac >> 16) & 0x3f;
            const int spot = xtemp | ytemp;

            const byte source = sourcebase[spot];
            *dest++ = colormap[source]; // first pixel
            *dest++ = colormap[source]; // second pixel

            ds_xfrac += xstep;
            ds_yfrac += ystep;
        }
    }
    else
    {
        // Flipped levels in blocky mode
        for (int i = 0; i < count; i++)
        {
            const unsigned int ytemp = (ds_yfrac >> 10) & 0x0fc0;
            const unsigned int xtemp = (ds_xfrac >> 16) & 0x3f;
            const int spot = xtemp | ytemp;

            const byte source = sourcebase[spot];

            // First pixel
            pixel_t *dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = colormap[source];

            // Second pixel
            dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = colormap[source];

            ds_xfrac += xstep;
            ds_yfrac += ystep;
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
    // [PN] Allocate the background buffer if necessary
    if (scaledviewwidth == SCREENWIDTH)
        return;

    if (background_buffer == NULL)
    {
        const int size = SCREENWIDTH * (SCREENHEIGHT - SBARHEIGHT);
        background_buffer = malloc(size * sizeof(*background_buffer));
    }

    // [PN] Adjust for precision issues on lower screen sizes
    const int yy = (dp_screen_size < 6) ? 1 : 0;

    // [PN] Use a separate buffer for drawing
    V_UseBuffer(background_buffer);

    // [PN] Retrieve the source flat for the background
    const byte *src = W_CacheLumpName("F_022", PU_CACHE);

    // [crispy] Unified flat filling function
    V_FillFlat(0, SCREENHEIGHT - SBARHEIGHT, 0, SCREENWIDTH, src, background_buffer);

    // [PN] Calculate patch positions
    const int view_x = viewwindowx / vid_resolution;
    const int view_y = viewwindowy / vid_resolution;
    const int view_w = scaledviewwidth / vid_resolution;
    const int view_h = viewheight / vid_resolution;
    const int delta = WIDESCREENDELTA;

    // [PN] Draw top and bottom borders
    for (int x = view_x; x < view_x + view_w; x += 16)
    {
        V_DrawPatch(x - delta, view_y - 4 + yy, W_CacheLumpName("bordt", PU_CACHE));
        V_DrawPatch(x - delta, view_y + view_h, W_CacheLumpName("bordb", PU_CACHE));
    }

    // [PN] Draw left and right borders
    for (int y = view_y; y < view_y + view_h; y += 16)
    {
        V_DrawPatch(view_x - 4 - delta, y, W_CacheLumpName("bordl", PU_CACHE));
        V_DrawPatch(view_x + view_w - delta, y, W_CacheLumpName("bordr", PU_CACHE));
    }

    // [PN] Draw corners
    V_DrawPatch(view_x - 4 - delta, view_y - 4 + yy, W_CacheLumpName("bordtl", PU_CACHE));
    V_DrawPatch(view_x + view_w - delta, view_y - 4 + yy, W_CacheLumpName("bordtr", PU_CACHE));
    V_DrawPatch(view_x + view_w - delta, view_y + view_h, W_CacheLumpName("bordbr", PU_CACHE));
    V_DrawPatch(view_x - 4 - delta, view_y + view_h, W_CacheLumpName("bordbl", PU_CACHE));

    // [PN] Restore the original buffer
    V_RestoreBuffer();
}

// -----------------------------------------------------------------------------
// Copy a screen buffer.
// [PN] Changed ofs to size_t for clarity and to represent offset more appropriately.
// -----------------------------------------------------------------------------

static void R_VideoErase (unsigned ofs, int count)
{ 
    // [PN] Ensure the background buffer is valid before copying
    if (background_buffer != NULL)
    {
        // [PN] Copy from background buffer to video buffer
        memcpy(I_VideoBuffer + ofs, background_buffer + ofs, count * sizeof(*I_VideoBuffer));
    }
}

// -----------------------------------------------------------------------------
// R_DrawViewBorder
// Draws the border around the view for different size windows.
// [PN] Optimized by precomputing common offsets and reducing repeated calculations.
//      Simplified logic for top, bottom, and side erasing.
// -----------------------------------------------------------------------------

void R_DrawViewBorder(void)
{
    if (scaledviewwidth == SCREENWIDTH || background_buffer == NULL)
        return;

    // [PN] Adjust for precision issues on lower screen sizes
    const int yy2 = (dp_screen_size < 6) ? 3 : 0;
    const int yy3 = (dp_screen_size < 6) ? 2 : 0;

    // [PN] Calculate top, side, and offsets
    const int top = ((SCREENHEIGHT - SBARHEIGHT) - viewheight) / 2;
    const int top2 = ((SCREENHEIGHT - SBARHEIGHT) - viewheight + yy2) / 2;
    const int top3 = (((SCREENHEIGHT - SBARHEIGHT) - viewheight) - yy3) / 2;
    int side = (SCREENWIDTH - scaledviewwidth) / 2;

    // [PN] Copy top and one line of left side
    R_VideoErase(0, top * SCREENWIDTH + side);

    // [PN] Copy one line of right side and bottom
    int ofs = (viewheight + top3) * SCREENWIDTH - side;
    R_VideoErase(ofs, top2 * SCREENWIDTH + side);

    // [PN] Copy sides using wraparound
    ofs = top * SCREENWIDTH + SCREENWIDTH - side;
    side <<= 1;

    for (int i = 1; i < viewheight; i++)
    {
       R_VideoErase(ofs, side);
       ofs += SCREENWIDTH;
    }

    // [PN] Mark the screen for refresh
    V_MarkRect(0, 0, SCREENWIDTH, SCREENHEIGHT - SBARHEIGHT);
}
