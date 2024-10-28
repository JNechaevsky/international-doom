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
// [crispy] replace R_DrawColumn() with Lee Killough's implementation
// found in MBF to fix Tutti-Frutti, taken from mbfsrc/R_DRAW.C:99-1979
//
// [PN] Optimized handling of non-power-of-2 textures by using modulo operation
//      instead of iterative loops to normalize 'frac' within bounds.
//      General cleanup and improved readability of the power-of-2 path.
// -----------------------------------------------------------------------------

void R_DrawColumn(void)
{
    int count;
    pixel_t *dest;
    fixed_t frac;
    fixed_t fracstep;
    int heightmask = dc_texheight - 1;

    count = dc_yh - dc_yl;

    // Zero length, column does not exceed a pixel.
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
    }
#endif

    // Framebuffer destination address.
    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // Determine scaling.
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // Handle non-power of 2 textures (Tutti-Frutti fix).
    if (dc_texheight & heightmask)
    {
        // Prepare heightmask for non-power of 2 textures.
        heightmask = (dc_texheight << FRACBITS);

        // Normalize frac within bounds of heightmask using modulo operation.
        frac = (frac % heightmask + heightmask) % heightmask;

        // Process each pixel with adjusted frac.
        do
        {
            const byte source = dc_source[frac >> FRACBITS];
            *dest = dc_colormap[dc_brightmap[source]][source];
            dest += SCREENWIDTH;
            frac = (frac + fracstep) % heightmask;
        } while (count--);
    }
    else // For power of 2 textures.
    {
        // Fast path for textures with height that is a power of 2.
        do
        {
            const byte source = dc_source[(frac >> FRACBITS) & heightmask];
            *dest = dc_colormap[dc_brightmap[source]][source]; // [crispy] brightmaps
            dest += SCREENWIDTH;
            frac += fracstep;
        } while (count--);
    }
}

//
// Low detail mode version.
//

void R_DrawColumnLow(void)
{
    int count;
    pixel_t *dest;
    pixel_t *dest2;
    fixed_t frac;
    fixed_t fracstep;
    int x;
    int heightmask = dc_texheight - 1;

    count = dc_yh - dc_yl;

    // Zero length.
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
    }
#endif

    // Blocky mode, need to multiply by 2.
    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x+1]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // Handle non-power of 2 textures (Tutti-Frutti fix).
    if (dc_texheight & heightmask)
    {
        // Prepare heightmask for non-power of 2 textures.
        heightmask = (dc_texheight << FRACBITS);

        // Normalize frac within bounds of heightmask using modulo operation.
        frac = (frac % heightmask + heightmask) % heightmask;

        // Process each pixel with adjusted frac.
        do
        {
            const byte source = dc_source[frac >> FRACBITS];
            *dest2 = *dest = dc_colormap[dc_brightmap[source]][source]; // [crispy] brightmaps

            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;

            frac = (frac + fracstep) % heightmask;

        } while (count--);
    }
    else // For power of 2 textures.
    {
        // Fast path for textures with height that is a power of 2.
        do
        {
            const byte source = dc_source[(frac >> FRACBITS) & heightmask];
            *dest2 = *dest = dc_colormap[dc_brightmap[source]][source]; // [crispy] brightmaps

            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;

            frac += fracstep;

        } while (count--);
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLColumn
// Translucent column draw - blended with background using tinttable.
// -----------------------------------------------------------------------------

void R_DrawTLColumn(void)
{
    int count;
    pixel_t *dest;
    fixed_t frac, fracstep;
    int heightmask = dc_texheight - 1; // [crispy]

    // [crispy] Show transparent lines at top and bottom of screen.
    /*
    if (!dc_yl)
        dc_yl = 1;
    if (dc_yh == viewheight - 1)
        dc_yh = viewheight - 2;
    */

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawTLColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

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
            *dest =
                tinttable[((*dest) << 8) +
                          dc_colormap[dc_brightmap[source]][source]];
#else
            const pixel_t destrgb = dc_colormap[dc_brightmap[source]][source];
            *dest = blendfunc(*dest, destrgb);
#endif
            dest += SCREENWIDTH;
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
            *dest =
                tinttable[((*dest) << 8) +
                          dc_colormap[dc_brightmap[source]][source]];
#else
            const pixel_t destrgb = dc_colormap[dc_brightmap[source]][source];
            *dest = blendfunc(*dest, destrgb);
#endif

            dest += SCREENWIDTH;
            frac += fracstep;
        }
        while (count--);
    }
}

