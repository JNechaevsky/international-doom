//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:
//	The actual span/column drawing functions.
//	Here find the main potential for optimization,
//	 e.g. inline assembly, different algorithms.
//



#include <stdlib.h>

#include "doomdef.h"
#include "deh_main.h"

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "r_local.h"

// Needs access to LFB (guess what).
#include "v_video.h"
#include "v_trans.h"

// State.
#include "doomstat.h"
#include "m_random.h"

#include "id_vars.h"


// status bar height at bottom of screen
#define SBARHEIGHT		(32 * vid_resolution)

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//  not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//  and we need only the base address,
//  and the total size == width*height*depth/8.,
//


byte*		viewimage; 
int		viewwidth;
int		scaledviewwidth;
int		viewheight;
int		viewwindowx;
int		viewwindowy; 
pixel_t*		ylookup[MAXHEIGHT];
int		columnofs[MAXWIDTH]; 

// Color tables for different players,
//  translate a limited part to another
//  (color ramps used for  suit colors).
//
byte		translations[3][256];	
 
// Backing buffer containing the bezel drawn around the screen and 
// surrounding background.

static pixel_t *background_buffer = NULL;


//
// R_DrawColumn
// Source is the top of the column to scale.
//
lighttable_t*		dc_colormap[2]; // [crispy] brightmaps
int			dc_x; 
int			dc_yl; 
int			dc_yh; 
fixed_t			dc_iscale; 
fixed_t			dc_texturemid;
int			dc_texheight; // [crispy] Tutti-Frutti fix

// first pixel in a column (possibly virtual) 
byte*			dc_source;		


// -----------------------------------------------------------------------------
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
// [PN] Optimized handling of non-power-of-2 textures by using modulo operation
//      instead of iterative loops to normalize 'frac' within bounds.
//      General cleanup and improved readability of the power-of-2 path.
// -----------------------------------------------------------------------------

void R_DrawColumn (void)
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

void R_DrawColumnLow (void)
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


//
// Spectre/Invisibility.
//
#define FUZZTABLE		50 
#define FUZZOFF	(1)


int	fuzzoffset[FUZZTABLE] =
{
    FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
    FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
    FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
    FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF 
}; 

int	fuzzpos = 0; 

// [crispy] draw fuzz effect independent of rendering frame rate
static int fuzzpos_tic;
void R_SetFuzzPosTic (void)
{
	// [crispy] prevent the animation from remaining static
	if (fuzzpos == fuzzpos_tic)
	{
		fuzzpos = (fuzzpos + 1) % FUZZTABLE;
	}
	fuzzpos_tic = fuzzpos;
}
void R_SetFuzzPosDraw (void)
{
	fuzzpos = fuzzpos_tic;
}

// -----------------------------------------------------------------------------
// R_DrawFuzzColumn
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels from adjacent ones to left and right.
// Used with an all black colormap, this could create the SHADOW effect,
// i.e. spectres and invisible players.
//
// [PN] Optimized border adjustments by eliminating unnecessary `cutoff` variable.
//      Simplified fuzz position updating logic and removed redundant operations
//      for better readability.
// -----------------------------------------------------------------------------

