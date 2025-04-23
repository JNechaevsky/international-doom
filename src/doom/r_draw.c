//
// Copyright(C) 1993-1996 Id Software, Inc.
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
#include "v_video.h"
#include "v_trans.h"
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

byte *viewimage; 
int   viewwidth;
int   scaledviewwidth;
int   viewheight;
int   viewwindowx;
int   viewwindowy; 
pixel_t *ylookup[MAXHEIGHT];
int      columnofs[MAXWIDTH]; 

// Color tables for different players,
//  translate a limited part to another
//  (color ramps used for  suit colors).
//
byte translations[3][256];	
 
// Backing buffer containing the bezel drawn around the screen and 
// surrounding background.

static pixel_t *background_buffer = NULL;


//
// R_DrawColumn
// Source is the top of the column to scale.
//

lighttable_t *dc_colormap[2]; // [crispy] brightmaps
int dc_x;
int dc_yl;
int dc_yh;
int dc_texheight; // [crispy] Tutti-Frutti fix
fixed_t dc_iscale;
fixed_t dc_texturemid;

// first pixel in a column (possibly virtual) 
byte *dc_source;
byte *dc_translation;
byte *translationtables;


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
    if (count < 0)
        return; // No pixels to draw

    // Pre-calculate destination pointer
    pixel_t *restrict dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // Pre-calculate scaling factors
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + ((dc_yl - centery) * fracstep);

    // Load base pointers for texture data and colormaps
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    // Texture wrapping specifics
    const int heightmask = dc_texheight - 1;
    const fixed_t heightshifted = dc_texheight << FRACBITS;

    if (dc_texheight & heightmask) // Non-power-of-two texture
    {
        frac = (frac % heightshifted + heightshifted) % heightshifted; // Normalize frac

        for (int i = 0; i <= count; ++i)
        {
            const unsigned s = sourcebase[frac >> FRACBITS]; // Texture sample
            *dest = brightmap[s] ? colormap1[s] : colormap0[s];
            dest += screenwidth;
            frac += fracstep;
            if (frac >= heightshifted)
                frac -= heightshifted; // Normalize frac inline
        }
    }
    else // Power-of-two texture
    {
        for (int i = 0; i <= count; ++i)
        {
            const unsigned s = sourcebase[(frac >> FRACBITS) & heightmask]; // Texture sample with mask
            *dest = brightmap[s] ? colormap1[s] : colormap0[s];
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
    if (count < 0)
        return; // No pixels to draw

    // Blocky mode: double the x coordinate
    const int x = dc_x << 1;

    // Destination pointer calculations
    pixel_t *restrict dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    // Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // Local pointers for faster access to global arrays
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    const int heightmask = dc_texheight - 1;
    const fixed_t heightshifted = dc_texheight << FRACBITS; // Pre-shifted height for modulo

    if (dc_texheight & heightmask) // Non-power-of-two texture
    {
        frac = ((frac % heightshifted) + heightshifted) % heightshifted; // Normalize frac within bounds

        for (int i = count; i >= 0; --i)
        {
            const unsigned s = sourcebase[frac >> FRACBITS]; // Texture sample
            const unsigned index = brightmap[s] ? colormap1[s] : colormap0[s];

            *dest = index;
            *dest2 = index;
            dest += screenwidth;
            dest2 += screenwidth;
            frac += fracstep;
            if (frac >= heightshifted) frac -= heightshifted; // Avoid modulo
        }
    }
    else // Power-of-two texture
    {
        for (int i = count; i >= 0; --i)
        {
            const unsigned s = sourcebase[(frac >> FRACBITS) & heightmask]; // Texture sample with bitmask
            const unsigned index = brightmap[s] ? colormap1[s] : colormap0[s];

            *dest = index;
            *dest2 = index;
            dest += screenwidth;
            dest2 += screenwidth;
            frac += fracstep; // Increment frac directly
        }
    }
}



//
// Spectre/Invisibility.
//
#define FUZZTABLE		50 
#define FUZZOFF	(1)


static const int fuzzoffset[FUZZTABLE] =
{
    FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
    FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
    FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
    FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF 
}; 

static int fuzzpos = 0; 

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
// [PN] Optimized to use local pointers for global arrays, replaced
// do/while with for loops, and simplified arithmetic operations.
// -----------------------------------------------------------------------------

void R_DrawFuzzColumn(void)
{
    const boolean cutoff = (dc_yh == viewheight - 1); // [crispy]
    if (cutoff)
        dc_yh = viewheight - 2;

    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Precompute destination pointer
    pixel_t *restrict dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // Local pointers to improve memory access
    const int *restrict const fuzzoffsetbase = fuzzoffset;
    int local_fuzzpos = fuzzpos;
    const int fuzzalpha = fuzz_alpha;
    const pixel_t *restrict const vbuf_start = I_VideoBuffer;
    const pixel_t *restrict const vbuf_end = I_VideoBuffer + SCREENAREA;
    const int screenwidth = SCREENWIDTH;

    // Aggressive optimization: inline fuzz offset calculations
    const int iterations = count + 1;
    for (int i = 0; i < iterations; ++i)
    {
        const int offset = screenwidth * fuzzoffsetbase[local_fuzzpos];
        const pixel_t *restrict src = dest + offset;

        // Ensure randomness by injecting horizontal shifts safely
        src = (src < vbuf_start) ? dest + screenwidth - 1 : src;

        if (src < vbuf_end)
            *dest = I_BlendDark(*src, fuzzalpha);

        // Update fuzzpos aggressively, avoiding modulo
        if (++local_fuzzpos == FUZZTABLE)
        {
            local_fuzzpos = 0;
            if (vis_improved_fuzz == 1 && realleveltime > oldleveltime)
                local_fuzzpos = ID_Random() % 49;
        }

        dest += screenwidth;
    }

    // Handle cutoff line aggressively
    if (cutoff)
    {
        const int fuzz_offset = screenwidth * (fuzzoffsetbase[local_fuzzpos] - FUZZOFF) / 2;
        *dest = I_BlendDark(dest[fuzz_offset], fuzzalpha);
    }

    // Restore fuzzpos
    fuzzpos = local_fuzzpos;
}


// -----------------------------------------------------------------------------
// R_DrawFuzzColumnLow
// Draws a vertical slice of pixels in blocky mode (low detail).
// Creates a fuzzy effect by copying pixels from adjacent ones.
//
// [PN] Optimized to use local pointers for global arrays, replaced
// do/while with for loops, and simplified arithmetic operations.
// -----------------------------------------------------------------------------

void R_DrawFuzzColumnLow(void)
{
    const boolean cutoff = (dc_yh == viewheight - 1); // [crispy]
    if (cutoff)
        dc_yh = viewheight - 2;

    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // Zero length check

    // Blocky mode: double the x coordinate
    const int x = dc_x << 1;

    // Destination pointer calculations
    pixel_t *restrict dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    // Local pointers to improve memory access
    const int *restrict const fuzzoffsetbase = fuzzoffset;
    int local_fuzzpos = fuzzpos;
    const int fuzzalpha = fuzz_alpha;
    const int screenwidth = SCREENWIDTH;
    const pixel_t *restrict const vbuf_start = I_VideoBuffer;
    const pixel_t *restrict const vbuf_end = I_VideoBuffer + SCREENAREA;

    // Aggressive optimization: calculate fuzz offsets directly in the loop
    const int iterations = count + 1;
    for (int i = 0; i < iterations; ++i)
    {
        const int offset = screenwidth * fuzzoffsetbase[local_fuzzpos];
        const pixel_t *restrict src1 = dest + offset;
        const pixel_t *restrict src2 = dest2 + offset;

        // Inject randomness safely, avoiding out-of-bounds accesses
        src1 = (src1 < vbuf_start) ? dest + screenwidth - 1 : src1;
        src2 = (src2 < vbuf_start) ? dest2 + screenwidth - 1 : src2;

        if (src1 < vbuf_end)
            *dest = I_BlendDark(*src1, fuzzalpha);
        if (src2 < vbuf_end)
            *dest2 = I_BlendDark(*src2, fuzzalpha);

        // Update fuzz position aggressively
        if (++local_fuzzpos == FUZZTABLE)
        {
            local_fuzzpos = 0;
            if (vis_improved_fuzz && realleveltime > oldleveltime)
                local_fuzzpos = ID_Random() % 49;
        }

        dest += screenwidth;
        dest2 += screenwidth;
    }

    // Aggressive handling of cutoff line
    if (cutoff)
    {
        const int fuzz_offset = screenwidth * (fuzzoffsetbase[local_fuzzpos] - FUZZOFF) / 2;
        *dest = I_BlendDark(dest[fuzz_offset], fuzzalpha);
        *dest2 = I_BlendDark(dest2[fuzz_offset], fuzzalpha);
    }

    // Restore fuzz position
    fuzzpos = local_fuzzpos;
}

// -----------------------------------------------------------------------------
// R_DrawFuzzTLColumn
// [PN/JN] Draw translucent column for fuzz effect, overlay blending. High detail.
// -----------------------------------------------------------------------------

void R_DrawFuzzTLColumn(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Local pointers for improved memory access
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;
    int y_end = dc_yh; 
    int y_start = dc_yl;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    // Compute one pixel, write it to two vertical lines
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t blended = I_BlendOver_64(*dest, destrgb);

        // Write two pixels (current and next line)
        dest[0] = blended;
        dest[screenwidth] = blended;

        // Move to next pair
        dest += screenwidth * step;
        frac += fracstep;
        y_start += step;
    }

    // Handle final odd line
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        dest[0] = I_BlendOver_64(*dest, brightmap[s] ? colormap1[s] : colormap0[s]);
    }
}