//
// Low detail mode version.
//

void R_DrawTLColumnLow(void)
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
        I_Error("R_DrawTLColumnLow: %i to %i at %i", dc_yl, dc_yh, dc_x);
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
            *dest2 = *dest =
                tinttable[((*dest) << 8) +
                          dc_colormap[dc_brightmap[source]][source]];
#else
            const pixel_t destrgb = dc_colormap[dc_brightmap[source]][source];
            *dest2 = *dest = blendfunc(*dest, destrgb);
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
            *dest2 = *dest =
                tinttable[((*dest) << 8) +
                          dc_colormap[dc_brightmap[source]][source]];
#else
            const pixel_t destrgb = dc_colormap[dc_brightmap[source]][source];
            *dest2 = *dest = blendfunc(*dest, destrgb);
#endif

            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;
            frac += fracstep;
        }
        while (count--);
    }
}

// -----------------------------------------------------------------------------
// R_DrawTranslatedColumn
// -----------------------------------------------------------------------------

byte *dc_translation;
byte *translationtables;

void R_DrawTranslatedColumn(void)
{
    int count;
    pixel_t *dest;
    fixed_t frac, fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawTranslatedColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    do
    {
        // [crispy] brightmaps
        const byte source = dc_source[frac>>FRACBITS];
        *dest = dc_colormap[dc_brightmap[source]][dc_translation[source]];
        dest += SCREENWIDTH;
        frac += fracstep;
    }
    while (count--);
}

//
// Low detail mode version.
//

void R_DrawTranslatedColumnLow(void)
{
    int count, x;
    pixel_t *dest, *dest2;
    fixed_t frac, fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawTranslatedColumnLow: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    // Blocky mode, need to multiply by 2.
    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x+1]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    do
    {
        // [crispy] brightmaps
        const byte source = dc_source[frac>>FRACBITS];
        *dest2 = *dest = dc_colormap[dc_brightmap[source]][dc_translation[source]];
        dest += SCREENWIDTH;
        dest2 += SCREENWIDTH;
        frac += fracstep;
    }
    while (count--);
}

// -----------------------------------------------------------------------------
// R_DrawTranslatedTLColumn
// -----------------------------------------------------------------------------

void R_DrawTranslatedTLColumn(void)
{
    int count;
    pixel_t *dest;
    fixed_t frac, fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawTranslatedTLColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    do
    {
        // [crispy] brightmaps
        byte src = dc_translation[dc_source[frac >> FRACBITS]];
#ifndef CRISPY_TRUECOLOR
        *dest = tinttable[((*dest) << 8)
                          +
                          dc_colormap[dc_brightmap[src]][src]];
#else
        const pixel_t destrgb = dc_colormap[dc_brightmap[src]][src];
        *dest = blendfunc(*dest, destrgb);
#endif
        dest += SCREENWIDTH;
        frac += fracstep;
    }
    while (count--);
}

//
// Low detail mode version.
//

void R_DrawTranslatedTLColumnLow(void)
{
    int count, x;
    pixel_t *dest, *dest2;
    fixed_t frac, fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawTranslatedTLColumnLow: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    // Blocky mode, need to multiply by 2.
    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x+1]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    do
    {
        // [crispy] brightmaps
        byte src = dc_translation[dc_source[frac >> FRACBITS]];
#ifndef CRISPY_TRUECOLOR
        *dest2 = *dest = tinttable[((*dest) << 8)
                          +
                          dc_colormap[dc_brightmap[src]][src]];
#else
        const pixel_t destrgb = dc_colormap[dc_brightmap[src]][src];
        *dest2 = *dest = blendfunc(*dest, destrgb);
#endif
        dest += SCREENWIDTH;
        dest2 += SCREENWIDTH;
        frac += fracstep;
    }
    while (count--);
}