void R_DrawFuzzColumn (void)
{
    int count;
    pixel_t* dest;
    boolean cutoff = (dc_yh == viewheight - 1); // [crispy]
    // [PN] Pointer to the fuzz drawing function
    const uint32_t (*fuzzdrawfunc)(uint32_t, int);

    // Adjust borders.
    if (!dc_yl)
        dc_yl = 1;

    if (cutoff)
        dc_yh = viewheight - 2;

    count = dc_yh - dc_yl;

    // Zero length.
    if (count < 0)
        return;

    // [PN] Determine which fuzz drawing function to use
    fuzzdrawfunc = (vis_improved_fuzz == 3) ? I_BlendDarkGrayscale : I_BlendDark;

#ifdef RANGECHECK
    if (dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error("R_DrawFuzzColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
    }
#endif

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // Looks like an attempt at dithering, using the colormap #6
    // (of 0-31, a bit brighter than average).
    do
    {
        const int fuzz_offset = SCREENWIDTH * fuzzoffset[fuzzpos];

#ifndef CRISPY_TRUECOLOR
        *dest = colormaps[6 * 256 + dest[fuzz_offset]];
#else
        *dest = fuzzdrawfunc(dest[fuzz_offset], 0xD3);
#endif

        // Update fuzzpos and clamp if necessary.
        fuzzpos = (fuzzpos + 1) % FUZZTABLE;
        if (fuzzpos == 0 && vis_improved_fuzz == 1)
        {
            fuzzpos = realleveltime > oldleveltime ? ID_Random() % 49 : 0;
        }

        dest += SCREENWIDTH;
    } while (count--);

    // [crispy] if the line at the bottom had to be cut off,
    // draw one extra line using only pixels of that line and the one above
    if (cutoff)
    {
        const int fuzz_offset = SCREENWIDTH * (fuzzoffset[fuzzpos] - FUZZOFF) / 2;

#ifndef CRISPY_TRUECOLOR
        *dest = colormaps[6 * 256 + dest[fuzz_offset]];
#else
        *dest = fuzzdrawfunc(dest[fuzz_offset], 0xD3);
#endif
    }
}

// -----------------------------------------------------------------------------
// R_DrawFuzzColumnLow
// Draws a vertical slice of pixels in blocky mode (low detail).
// Creates a fuzzy effect by copying pixels from adjacent ones.
// [PN] Optimized border adjustments by eliminating unnecessary `cutoff` variable.
//      Simplified fuzz position updating logic and removed redundant operations
//      for better readability.
// -----------------------------------------------------------------------------

void R_DrawFuzzColumnLow (void)
{
    int count;
    pixel_t *dest;
    pixel_t *dest2;
    int x;
    boolean cutoff = (dc_yh == viewheight - 1); // [crispy]
    // [PN] Pointer to the fuzz drawing function
    const uint32_t (*fuzzdrawfunc)(uint32_t, int);

    // Adjust borders.
    if (!dc_yl)
        dc_yl = 1;

    if (cutoff)
        dc_yh = viewheight - 2;

    count = dc_yh - dc_yl;

    // Zero length.
    if (count < 0)
        return;

    // Low detail mode, need to multiply by 2.
    x = dc_x << 1;

    // [PN] Determine which fuzz drawing function to use
    fuzzdrawfunc = (vis_improved_fuzz == 3) ? I_BlendDarkGrayscale : I_BlendDark;

#ifdef RANGECHECK
    if (x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error("R_DrawFuzzColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
    }
#endif

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    // Looks like an attempt at dithering.
    do
    {
        const int fuzz_offset = SCREENWIDTH * fuzzoffset[fuzzpos];

#ifndef CRISPY_TRUECOLOR
        *dest = colormaps[6 * 256 + dest[fuzz_offset]];
        *dest2 = colormaps[6 * 256 + dest2[fuzz_offset]];
#else
        *dest = fuzzdrawfunc(dest[fuzz_offset], 0xD3);
        *dest2 = fuzzdrawfunc(dest2[fuzz_offset], 0xD3);
#endif

        // Update fuzzpos and clamp if necessary.
        fuzzpos = (fuzzpos + 1) % FUZZTABLE;
        if (fuzzpos == 0 && vis_improved_fuzz)
        {
            fuzzpos = realleveltime > oldleveltime ? ID_Random() % 49 : 0;
        }

        dest += SCREENWIDTH;
        dest2 += SCREENWIDTH;
    } while (count--);

    // [crispy] if the line at the bottom had to be cut off,
    // draw one extra line using only pixels of that line and the one above
    if (cutoff)
    {
        const int fuzz_offset = SCREENWIDTH * (fuzzoffset[fuzzpos] - FUZZOFF) / 2;

#ifndef CRISPY_TRUECOLOR
        *dest = colormaps[6 * 256 + dest[fuzz_offset]];
        *dest2 = colormaps[6 * 256 + dest2[fuzz_offset]];
#else
        *dest = fuzzdrawfunc(dest[fuzz_offset], 0xD3);
        *dest2 = fuzzdrawfunc(dest2[fuzz_offset], 0xD3);
#endif
    }
}
 
  
  
 

//
// R_DrawTranslatedColumn
// Used to draw player sprites
//  with the green colorramp mapped to others.
// Could be used with different translation
//  tables, e.g. the lighter colored version
//  of the BaronOfHell, the HellKnight, uses
//  identical sprites, kinda brightened up.
//
byte*	dc_translation;
byte*	translationtables;

void R_DrawTranslatedColumn (void) 
{ 
    int			count; 
    pixel_t*		dest;
    fixed_t		frac;
    fixed_t		fracstep;	 
 
    count = dc_yh - dc_yl; 
    if (count < 0) 
	return; 
				 
#ifdef RANGECHECK 
    if (dc_x >= SCREENWIDTH
	|| dc_yl < 0
	|| dc_yh >= SCREENHEIGHT)
    {
	I_Error ( "R_DrawColumn: %i to %i at %i",
		  dc_yl, dc_yh, dc_x);
    }
    
#endif 


    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // Looks familiar.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Here we do an additional index re-mapping.
    do 
    {
	// Translation tables are used
	//  to map certain colorramps to other ones,
	//  used with PLAY sprites.
	// Thus the "green" ramp of the player 0 sprite
	//  is mapped to gray, red, black/indigo. 
	// [crispy] brightmaps
	const byte source = dc_source[frac>>FRACBITS];
	*dest = dc_colormap[dc_brightmap[source]][dc_translation[source]];
	dest += SCREENWIDTH;
	
	frac += fracstep; 
    } while (count--); 
} 

void R_DrawTranslatedColumnLow (void) 
{ 
    int			count; 
    pixel_t*		dest;
    pixel_t*		dest2;
    fixed_t		frac;
    fixed_t		fracstep;	 
    int                 x;
 
    count = dc_yh - dc_yl; 
    if (count < 0) 
	return; 

    // low detail, need to scale by 2
    x = dc_x << 1;
				 
#ifdef RANGECHECK 
    if (x >= SCREENWIDTH
	|| dc_yl < 0
	|| dc_yh >= SCREENHEIGHT)
    {
	I_Error ( "R_DrawColumn: %i to %i at %i",
		  dc_yl, dc_yh, x);
    }
    
#endif 


    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x+1]];

    // Looks familiar.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Here we do an additional index re-mapping.
    do 
    {
	// Translation tables are used
	//  to map certain colorramps to other ones,
	//  used with PLAY sprites.
	// Thus the "green" ramp of the player 0 sprite
	//  is mapped to gray, red, black/indigo. 
	// [crispy] brightmaps
	const byte source = dc_source[frac>>FRACBITS];
	*dest = dc_colormap[dc_brightmap[source]][dc_translation[source]];
	*dest2 = dc_colormap[dc_brightmap[source]][dc_translation[source]];
	dest += SCREENWIDTH;
	dest2 += SCREENWIDTH;
	
	frac += fracstep; 
    } while (count--); 
} 