// -----------------------------------------------------------------------------
// R_DrawFuzzTLColumnLow
// [PN/JN] Draw translucent column for fuzz effect, overlay blending. Low detail.
// -----------------------------------------------------------------------------

void R_DrawFuzzTLColumnLow(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Low detail: double horizontal resolution
    const int x = dc_x << 1;

    // Local pointers for improved memory access
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointers
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    // Process screen in 2×2 pixel blocks (2 lines, 2 columns)
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = brightmap[s] ? colormap1[s] : colormap0[s];
        
        // Process two lines for both columns
        const pixel_t blended = I_BlendOver_64(*dest1, destrgb);
        dest1[0] = blended;
        dest1[screenwidth] = blended;
        
        const pixel_t blended2 = I_BlendOver_64(*dest2, destrgb);
        dest2[0] = blended2;
        dest2[screenwidth] = blended2;

        // Move to next pair of lines
        dest1 += screenwidth * step;
        dest2 += screenwidth * step;
        frac += fracstep;
        y_start += step;
    }

    // Handle final row if height is odd (draw single line, both columns)
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = brightmap[s] ? colormap1[s] : colormap0[s];
        
        dest1[0] = I_BlendOver_64(*dest1, destrgb);
        dest2[0] = I_BlendOver_64(*dest2, destrgb);
    }
}