// -----------------------------------------------------------------------------
// R_DrawExtraTLColumn
// [JN] Extra translucent column.
// -----------------------------------------------------------------------------

void R_DrawExtraTLColumn(void) 
{ 
    int count; 
    pixel_t *dest; 
    fixed_t frac, fracstep; 
    int heightmask = dc_texheight - 1; // [crispy] 
 
    count = dc_yh - dc_yl; 
    if (count < 0) 
        return; 
 
#ifdef RANGECHECK 
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT) 
        I_Error("R_DrawExtraTLColumn: %i to %i at %i", dc_yl, dc_yh, dc_x); 
#endif 
 
    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]]; 
 
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
            *dest = blendfunc[((*dest) << 8) + dc_colormap[dc_brightmap[source]][source]]; 
#else 
            const pixel_t destrgb = dc_colormap[dc_brightmap[source]][source]; 
            *dest = blendfunc(*dest, destrgb); 
#endif 
            dest += SCREENWIDTH; 
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
            *dest = blendfunc[((*dest) << 8) + dc_colormap[dc_brightmap[source]][source]]; 
#else 
            const pixel_t destrgb = dc_colormap[dc_brightmap[source]][source]; 
            *dest = blendfunc(*dest, destrgb); 
#endif 
 
            dest += SCREENWIDTH; 
            frac += fracstep; 
        } 
        while (count--); 
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
            *dest2 = *dest = blendfunc(*dest, destrgb);
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
            *dest2 = *dest = blendfunc(*dest, destrgb);
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
// [PN] Uses a different approach depending on whether mirrored levels are enabled.
//      Precomputes the destination pointer when mirrored levels are off
//      for better performance.
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


void R_DrawSpan (void) 
{ 
    pixel_t *dest;
    int count;
    int spot;
    unsigned int xtemp, ytemp;
    byte source;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH || ds_y > SCREENHEIGHT)
    {
        I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
    }
#endif

    // Calculate the span length.
    count = ds_x2 - ds_x1 + 1;

    // Optimized version for normal (non-flipped) levels.
    if (!gp_flip_levels)
    {
        // [PN] Precompute the destination pointer for normal levels, without flipping.
        dest = ylookup[ds_y] + columnofs[ds_x1];

        // [JN/PN] Loop unrolled by four for performance optimization:
        while (count >= 4)
        {
            // First iteration
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = ds_source[spot];
            dest[0] = ds_colormap[ds_brightmap[source]][source];
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Second iteration
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = ds_source[spot];
            dest[1] = ds_colormap[ds_brightmap[source]][source];
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Third iteration
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = ds_source[spot];
            dest[2] = ds_colormap[ds_brightmap[source]][source];
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Fourth iteration
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = ds_source[spot];
            dest[3] = ds_colormap[ds_brightmap[source]][source];
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;
            
            dest += 4;
            count -= 4;
        }

        // Render remaining pixels one by one, if any
        while (count-- > 0)
        {
            // [crispy] fix flats getting more distorted the closer they are to the right
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;

            // Lookup the pixel and apply lighting.
            source = ds_source[spot];
            *dest = ds_colormap[ds_brightmap[source]][source];
            
            // Move to the next pixel.
            dest++;  // [PN] Increment destination pointer without recalculating.
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;
        }
    }
    else
    {
        // Version for mirrored (flipped) levels.
        do
        {
            // [crispy] fix flats getting more distorted the closer they are to the right
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;

            // Lookup the pixel and apply lighting.
            source = ds_source[spot];

            // [PN] Recalculate destination pointer using `flipviewwidth` for flipped levels.
            dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = ds_colormap[ds_brightmap[source]][source];

            // Move to the next pixel.
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

        } while (count--);
    }
}

void R_DrawSpanLow (void)
{
    unsigned int xtemp, ytemp;
    pixel_t *dest;
    int count;
    int spot;
    byte source;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH || ds_y > SCREENHEIGHT)
    {
        I_Error("R_DrawSpanLow: %i to %i at %i", ds_x1, ds_x2, ds_y);
    }