void R_DrawTLColumn (void)
{
    int			count;
    pixel_t*		dest;
    fixed_t		frac;
    fixed_t		fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
	return;

#ifdef RANGECHECK
    if (dc_x >= SCREENWIDTH
	|| dc_yl < 0
	|| dc_yh >= SCREENHEIGHT)
    {
	I_Error ( "R_DrawColumn: %i to %i at %i",
		  dc_yl, dc_yh, dc_x);
    }
#endif

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    do
    {
        // [crispy] brightmaps
        const byte source = dc_source[frac>>FRACBITS];
#ifndef CRISPY_TRUECOLOR
        // [JN] Draw full bright sprites with different functions, depending on user's choice.
        *dest = blendfunc[(*dest<<8)+dc_colormap[dc_brightmap[source]][source]];
#else
        const pixel_t destrgb = dc_colormap[dc_brightmap[source]][source];
        *dest = blendfunc(*dest, destrgb);
#endif
	dest += SCREENWIDTH;
	frac += fracstep;
    } while (count--);
}

// [crispy] draw translucent column, low-resolution version
void R_DrawTLColumnLow (void)
{
    int			count;
    pixel_t*		dest;
    pixel_t*		dest2;
    fixed_t		frac;
    fixed_t		fracstep;
    int                 x;

    count = dc_yh - dc_yl;
    if (count < 0)
	return;

    x = dc_x << 1;

#ifdef RANGECHECK
    if (x >= SCREENWIDTH
	|| dc_yl < 0
	|| dc_yh >= SCREENHEIGHT)
    {
	I_Error ( "R_DrawColumn: %i to %i at %i",
		  dc_yl, dc_yh, x);
    }
#endif

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x+1]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    do
    {
	// [crispy] brightmaps
	const byte source = dc_source[frac>>FRACBITS];    
#ifndef CRISPY_TRUECOLOR
	// [JN] Draw full bright sprites with different functions, depending on user's choice.
	*dest = blendfunc[(*dest<<8)+dc_colormap[dc_brightmap[source]][source]];
	*dest2 = blendfunc[(*dest2<<8)+dc_colormap[dc_brightmap[source]][source]];
#else
	const pixel_t destrgb = dc_colormap[dc_brightmap[source]][source];
	*dest = blendfunc(*dest, destrgb);
	*dest2 = blendfunc(*dest2, destrgb);
#endif
	dest += SCREENWIDTH;
	dest2 += SCREENWIDTH;
	frac += fracstep;
    } while (count--);
}