// -----------------------------------------------------------------------------
// R_DrawFuzzBWColumn
// [PN] Draw grayscale fuzz columnn. High detail.
// -----------------------------------------------------------------------------

void R_DrawFuzzBWColumn(void)
{
    const boolean cutoff = (dc_yh == viewheight - 1); // [crispy]
    if (cutoff)
        dc_yh = viewheight - 2;

    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Precompute destination pointer
    pixel_t *restrict dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // Local pointers for improved memory access
    const int *restrict const fuzzoffsetbase = fuzzoffset;
    int local_fuzzpos = fuzzpos;
    const int fuzzalpha = fuzz_alpha;
    const int screenwidth = SCREENWIDTH;
    const pixel_t *restrict const vbuf_start = I_VideoBuffer;
    const pixel_t *restrict const vbuf_end = I_VideoBuffer + SCREENAREA;

    // Aggressive optimization: reduce overhead in loop
    const int iterations = count + 1;
    for (int i = 0; i < iterations; ++i)
    {
        const int offset = screenwidth * fuzzoffsetbase[local_fuzzpos];
        const pixel_t *restrict src = dest + offset;

        // Safely inject horizontal randomness
        src = (src < vbuf_start) ? dest + screenwidth - 1 : src;

        if (src < vbuf_end)
            *dest = I_BlendDarkGrayscale(*src, fuzzalpha);

        // Update fuzz position aggressively
        if (++local_fuzzpos == FUZZTABLE)
            local_fuzzpos = 0;

        dest += screenwidth; // Advance destination pointer
    }

    // Handle cutoff line
    if (cutoff)
    {
        const int fuzz_offset = screenwidth * (fuzzoffsetbase[local_fuzzpos] - FUZZOFF) / 2;
        *dest = I_BlendDarkGrayscale(dest[fuzz_offset], fuzzalpha);
    }

    // Restore fuzz position
    fuzzpos = local_fuzzpos;
}