#endif

    // Calculate the span length.
    count = ds_x2 - ds_x1 + 1;

    // Blocky mode, need to multiply by 2.
    ds_x1 <<= 1;
    ds_x2 <<= 1;

    // Optimized version for normal (non-flipped) levels in blocky mode.
    if (!gp_flip_levels)
    {
        // [PN] Precompute the destination pointer for normal levels, without flipping.
        dest = ylookup[ds_y] + columnofs[ds_x1];

        // [JN/PN] Loop unrolled by four for performance optimization:
        while (count >= 4)
        {
            // First pair of pixels
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = ds_source[spot];            
            dest[0] = ds_colormap[ds_brightmap[source]][source];
            dest[1] = ds_colormap[ds_brightmap[source]][source];
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Second pair of pixels
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = ds_source[spot];            
            dest[0] = ds_colormap[ds_brightmap[source]][source];
            dest[1] = ds_colormap[ds_brightmap[source]][source];
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Third pair of pixels
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = ds_source[spot];            
            dest[0] = ds_colormap[ds_brightmap[source]][source];
            dest[1] = ds_colormap[ds_brightmap[source]][source];
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Fourth pair of pixels
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = ds_source[spot];            
            dest[0] = ds_colormap[ds_brightmap[source]][source];
            dest[1] = ds_colormap[ds_brightmap[source]][source];
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            count -= 4;
        }

        // Render remaining pixels one by one, if any
        while (count-- > 0)
        {
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;

            source = ds_source[spot];
            *dest = ds_colormap[ds_brightmap[source]][source];  // First pixel
            dest++;
            *dest = ds_colormap[ds_brightmap[source]][source];  // Second pixel
            dest++;

            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;
        }
    }
    else
    {
        // Version for mirrored (flipped) levels in blocky mode.
        do
        {
            // [crispy] fix flats getting more distorted the closer they are to the right
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;

            // Lookup the pixel and apply lighting.
            source = ds_source[spot];

            // [PN] Recalculate destination pointer using `flipviewwidth` for flipped levels.
            dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = ds_colormap[ds_brightmap[source]][source];

            dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = ds_colormap[ds_brightmap[source]][source];

            // Update fractional positions.
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

        } while (count--);
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
// [JN] Replaced Heretic's original R_DrawViewBorder and R_DrawTopBorder
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
	src = W_CacheLumpName(DEH_String(gamemode == shareware ? "FLOOR04" : "FLAT513"), PU_CACHE); 
	
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
        V_DrawPatch(x - delta, view_y - 4 + yy, W_CacheLumpName(DEH_String("bordt"), PU_CACHE));
        V_DrawPatch(x - delta, view_y + view_h, W_CacheLumpName(DEH_String("bordb"), PU_CACHE));
    }

    // [PN] Draw the borders (left and right)
    for (y = view_y; y < view_y + view_h; y += 16)
    {
        V_DrawPatch(view_x - 4 - delta, y, W_CacheLumpName(DEH_String("bordl"), PU_CACHE));
        V_DrawPatch(view_x + view_w - delta, y, W_CacheLumpName(DEH_String("bordr"), PU_CACHE));
    }

    // [PN] Draw the corners
    V_DrawPatch(view_x - 4 - delta, view_y - 4 + yy, W_CacheLumpName(DEH_String("bordtl"), PU_CACHE));
    V_DrawPatch(view_x + view_w - delta, view_y - 4 + yy, W_CacheLumpName(DEH_String("bordtr"), PU_CACHE));
    V_DrawPatch(view_x + view_w - delta, view_y + view_h, W_CacheLumpName(DEH_String("bordbr"), PU_CACHE));
    V_DrawPatch(view_x - 4 - delta, view_y + view_h, W_CacheLumpName(DEH_String("bordbl"), PU_CACHE));

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
	int yy2 = 0, yy3 = 0;
	int side;
	int ofs;
	int i; 
    
	if (scaledviewwidth == SCREENWIDTH || background_buffer == NULL)
	{
		return;
	}

	// [JN] Attempt to round up precision problem on lower screen sizes.
	if (dp_screen_size < 6)
	{
		yy2 = 3;
		yy3 = 2;
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