// -----------------------------------------------------------------------------
// R_DrawTLFuzzColumn
// [JN] Translucent fuzz column.
// -----------------------------------------------------------------------------

void R_DrawTLFuzzColumn (void)
{
    int       count = dc_yh - dc_yl;
    pixel_t  *dest;
    fixed_t   frac;
    fixed_t   fracstep;

    if (count < 0)
    return;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    do
    {
#ifndef CRISPY_TRUECOLOR
        // actual translucency map lookup taken from boom202s/R_DRAW.C:255
        *dest = fuzzmap[(*dest<<8)+dc_colormap[0][dc_source[frac>>FRACBITS]]];
#else
        const pixel_t destrgb = dc_colormap[0][dc_source[frac>>FRACBITS]];

       *dest = I_BlendFuzz(*dest, destrgb);
#endif
        dest += SCREENWIDTH;
        frac += fracstep;
    } while (count--);
}

// -----------------------------------------------------------------------------
// R_DrawTLFuzzColumnLow
// [JN] Translucent fuzz column, low-resolution version.
// -----------------------------------------------------------------------------

void R_DrawTLFuzzColumnLow (void)
{
    int       count = dc_yh - dc_yl;
    int       x;
    pixel_t  *dest, *dest2;
    fixed_t   frac, fracstep;

    if (count < 0)
    return;

    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x+1]];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    do
    {
#ifndef CRISPY_TRUECOLOR
        const pixel_t source = dc_colormap[0][dc_source[frac>>FRACBITS]];

        *dest = fuzzmap[(*dest<<8)+source];
        *dest2 = fuzzmap[(*dest2<<8)+source];
#else
        const pixel_t destrgb = dc_colormap[0][dc_source[frac>>FRACBITS]];

       *dest = I_BlendFuzz(*dest, destrgb);
       *dest2 = I_BlendFuzz(*dest2, destrgb);
#endif
        dest += SCREENWIDTH;
        dest2 += SCREENWIDTH;
        frac += fracstep;
    } while (count--);
}

// -----------------------------------------------------------------------------
// R_DrawTransTLFuzzColumn
// [JN] Translucent, translated fuzz column.
// -----------------------------------------------------------------------------

void R_DrawTransTLFuzzColumn (void)
{
    int       count = dc_yh - dc_yl;
    pixel_t  *dest;
    fixed_t   frac;
    fixed_t   fracstep;

    if (count < 0)
    return;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    do
    {
#ifndef CRISPY_TRUECOLOR
        // actual translucency map lookup taken from boom202s/R_DRAW.C:255
        *dest = fuzzmap[(*dest<<8)+dc_colormap[0][dc_translation[dc_source[frac>>FRACBITS]]]];
#else
        const pixel_t destrgb = dc_colormap[0][dc_translation[dc_source[frac>>FRACBITS]]];

       *dest = I_BlendFuzz(*dest, destrgb);
#endif
        dest += SCREENWIDTH;
        frac += fracstep;
    } while (count--);
}