// -----------------------------------------------------------------------------
// R_DrawFuzzBWColumnLow
// [PN] Draw grayscale fuzz columnn. Low detail.
// -----------------------------------------------------------------------------

void R_DrawFuzzBWColumnLow(void)
{
    const boolean cutoff = (dc_yh == viewheight - 1); // [crispy]
    if (cutoff)
        dc_yh = viewheight - 2;

    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Blocky mode: double the x coordinate
    const int x = dc_x << 1;

    // Destination pointer calculations
    pixel_t *restrict dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    // Local pointers for improved memory access
    const int *restrict const fuzzoffsetbase = fuzzoffset;
    int local_fuzzpos = fuzzpos;
    const int fuzzalpha = fuzz_alpha;
    const int screenwidth = SCREENWIDTH;
    const pixel_t *restrict const vbuf_start = I_VideoBuffer;
    const pixel_t *restrict const vbuf_end = I_VideoBuffer + SCREENAREA;

    // Loop for blending pixels with aggressive optimizations
    const int iterations = count + 1;
    for (int i = 0; i < iterations; ++i)
    {
        const int offset = screenwidth * fuzzoffsetbase[local_fuzzpos];
        const pixel_t *restrict src1 = dest + offset;
        const pixel_t *restrict src2 = dest2 + offset;

        // Safely inject horizontal randomness
        src1 = (src1 < vbuf_start) ? dest + screenwidth - 1 : src1;
        src2 = (src2 < vbuf_start) ? dest2 + screenwidth - 1 : src2;

        if (src1 < vbuf_end)
            *dest = I_BlendDarkGrayscale(*src1, fuzzalpha);
        if (src2 < vbuf_end)
            *dest2 = I_BlendDarkGrayscale(*src2, fuzzalpha);

        // Update fuzz position efficiently
        if (++local_fuzzpos == FUZZTABLE)
            local_fuzzpos = 0;

        dest += screenwidth;
        dest2 += screenwidth;
    }

    // Handle cutoff line
    if (cutoff)
    {
        const int fuzz_offset = screenwidth * (fuzzoffsetbase[local_fuzzpos] - FUZZOFF) / 2;

        *dest = I_BlendDarkGrayscale(dest[fuzz_offset], fuzzalpha);
        *dest2 = I_BlendDarkGrayscale(dest2[fuzz_offset], fuzzalpha);
    }

    // Restore fuzz position
    fuzzpos = local_fuzzpos;
}



// -----------------------------------------------------------------------------
// R_DrawTransTLFuzzColumn
// [PN/JN] Translucent, translated fuzz column.
// -----------------------------------------------------------------------------

