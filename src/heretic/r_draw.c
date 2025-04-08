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
// R_draw.c

#include <stdlib.h>
#include "doomdef.h"
#include "deh_str.h"
#include "r_local.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_trans.h" // [crispy] blending functions

#include "id_vars.h"


/*

All drawing to the view buffer is accomplished in this file.  The other refresh
files only know about ccordinates, not the architecture of the frame buffer.

*/

byte *viewimage;
int viewwidth, scaledviewwidth, viewheight, viewwindowx, viewwindowy;
pixel_t *ylookup[MAXHEIGHT];
int columnofs[MAXWIDTH];
byte translations[3][256];      // color tables for different players

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

lighttable_t *dc_colormap[2];   // [crispy] brightmaps
int dc_x;
int dc_yl;
int dc_yh;
fixed_t dc_iscale;
fixed_t dc_texturemid;
int dc_texheight;
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

        *dest = I_BlendOver(*dest, destrgb, TINTTAB_ALPHA);

        dest += screenwidth;
        frac += fracstep;
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLColumn
// [PN] Draw translucent column, overlay blending. Low detail.
// -----------------------------------------------------------------------------

void R_DrawTLColumnLow(void)
{
    const int count = dc_yh - dc_yl;

    if (count < 0)
        return;

    // [PN] Blocky mode: double the x coordinate
    const int x = dc_x << 1;

    // [PN] Destination pointer calculation
    pixel_t *dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    pixel_t *dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x+1]];

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

        *dest = I_BlendOver(*dest, destrgb, TINTTAB_ALPHA);
        *dest2 = I_BlendOver(*dest2, destrgb, TINTTAB_ALPHA);

        dest += screenwidth;
        dest2 += screenwidth;
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

void R_DrawTranslatedColumnLow(void)
{
    const int count = dc_yh - dc_yl;

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
        *dest2 = index;

        dest += screenwidth;
        dest2 += screenwidth;
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
    int count = dc_yh - dc_yl;

    if (count < 0)
        return;

    // [PN] Destination pointer calculation
    pixel_t *dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // [PN] Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers for global arrays
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

void R_DrawTranslatedTLColumnLow(void)
{
    int count = dc_yh - dc_yl;

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

        const pixel_t destrgb = (brightmap[t] ? colormap1[t] : colormap0[t]);
        *dest = I_BlendOver(*dest, destrgb, TINTTAB_ALPHA);
        *dest2 = I_BlendOver(*dest2, destrgb, TINTTAB_ALPHA);

        dest += screenwidth;
        dest2 += screenwidth;
        frac += fracstep;
    }
}

// -----------------------------------------------------------------------------
// R_DrawExtraTLColumn
// [PN/JN] Extra translucent column.
// -----------------------------------------------------------------------------