// -----------------------------------------------------------------------------
// R_DrawTransTLFuzzColumnLow
// [JN] Translucent, translated fuzz column, low-resolution version.
// -----------------------------------------------------------------------------

void R_DrawTransTLFuzzColumnLow (void)
{
    int       count = dc_yh - dc_yl;
    int       x;
    pixel_t  *dest, *dest2;
    fixed_t   frac, fracstep;

    if (count < 0)
    return;

    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x+1]];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    do
    {
#ifndef CRISPY_TRUECOLOR
        const pixel_t source = dc_colormap[0][dc_translation[dc_source[frac>>FRACBITS]]];

        *dest = fuzzmap[(*dest<<8)+source];
        *dest2 = fuzzmap[(*dest2<<8)+source];
#else
        const pixel_t destrgb = dc_colormap[0][dc_translation[dc_source[frac>>FRACBITS]]];

       *dest = I_BlendFuzz(*dest, destrgb);
       *dest2 = I_BlendFuzz(*dest2, destrgb);
#endif
        dest += SCREENWIDTH;
        dest2 += SCREENWIDTH;
        frac += fracstep;
    } while (count--);
}

//
// R_InitTranslationTables
// Creates the translation tables to map
//  the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void R_InitTranslationTables (void)
{
    int		i;
	
    translationtables = Z_Malloc (256*3, PU_STATIC, 0);
    
    // translate just the 16 green colors
    for (i=0 ; i<256 ; i++)
    {
	if (i >= 0x70 && i<= 0x7f)
	{
	    // map green ramp to gray, brown, red
	    const int offset = i & 0xf;  // [PN] Precalculate value

	    translationtables[i] = 0x60 + offset;
	    translationtables [i+256] = 0x40 + offset;
	    translationtables [i+512] = 0x20 + offset;
	}
	else
	{
	    // Keep all other colors as is.
	    translationtables[i] = translationtables[i+256] 
		= translationtables[i+512] = i;
	}
    }
}




//
// R_DrawSpan 
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//
int			ds_y; 
int			ds_x1; 
int			ds_x2;

lighttable_t*		ds_colormap[2];
const byte         *ds_brightmap;

fixed_t			ds_xfrac; 
fixed_t			ds_yfrac; 
fixed_t			ds_xstep; 
fixed_t			ds_ystep;

// start of a 64*64 tile image 
byte*			ds_source;	


// -----------------------------------------------------------------------------
// R_DrawSpan
// Draws a horizontal span of pixels.
// [PN] Uses a different approach depending on whether mirrored levels are enabled.
//      Precomputes the destination pointer when mirrored levels are off
//      for better performance.
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// R_DrawSpanLow
// Draws a horizontal span of pixels in blocky mode (low resolution).
// [PN] Uses a different approach depending on whether mirrored levels are enabled.
//      Precomputes the destination pointer when mirrored levels are off
//      for better performance.
// -----------------------------------------------------------------------------

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

void R_InitBuffer (int width, int height)
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
// R_FillBackScreen
// Fills the back screen with a pattern for variable screen sizes.
// Also draws a beveled edge.
// [PN] Optimized for readability and reduced code duplication.
//      Pre-cache patches and precompute commonly used values to improve efficiency.
// -----------------------------------------------------------------------------