void R_DrawTransTLFuzzColumn(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Local pointers for improved memory access
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const translation = dc_translation;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;
    int y_end = dc_yh; 
    int y_start = dc_yl;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    // Compute one pixel, write it to two vertical lines
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = colormap0[translation[s]];
        const pixel_t blended = I_BlendOver_64(*dest, destrgb);

        // Write two pixels (current and next line)
        dest[0] = blended;
        dest[screenwidth] = blended;

        // Move to next pair
        dest += screenwidth * step;
        frac += fracstep;
        y_start += step;
    }

    // Handle final odd line
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        dest[0] = I_BlendOver_64(*dest, colormap0[translation[s]]);
    }
}



// -----------------------------------------------------------------------------
// R_DrawTransTLFuzzColumnLow
// [PN/JN] Translucent, translated fuzz column, low-resolution version.
// -----------------------------------------------------------------------------

void R_DrawTransTLFuzzColumnLow(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Low detail: double horizontal resolution
    const int x = dc_x << 1;

    // Local pointers for improved memory access
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const translation = dc_translation;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointers
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    // Process screen in 2×2 pixel blocks (2 lines, 2 columns)
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = colormap0[translation[s]];
        
        // Process two lines for both columns
        const pixel_t blended1 = I_BlendOver_64(*dest1, destrgb);
        const pixel_t blended2 = I_BlendOver_64(*dest2, destrgb);

        dest1[0] = blended1;
        dest1[screenwidth] = blended1;
        dest2[0] = blended2;
        dest2[screenwidth] = blended2;

        // Move to next pair of lines
        dest1 += screenwidth * step;
        dest2 += screenwidth * step;
        frac += fracstep;
        y_start += step;
    }

    // Handle final row if height is odd (draw single line, both columns)
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = colormap0[translation[s]];

        dest1[0] = I_BlendOver_64(*dest1, destrgb);
        dest2[0] = I_BlendOver_64(*dest2, destrgb);
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

void R_DrawTranslatedColumn(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Destination pointer calculation
    pixel_t *restrict dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // Local pointers for improved memory access
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap = dc_brightmap;
    const byte *restrict const translation = dc_translation;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    // Aggressive optimization: minimize overhead inside the loop
    const int iterations = count + 1;
    for (int i = 0; i < iterations; ++i)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];  // Texture sample
        const unsigned t = translation[s];               // Translation lookup
        *dest = brightmap[s] ? colormap1[t] : colormap0[t]; // Conditionally blend using colormap

        dest += screenwidth; // Advance destination pointer
        frac += fracstep;    // Increment texture coordinate
    }
}


void R_DrawTranslatedColumnLow(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Blocky mode: double the x coordinate
    const int x = dc_x << 1;

    // Destination pointer calculations
    pixel_t *restrict dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    // Setup scaling
    const fixed_t fracstep = dc_iscale;
    fixed_t frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // Local pointers for improved memory access
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap = dc_brightmap;
    const byte *restrict const translation = dc_translation;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;

    // Aggressively optimized loop for blending pixels
    const int iterations = count + 1;
    for (int i = 0; i < iterations; ++i)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];  // Texture sample
        const unsigned t = translation[s];               // Translation lookup
        const pixel_t index = brightmap[s] ? colormap1[t] : colormap0[t]; // Conditional colormap lookup

        *dest = index;
        *dest2 = index;

        // Advance destination pointers and texture coordinate
        dest += screenwidth;
        dest2 += screenwidth;
        frac += fracstep;
    }
}



// -----------------------------------------------------------------------------
// R_DrawTLColumn
// [PN/JN] Draw translucent column, overlay blending. High detail.
// -----------------------------------------------------------------------------

void R_DrawTLColumn(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Local pointers for improved memory access
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;
    int y_end = dc_yh; 
    int y_start = dc_yl;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    // Compute one pixel, write it to two vertical lines
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t blended = I_BlendOver_168(*dest, destrgb);

        // Write two pixels (current and next line)
        dest[0] = blended;
        dest[screenwidth] = blended;

        // Move to next pair
        dest += screenwidth * step;
        frac += fracstep;
        y_start += step;
    }

    // Handle final odd line
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        dest[0] = I_BlendOver_168(*dest, brightmap[s] ? colormap1[s] : colormap0[s]);
    }
}



