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
// -----------------------------------------------------------------------------

void R_DrawColumn(void)
{
    int count;
    pixel_t *dest;
    fixed_t frac, fracstep;
    int heightmask = dc_texheight - 1;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
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
	*dest = dc_colormap[dc_brightmap[source]][source];
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
        *dest = dc_colormap[dc_brightmap[source]][source];
        dest += SCREENWIDTH;
        frac += fracstep;
    }
    while (count--);
  }
}

//
// Low detail mode version.
//

void R_DrawColumnLow(void)
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
        I_Error("R_DrawColumnLow: %i to %i at %i", dc_yl, dc_yh, dc_x);
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
            const byte source = dc_source[frac>>FRACBITS];
            *dest2 = *dest = dc_colormap[dc_brightmap[source]][source];
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
            *dest2 = *dest = dc_colormap[dc_brightmap[source]][source];
            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;
            frac += fracstep;
        }
        while (count--);
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
        if (i >= 225 && i <= 240)
        {
            translationtables[i] = 114 + (i - 225);     // yellow
            translationtables[i + 256] = 145 + (i - 225);       // red
            translationtables[i + 512] = 190 + (i - 225);       // blue
        }
        else
        {
            translationtables[i] = translationtables[i + 256]
                = translationtables[i + 512] = i;
        }
    }
}

/*
================
=
= R_DrawSpan
=
================
*/

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
    pixel_t *dest;
    int count, spot;
    unsigned int xtemp, ytemp;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH
        || (unsigned) ds_y > SCREENHEIGHT)
        I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif

    count = ds_x2 - ds_x1;
    do
    {
        byte source;
        // [crispy] fix flats getting more distorted the closer they are to the right
        ytemp = (ds_yfrac >> 10) & 0x0fc0;
        xtemp = (ds_xfrac >> 16) & 0x3f;
        spot = xtemp | ytemp;
        
        source = ds_source[spot];
        dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
        *dest = ds_colormap[ds_brightmap[source]][source];
        ds_xfrac += ds_xstep;
        ds_yfrac += ds_ystep;
    }
    while (count--);
}

void R_DrawSpanLow(void)
{
    unsigned int xtemp, ytemp;
    pixel_t *dest;
    int count, spot;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH
        || (unsigned) ds_y > SCREENHEIGHT)
        I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif

    count = ds_x2 - ds_x1;
    
    // Blocky mode, need to multiply by 2.
    ds_x1 <<= 1;
    ds_x2 <<= 1;
    
    do
    {
        byte source;
        // Calculate current texture index in u,v.
        // [crispy] fix flats getting more distorted the closer they are to the right
        ytemp = (ds_yfrac >> 10) & 0x0fc0;
        xtemp = (ds_xfrac >> 16) & 0x3f;
        spot = xtemp | ytemp;
        
        // Lowres/blocky mode does it twice,
        //  while scale is adjusted appropriately.
        source = ds_source[spot];
        dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
        *dest = ds_colormap[ds_brightmap[source]][source];
        dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
        *dest = ds_colormap[ds_brightmap[source]][source];

        ds_xfrac += ds_xstep;
        ds_yfrac += ds_ystep;
    }
    while (count--);
}



/*
================
=
= R_InitBuffer
=
=================
*/

void R_InitBuffer(int width, int height)
{
    int i;

    viewwindowx = (SCREENWIDTH - width) >> 1;
    for (i = 0; i < width; i++)
        columnofs[i] = viewwindowx + i;
    if (width == SCREENWIDTH)
        viewwindowy = 0;
    else
        viewwindowy = (SCREENHEIGHT - SBARHEIGHT - height) >> 1;
    // [crispy] make sure viewwindowy is always an even number
    viewwindowy &= ~1;
    for (i = 0; i < height; i++)
        ylookup[i] = I_VideoBuffer + (i + viewwindowy) * SCREENWIDTH;

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
// -----------------------------------------------------------------------------

void R_FillBackScreen (void) 
{ 
	byte    *src;
	pixel_t *dest;
	int x;
	int y;
	int yy = 0;
	
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

	// Draw screen and bezel; this is done to a separate screen buffer.
	
	V_UseBuffer(background_buffer);
	
	// [JN] Attempt to round up precision problem on lower screen sizes.
	if (dp_screen_size < 6)
	{
		yy = 1;
	}
	
	src = W_CacheLumpName(DEH_String(gamemode == shareware ? "FLOOR04" : "FLAT513"), PU_CACHE); 
	dest = background_buffer;
	
	// [crispy] use unified flat filling function
	V_FillFlat(0, SCREENHEIGHT-SBARHEIGHT, 0, SCREENWIDTH, src, dest);
	
	for (x = (viewwindowx / vid_resolution); x < (viewwindowx + scaledviewwidth) / vid_resolution; x += 16)
	{
		V_DrawPatch(x - WIDESCREENDELTA, ((viewwindowy / vid_resolution) - 4) + yy,
					W_CacheLumpName(DEH_String("bordt"), PU_CACHE));
		V_DrawPatch(x - WIDESCREENDELTA, (viewwindowy + viewheight) / vid_resolution,
					W_CacheLumpName(DEH_String("bordb"), PU_CACHE));
	}
	for (y = (viewwindowy / vid_resolution); y < (viewwindowy + viewheight) / vid_resolution; y += 16)
	{
		V_DrawPatch((viewwindowx / vid_resolution) - 4 - WIDESCREENDELTA, y,
					W_CacheLumpName(DEH_String("bordl"), PU_CACHE));
		V_DrawPatch(((viewwindowx + scaledviewwidth) / vid_resolution) - WIDESCREENDELTA, y,
					W_CacheLumpName(DEH_String("bordr"), PU_CACHE));
	}
	V_DrawPatch((viewwindowx / vid_resolution) - 4 - WIDESCREENDELTA,
				((viewwindowy / vid_resolution) - 4) + yy,
				W_CacheLumpName(DEH_String("bordtl"), PU_CACHE));
	V_DrawPatch(((viewwindowx + scaledviewwidth) / vid_resolution) - WIDESCREENDELTA,
				((viewwindowy / vid_resolution) - 4) + yy,
				W_CacheLumpName(DEH_String("bordtr"), PU_CACHE));
	V_DrawPatch(((viewwindowx + scaledviewwidth) / vid_resolution) - WIDESCREENDELTA,
				(viewwindowy + viewheight) / vid_resolution,
				W_CacheLumpName(DEH_String("bordbr"), PU_CACHE));
	V_DrawPatch((viewwindowx / vid_resolution) - 4 - WIDESCREENDELTA,
				(viewwindowy + viewheight) / vid_resolution,
				W_CacheLumpName(DEH_String("bordbl"), PU_CACHE));
                
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