void R_FillBackScreen (void) 
{ 
    byte *src;
    pixel_t *dest;
    patch_t *patch_top;
    patch_t *patch_bottom;
    patch_t *patch_left;
    patch_t *patch_right;
    patch_t *patch_tl;
    patch_t *patch_tr;
    patch_t *patch_bl;
    patch_t *patch_br;
    int viewx, viewy;
    int scaledwidth;
    int viewheight_res;

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

    // [PN] Use background buffer for drawing
    V_UseBuffer(background_buffer);
    
    // [PN] Cache the background texture and fill the screen
    src = W_CacheLumpName(DEH_String(gamemode == commercial ? "GRNROCK" : "FLOOR7_2"), PU_CACHE); 
    dest = background_buffer;

    // [PN] Pre-cache patches for border drawing
    patch_top = W_CacheLumpName(DEH_String("brdr_t"), PU_CACHE);
    patch_bottom = W_CacheLumpName(DEH_String("brdr_b"), PU_CACHE);
    patch_left = W_CacheLumpName(DEH_String("brdr_l"), PU_CACHE);
    patch_right = W_CacheLumpName(DEH_String("brdr_r"), PU_CACHE);
    patch_tl = W_CacheLumpName(DEH_String("brdr_tl"), PU_CACHE);
    patch_tr = W_CacheLumpName(DEH_String("brdr_tr"), PU_CACHE);
    patch_bl = W_CacheLumpName(DEH_String("brdr_bl"), PU_CACHE);
    patch_br = W_CacheLumpName(DEH_String("brdr_br"), PU_CACHE);

    // [PN] Precompute commonly used values
    viewx = viewwindowx / vid_resolution;
    viewy = viewwindowy / vid_resolution;
    scaledwidth = scaledviewwidth / vid_resolution;
    viewheight_res = viewheight / vid_resolution;

    // [crispy] Unified flat filling function
    V_FillFlat(0, SCREENHEIGHT - SBARHEIGHT, 0, SCREENWIDTH, src, dest);

    // [PN] Draw top and bottom borders
    for (int x = 0; x < scaledwidth; x += 8)
    {
        V_DrawPatch(viewx + x - WIDESCREENDELTA, viewy - 8, patch_top);
        V_DrawPatch(viewx + x - WIDESCREENDELTA, viewy + viewheight_res, patch_bottom);
    }

    // [PN] Draw left and right borders
    for (int y = 0; y < viewheight_res; y += 8)
    {
        V_DrawPatch(viewx - 8 - WIDESCREENDELTA, viewy + y, patch_left);
        V_DrawPatch(viewx + scaledwidth - WIDESCREENDELTA, viewy + y, patch_right);
    }

    // [PN] Draw corners
    V_DrawPatch(viewx - 8 - WIDESCREENDELTA, viewy - 8, patch_tl);
    V_DrawPatch(viewx + scaledwidth - WIDESCREENDELTA, viewy - 8, patch_tr);
    V_DrawPatch(viewx - 8 - WIDESCREENDELTA, viewy + viewheight_res, patch_bl);
    V_DrawPatch(viewx + scaledwidth - WIDESCREENDELTA, viewy + viewheight_res, patch_br);

    // [PN] Restore the previous buffer
    V_RestoreBuffer();
}

// -----------------------------------------------------------------------------
// Copy a screen buffer.
// [PN] Changed ofs to size_t for clarity and to represent offset more appropriately.
// -----------------------------------------------------------------------------

static void R_VideoErase (size_t ofs, int count)
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

void R_DrawViewBorder (void) 
{ 
    int top;
    int side;
    int ofs;
    int top_offset;
    int bottom_offset;

    if (scaledviewwidth == SCREENWIDTH || background_buffer == NULL)
    {
        return;
    }

    top = ((SCREENHEIGHT - SBARHEIGHT) - viewheight) / 2;
    side = (SCREENWIDTH - scaledviewwidth) / 2;

    // [PN] Precompute common offsets
    top_offset = top * SCREENWIDTH + side;
    bottom_offset = (viewheight + top) * SCREENWIDTH - side;

    // [PN] Copy top and bottom sections
    R_VideoErase(0, top_offset);
    R_VideoErase(bottom_offset, top_offset);

    // [PN] Precompute for sides
    ofs = top * SCREENWIDTH + SCREENWIDTH - side;
    side <<= 1;

    // [PN] Copy sides
    for (int i = 1; i < viewheight; i++) 
    { 
        R_VideoErase(ofs, side);
        ofs += SCREENWIDTH;
    }

    // [PN] Mark the entire screen for refresh
    V_MarkRect(0, 0, SCREENWIDTH, SCREENHEIGHT - SBARHEIGHT);
}