// -----------------------------------------------------------------------------
// R_DrawTLColumn
// [PN/JN] Draw translucent column, overlay blending. Low detail.
// -----------------------------------------------------------------------------

void R_DrawTLColumnLow(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Low detail: double horizontal resolution
    const int x = dc_x << 1;

    // Local pointers for improved memory access
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointers
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    // Process screen in 2×2 pixel blocks (2 lines, 2 columns)
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = brightmap[s] ? colormap1[s] : colormap0[s];
        
        // Process two lines for both columns
        const pixel_t blended = I_BlendOver_168(*dest1, destrgb);
        dest1[0] = blended;
        dest1[screenwidth] = blended;
        
        const pixel_t blended2 = I_BlendOver_168(*dest2, destrgb);
        dest2[0] = blended2;
        dest2[screenwidth] = blended2;

        // Move to next pair of lines
        dest1 += screenwidth * step;
        dest2 += screenwidth * step;
        frac += fracstep;
        y_start += step;
    }

    // Handle final row if height is odd (draw single line, both columns)
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = brightmap[s] ? colormap1[s] : colormap0[s];
        
        dest1[0] = I_BlendOver_168(*dest1, destrgb);
        dest2[0] = I_BlendOver_168(*dest2, destrgb);
    }
}


// -----------------------------------------------------------------------------
// R_DrawTLAddColumn
// [PN/JN] Draw translucent column, additive blending. High detail.
// -----------------------------------------------------------------------------

void R_DrawTLAddColumn(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Local pointers for improved memory access
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;
    int y_end = dc_yh; 
    int y_start = dc_yl;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    // Compute one pixel, write it to two vertical lines
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t blended = I_BlendAdd(*dest, destrgb);

        // Write two pixels (current and next line)
        dest[0] = blended;
        dest[screenwidth] = blended;

        // Move to next pair
        dest += screenwidth * step;
        frac += fracstep;
        y_start += step;
    }

    // Handle final odd line
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        dest[0] = I_BlendAdd(*dest, brightmap[s] ? colormap1[s] : colormap0[s]);
    }
}


// -----------------------------------------------------------------------------
// R_DrawTLAddColumn
// [PN/JN] Draw translucent column, additive blending. Low detail.
// -----------------------------------------------------------------------------

void R_DrawTLAddColumnLow(void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Low detail: double horizontal resolution
    const int x = dc_x << 1;

    // Local pointers for improved memory access
    const byte *restrict const sourcebase = dc_source;
    const byte *restrict const brightmap = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointers
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    // Process screen in 2×2 pixel blocks (2 lines, 2 columns)
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = brightmap[s] ? colormap1[s] : colormap0[s];
        
        // Process two lines for both columns
        const pixel_t blended = I_BlendAdd(*dest1, destrgb);
        dest1[0] = blended;
        dest1[screenwidth] = blended;
        
        const pixel_t blended2 = I_BlendAdd(*dest2, destrgb);
        dest2[0] = blended2;
        dest2[screenwidth] = blended2;

        // Move to next pair of lines
        dest1 += screenwidth * step;
        dest2 += screenwidth * step;
        frac += fracstep;
        y_start += step;
    }

    // Handle final row if height is odd (draw single line, both columns)
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t destrgb = brightmap[s] ? colormap1[s] : colormap0[s];
        
        dest1[0] = I_BlendAdd(*dest1, destrgb);
        dest2[0] = I_BlendAdd(*dest2, destrgb);
    }
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
    translationtables = Z_Malloc (256*3, PU_STATIC, 0);

    // translate just the 16 green colors
    for (int i=0 ; i<256 ; i++)
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

int ds_y; 
int ds_x1; 
int ds_x2;