void R_DrawExtraTLColumn(void)
{
    int count = dc_yh - dc_yl;

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

void R_DrawExtraTLColumnLow(void)
{
    int count = dc_yh - dc_yl;

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

    // [PN] Local pointers to speed up access
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

    // Allocate translation tables
    translationtables = Z_Malloc(256 * 3, PU_STATIC, 0);

    // Fill out the translation tables
    for (i = 0; i < 256; i++)
    {
        // [PN] Default case: all tables point to the original value
        translationtables[i] = i;
        translationtables[i + 256] = i;
        translationtables[i + 512] = i;

        // [PN] Special case: remap colors in the range 225-240
        if (i >= 225 && i <= 240)
        {
            const int offset = i - 225;

            translationtables[i] = 114 + offset;         // yellow
            translationtables[i + 256] = 145 + offset;   // red
            translationtables[i + 512] = 190 + offset;   // blue
        }
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
lighttable_t *ds_colormap[2];   // [crispy] brightmaps
fixed_t ds_xfrac;
fixed_t ds_yfrac;
fixed_t ds_xstep;
fixed_t ds_ystep;
byte *ds_source;                // start of a 64*64 tile image
const byte *ds_brightmap;       // [crispy] brightmaps


void R_DrawSpan(void)
{
    // Calculate the span length
    int count = ds_x2 - ds_x1 + 1;

    // [PN] Local pointers to global arrays
    const byte *const sourcebase = ds_source;
    const byte *const brightmap = ds_brightmap;
    const pixel_t *const colormap0 = ds_colormap[0];
    const pixel_t *const colormap1 = ds_colormap[1];
    const fixed_t xstep = ds_xstep;
    const fixed_t ystep = ds_ystep;

    // [PN] Local copies of fractional coordinates
    fixed_t xfrac = ds_xfrac;
    fixed_t yfrac = ds_yfrac;

    if (!gp_flip_levels)
    {
        // [PN] Precompute the destination pointer for normal levels
        pixel_t *dest = ylookup[ds_y] + columnofs[ds_x1];

        // [PN] Process in chunks of four pixels
        for (; count >= 4; count -= 4)
        {
            for (int j = 0; j < 4; j++)
            {
                const unsigned int ytemp = (yfrac >> 10) & 0x0fc0;
                const unsigned int xtemp = (xfrac >> 16) & 0x3f;
                const int spot = xtemp | ytemp;

                const byte source = sourcebase[spot];
                dest[j] = (brightmap[source] ? colormap1[source] : colormap0[source]);

                xfrac += xstep;
                yfrac += ystep;
            }

            dest += 4;
        }

        // [PN] Render remaining pixels if any
        for (; count > 0; count--)
        {
            const unsigned int ytemp = (yfrac >> 10) & 0x0fc0;
            const unsigned int xtemp = (xfrac >> 16) & 0x3f;
            const int spot = xtemp | ytemp;

            const byte source = sourcebase[spot];
            *dest = (brightmap[source] ? colormap1[source] : colormap0[source]);

            dest++;
            xfrac += xstep;
            yfrac += ystep;
        }
    }
    else
    {
        // [PN] Flipped levels
        for (int i = 0; i < count; i++)
        {
            const unsigned int ytemp = (yfrac >> 10) & 0x0fc0;
            const unsigned int xtemp = (xfrac >> 16) & 0x3f;
            const int spot = xtemp | ytemp;

            const byte source = sourcebase[spot];
            pixel_t *dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = (brightmap[source] ? colormap1[source] : colormap0[source]);

            xfrac += xstep;
            yfrac += ystep;
        }
    }

    // [PN] Store back updated fractional values
    ds_xfrac = xfrac;
    ds_yfrac = yfrac;
}

// -----------------------------------------------------------------------------
// R_DrawSpanLow
// Draws a horizontal span of pixels in blocky mode (low resolution).
// [PN] Uses a different approach depending on whether mirrored levels are enabled.
// Optimized by introducing local pointers for ds_source, ds_brightmap, ds_colormap,
// and converting do/while loops into for loops. This approach can improve readability and may
// allow better compiler optimizations, reducing overhead from repeated global lookups.
// The loop unrolling by four is retained for performance reasons.
// -----------------------------------------------------------------------------

void R_DrawSpanLow(void)
{
    // Calculate the span length
    int count = ds_x2 - ds_x1 + 1;

    // [PN] Blocky mode, multiply by 2
    ds_x1 <<= 1;
    ds_x2 <<= 1;

    // [PN] Local pointers to global arrays
    const byte *const sourcebase = ds_source;
    const byte *const brightmap = ds_brightmap;
    const pixel_t *const colormap0 = ds_colormap[0];
    const pixel_t *const colormap1 = ds_colormap[1];
    const fixed_t xstep = ds_xstep;
    const fixed_t ystep = ds_ystep;

    // [PN] Local copies of fractional coordinates
    fixed_t xfrac = ds_xfrac;
    fixed_t yfrac = ds_yfrac;

    if (!gp_flip_levels)
    {
        // [PN] Precompute the destination pointer for normal levels
        pixel_t *dest = ylookup[ds_y] + columnofs[ds_x1];

        // [PN] Process in chunks of four pixels
        while (count >= 4)
        {
            for (int j = 0; j < 4; j++)
            {
                const unsigned int ytemp = (yfrac >> 10) & 0x0fc0;
                const unsigned int xtemp = (xfrac >> 16) & 0x3f;
                const int spot = xtemp | ytemp;

                const byte source = sourcebase[spot];
                dest[0] = (brightmap[source] ? colormap1[source] : colormap0[source]);
                dest[1] = (brightmap[source] ? colormap1[source] : colormap0[source]);
                dest += 2;

                xfrac += xstep;
                yfrac += ystep;
            }

            count -= 4;
        }

        // [PN] Render remaining pixels if any
        while (count-- > 0)
        {
            const unsigned int ytemp = (yfrac >> 10) & 0x0fc0;
            const unsigned int xtemp = (xfrac >> 16) & 0x3f;
            const int spot = xtemp | ytemp;

            const byte source = sourcebase[spot];
            dest[0] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest[1] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest += 2;

            xfrac += xstep;
            yfrac += ystep;
        }
    }
    else
    {
        // [PN] Flipped levels
        for (int i = 0; i < count; i++)
        {
            const unsigned int ytemp = (yfrac >> 10) & 0x0fc0;
            const unsigned int xtemp = (xfrac >> 16) & 0x3f;
            const int spot = xtemp | ytemp;

            const byte source = sourcebase[spot];

            pixel_t *dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = (brightmap[source] ? colormap1[source] : colormap0[source]);

            dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = (brightmap[source] ? colormap1[source] : colormap0[source]);

            xfrac += xstep;
            yfrac += ystep;
        }
    }

    // [PN] Store back updated fractional values
    ds_xfrac = xfrac;
    ds_yfrac = yfrac;
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
// [JN] Replaced Heretic's original R_DrawViewBorder and R_DrawTopBorder
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
    const byte *src = W_CacheLumpName(DEH_String(gamemode == shareware ? "FLOOR04" : "FLAT513"), PU_CACHE);

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
        V_DrawPatch(x - delta, view_y - 4 + yy, W_CacheLumpName(DEH_String("bordt"), PU_CACHE));
        V_DrawPatch(x - delta, view_y + view_h, W_CacheLumpName(DEH_String("bordb"), PU_CACHE));
    }

    // [PN] Draw left and right borders
    for (int y = view_y; y < view_y + view_h; y += 16)
    {
        V_DrawPatch(view_x - 4 - delta, y, W_CacheLumpName(DEH_String("bordl"), PU_CACHE));
        V_DrawPatch(view_x + view_w - delta, y, W_CacheLumpName(DEH_String("bordr"), PU_CACHE));
    }

    // [PN] Draw corners
    V_DrawPatch(view_x - 4 - delta, view_y - 4 + yy, W_CacheLumpName(DEH_String("bordtl"), PU_CACHE));
    V_DrawPatch(view_x + view_w - delta, view_y - 4 + yy, W_CacheLumpName(DEH_String("bordtr"), PU_CACHE));
    V_DrawPatch(view_x + view_w - delta, view_y + view_h, W_CacheLumpName(DEH_String("bordbr"), PU_CACHE));
    V_DrawPatch(view_x - 4 - delta, view_y + view_h, W_CacheLumpName(DEH_String("bordbl"), PU_CACHE));

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