lighttable_t *ds_colormap[2];
const byte   *ds_brightmap;

fixed_t ds_xfrac; 
fixed_t ds_yfrac; 
fixed_t ds_xstep; 
fixed_t ds_ystep;

// start of a 64*64 tile image 
byte *ds_source;


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

void R_DrawSpan(void)
{
    // Calculate the span length
    int count = ds_x2 - ds_x1 + 1;
    if (count <= 0)
        return; // No pixels to draw

    // Local pointers to global arrays
    const byte *restrict const sourcebase = ds_source;
    const byte *restrict const brightmap = ds_brightmap;
    const pixel_t *restrict const colormap0 = ds_colormap[0];
    const pixel_t *restrict const colormap1 = ds_colormap[1];
    const fixed_t xstep = ds_xstep;
    const fixed_t ystep = ds_ystep;

    // Local copies of fractional coordinates
    fixed_t xfrac = ds_xfrac;
    fixed_t yfrac = ds_yfrac;

    if (!gp_flip_levels)
    {
        // Precompute the destination pointer for normal levels
        pixel_t *restrict dest = ylookup[ds_y] + columnofs[ds_x1];

        // Process in chunks of four pixels
        while (count >= 4)
        {
            for (int j = 0; j < 4; ++j)
            {
                const unsigned ytemp = (yfrac >> 10) & 0x0FC0;
                const unsigned xtemp = (xfrac >> 16) & 0x3F;
                const int spot = xtemp | ytemp;

                const byte source = sourcebase[spot];
                dest[j] = brightmap[source] ? colormap1[source] : colormap0[source];

                xfrac += xstep;
                yfrac += ystep;
            }

            dest += 4;
            count -= 4;
        }

        // Render remaining pixels if any
        while (count > 0)
        {
            const unsigned ytemp = (yfrac >> 10) & 0x0FC0;
            const unsigned xtemp = (xfrac >> 16) & 0x3F;
            const int spot = xtemp | ytemp;

            const byte source = sourcebase[spot];
            *dest = brightmap[source] ? colormap1[source] : colormap0[source];

            ++dest;
            xfrac += xstep;
            yfrac += ystep;
            --count;
        }
    }
    else
    {
        // Flipped levels
        for (int i = 0; i < count; ++i)
        {
            const unsigned ytemp = (yfrac >> 10) & 0x0FC0;
            const unsigned xtemp = (xfrac >> 16) & 0x3F;
            const int spot = xtemp | ytemp;

            const byte source = sourcebase[spot];
            pixel_t *restrict dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = brightmap[source] ? colormap1[source] : colormap0[source];

            xfrac += xstep;
            yfrac += ystep;
        }
    }

    // Store back updated fractional values
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
    if (count <= 0)
        return; // No pixels to draw

    // Blocky mode, multiply by 2
    ds_x1 <<= 1;
    ds_x2 <<= 1;

    // Local pointers to global arrays
    const byte *restrict const sourcebase = ds_source;
    const byte *restrict const brightmap = ds_brightmap;
    const pixel_t *restrict const colormap0 = ds_colormap[0];
    const pixel_t *restrict const colormap1 = ds_colormap[1];
    const fixed_t xstep = ds_xstep;
    const fixed_t ystep = ds_ystep;

    // Local copies of fractional coordinates
    fixed_t xfrac = ds_xfrac;
    fixed_t yfrac = ds_yfrac;

    if (!gp_flip_levels)
    {
        // Precompute the destination pointer for normal levels
        pixel_t *restrict dest = ylookup[ds_y] + columnofs[ds_x1];

        // Process in chunks of four pixels
        while (count >= 4)
        {
            for (int j = 0; j < 4; ++j)
            {
                const unsigned ytemp = (yfrac >> 10) & 0x0FC0;
                const unsigned xtemp = (xfrac >> 16) & 0x3F;
                const int spot = xtemp | ytemp;

                const byte source = sourcebase[spot];
                dest[0] = brightmap[source] ? colormap1[source] : colormap0[source];
                dest[1] = brightmap[source] ? colormap1[source] : colormap0[source];
                dest += 2;

                xfrac += xstep;
                yfrac += ystep;
            }

            count -= 4;
        }

        // Render remaining pixels if any
        while (count-- > 0)
        {
            const unsigned ytemp = (yfrac >> 10) & 0x0FC0;
            const unsigned xtemp = (xfrac >> 16) & 0x3F;
            const int spot = xtemp | ytemp;

            const byte source = sourcebase[spot];
            dest[0] = brightmap[source] ? colormap1[source] : colormap0[source];
            dest[1] = brightmap[source] ? colormap1[source] : colormap0[source];
            dest += 2;

            xfrac += xstep;
            yfrac += ystep;
        }
    }
    else
    {
        // Flipped levels
        for (int i = 0; i < count; ++i)
        {
            const unsigned ytemp = (yfrac >> 10) & 0x0FC0;
            const unsigned xtemp = (xfrac >> 16) & 0x3F;
            const int spot = xtemp | ytemp;

            const byte source = sourcebase[spot];

            pixel_t *restrict dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = brightmap[source] ? colormap1[source] : colormap0[source];

            dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = brightmap[source] ? colormap1[source] : colormap0[source];

            xfrac += xstep;
            yfrac += ystep;
        }
    }

    // Store back updated fractional values
    ds_xfrac = xfrac;
    ds_yfrac = yfrac;
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
    const byte *src = W_CacheLumpName(DEH_String(gamemode == commercial ? "GRNROCK" : "FLOOR7_2"), PU_CACHE); 
    pixel_t *dest = background_buffer;

    // [PN] Pre-cache patches for border drawing
    patch_t *patch_top = W_CacheLumpName(DEH_String("brdr_t"), PU_CACHE);
    patch_t *patch_bottom = W_CacheLumpName(DEH_String("brdr_b"), PU_CACHE);
    patch_t *patch_left = W_CacheLumpName(DEH_String("brdr_l"), PU_CACHE);
    patch_t *patch_right = W_CacheLumpName(DEH_String("brdr_r"), PU_CACHE);
    patch_t *patch_tl = W_CacheLumpName(DEH_String("brdr_tl"), PU_CACHE);
    patch_t *patch_tr = W_CacheLumpName(DEH_String("brdr_tr"), PU_CACHE);
    patch_t *patch_bl = W_CacheLumpName(DEH_String("brdr_bl"), PU_CACHE);
    patch_t *patch_br = W_CacheLumpName(DEH_String("brdr_br"), PU_CACHE);

    // [PN] Precompute commonly used values
    const int viewx = viewwindowx / vid_resolution;
    const int viewy = viewwindowy / vid_resolution;
    const int scaledwidth = scaledviewwidth / vid_resolution;
    const int viewheight_res = viewheight / vid_resolution;

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
    if (scaledviewwidth == SCREENWIDTH || background_buffer == NULL)
    {
        return;
    }

    // [PN] Precompute common values
    const int top = ((SCREENHEIGHT - SBARHEIGHT) - viewheight) / 2;
    const int side = (SCREENWIDTH - scaledviewwidth) / 2;
    const int top_offset = top * SCREENWIDTH + side;
    const int bottom_offset = (viewheight + top) * SCREENWIDTH - side;

    // [PN] Copy top and bottom sections
    R_VideoErase(0, top_offset);
    R_VideoErase(bottom_offset, top_offset);

    // [PN] Precompute for sides
    int ofs = top * SCREENWIDTH + SCREENWIDTH - side;
    const int doubled_side = side << 1;

    // [PN] Copy sides
    for (int i = 1; i < viewheight; i++) 
    { 
        R_VideoErase(ofs, doubled_side);
        ofs += SCREENWIDTH;
    }

    // [PN] Mark the entire screen for refresh
    V_MarkRect(0, 0, SCREENWIDTH, SCREENHEIGHT - SBARHEIGHT);
}
