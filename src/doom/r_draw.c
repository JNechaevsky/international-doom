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
// [PN] Optimized to use local pointers for global arrays, replaced
// do/while with for loops, and simplified arithmetic operations.
// -----------------------------------------------------------------------------

void R_DrawFuzzColumn(void)
{
    int count;
    pixel_t *dest;
    boolean cutoff = (dc_yh == viewheight - 1); // [crispy]
    int i;
    const int *fuzzoffsetbase; // [PN] local pointer to fuzzoffset
    int local_fuzzpos = fuzzpos; // [PN] local copy of fuzzpos for potential optimization

    // [PN] Adjust borders
    if (!dc_yl)
        dc_yl = 1;

    if (cutoff)
        dc_yh = viewheight - 2;

    count = dc_yh - dc_yl;

    // [PN] Zero length check
    if (count < 0)
    {
        // [PN] restore fuzzpos before return if needed
        fuzzpos = local_fuzzpos;
        return;
    }

    // [PN] Destination calculation
    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    fuzzoffsetbase = fuzzoffset;

    // [PN] Use a for loop for clarity and potential optimizations
    {
        int iterations = count + 1; // [PN] since do/while decrements count after use
        for (i = 0; i < iterations; i++)
        {
            int fuzz_offset = SCREENWIDTH * fuzzoffsetbase[local_fuzzpos];

            *dest = I_BlendDark(dest[fuzz_offset], FUZZ_ALPHA);

            // [PN] Update fuzzpos
            local_fuzzpos = (local_fuzzpos + 1) % FUZZTABLE;
            if (local_fuzzpos == 0 && vis_improved_fuzz == 1)
            {
                local_fuzzpos = (realleveltime > oldleveltime) ? ID_Random() % 49 : 0;
            }

            dest += SCREENWIDTH;
        }
    }

    // [PN] handle cutoff line
    if (cutoff)
    {
        int fuzz_offset = SCREENWIDTH * (fuzzoffsetbase[local_fuzzpos] - FUZZOFF) / 2;

        *dest = I_BlendDark(dest[fuzz_offset], FUZZ_ALPHA);
    }

    // [PN] restore fuzzpos
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
    int count;
    pixel_t *dest, *dest2;
    int x;
    boolean cutoff = (dc_yh == viewheight - 1);
    int i;
    const int *fuzzoffsetbase;
    int local_fuzzpos = fuzzpos; // [PN] local copy for optimization

    // [PN] Adjust borders
    if (!dc_yl)
        dc_yl = 1;

    if (cutoff)
        dc_yh = viewheight - 2;

    count = dc_yh - dc_yl;
    if (count < 0)
    {
        fuzzpos = local_fuzzpos; // [PN] restore fuzzpos before return if needed
        return;
    }

    // [PN] Low detail mode: multiply x by 2
    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    // [PN] Local pointer to fuzzoffset
    fuzzoffsetbase = fuzzoffset;

    {
        int iterations = count + 1; // [PN] do/while decrement after use, so we run count+1 times
        for (i = 0; i < iterations; i++)
        {
            int fuzz_offset = SCREENWIDTH * fuzzoffsetbase[local_fuzzpos];

            *dest = I_BlendDark(dest[fuzz_offset], FUZZ_ALPHA);
            *dest2 = I_BlendDark(dest2[fuzz_offset], FUZZ_ALPHA);

            // [PN] Update fuzzpos
            local_fuzzpos = (local_fuzzpos + 1) % FUZZTABLE;
            if (local_fuzzpos == 0 && vis_improved_fuzz)
            {
                local_fuzzpos = (realleveltime > oldleveltime) ? ID_Random() % 49 : 0;
            }

            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;
        }
    }

    // [PN] Handle cutoff line
    if (cutoff)
    {
        int fuzz_offset = SCREENWIDTH * (fuzzoffsetbase[local_fuzzpos] - FUZZOFF) / 2;

        *dest = I_BlendDark(dest[fuzz_offset], FUZZ_ALPHA);
        *dest2 = I_BlendDark(dest2[fuzz_offset], FUZZ_ALPHA);
    }

    // [PN] Restore fuzzpos
    fuzzpos = local_fuzzpos;
}

// -----------------------------------------------------------------------------
// R_DrawFuzzTLColumn
// [PN] Draw translucent column for fuzz effect, overlay blending. High detail.
// -----------------------------------------------------------------------------

void R_DrawFuzzTLColumn(void)
{
    int count = dc_yh - dc_yl;
    pixel_t *dest;
    fixed_t frac;
    fixed_t fracstep;
    int i;
    const byte *sourcebase;
    const pixel_t *colormap0;

    if (count < 0)
        return;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers for global arrays
    sourcebase = dc_source;
    colormap0 = dc_colormap[0];

    {
        int iterations = count + 1; // converting do/while to for
        for (i = 0; i < iterations; i++)
        {
            {
                pixel_t destrgb;
                unsigned s = sourcebase[frac >> FRACBITS];
                destrgb = colormap0[s];
                *dest = I_BlendOver(*dest, destrgb, FUZZTL_ALPHA);
            }

            dest += SCREENWIDTH;
            frac += fracstep;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawFuzzTLColumnLow
// [PN] Draw translucent column for fuzz effect, overlay blending. Low detail.
// -----------------------------------------------------------------------------

void R_DrawFuzzTLColumnLow(void)
{
    int count = dc_yh - dc_yl;
    int x;
    pixel_t *dest, *dest2;
    fixed_t frac, fracstep;
    int i;
    const byte *sourcebase;
    const pixel_t *colormap0;

    if (count < 0)
        return;

    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers for arrays
    sourcebase = dc_source;
    colormap0 = dc_colormap[0];

    {
        int iterations = count + 1;
        for (i = 0; i < iterations; i++)
        {
            unsigned s = sourcebase[frac >> FRACBITS];
            pixel_t sourcecolor = colormap0[s];

            {
                pixel_t destrgb = sourcecolor;
                *dest = I_BlendOver(*dest, destrgb, FUZZTL_ALPHA);
                *dest2 = I_BlendOver(*dest2, destrgb, FUZZTL_ALPHA);
            }

            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;
            frac += fracstep;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawFuzzBWColumn
// [PN] Draw grayscale fuzz columnn. High detail.
// -----------------------------------------------------------------------------

void R_DrawFuzzBWColumn(void)
{
    int count;
    pixel_t *dest;
    boolean cutoff = (dc_yh == viewheight - 1);
    int i;
    const int *fuzzoffsetbase;
    int local_fuzzpos = fuzzpos; // [PN] local copy of fuzzpos

    // [PN] Adjust borders
    if (!dc_yl)
        dc_yl = 1;

    if (cutoff)
        dc_yh = viewheight - 2;

    count = dc_yh - dc_yl;
    if (count < 0)
    {
        // [PN] restore fuzzpos before return
        fuzzpos = local_fuzzpos;
        return;
    }

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];

    // [PN] Local pointer for fuzzoffset
    fuzzoffsetbase = fuzzoffset;

    {
        int iterations = count + 1; // [PN] do/while pattern
        for (i = 0; i < iterations; i++)
        {
            int fuzz_offset = SCREENWIDTH * fuzzoffsetbase[local_fuzzpos];

            *dest = I_BlendDarkGrayscale(dest[fuzz_offset], FUZZ_ALPHA);

            // [PN] Update fuzzpos
            local_fuzzpos = (local_fuzzpos + 1) % FUZZTABLE;

            dest += SCREENWIDTH;
        }
    }

    // [PN] Handle cutoff line
    if (cutoff)
    {
        int fuzz_offset = SCREENWIDTH * (fuzzoffsetbase[local_fuzzpos] - FUZZOFF) / 2;
        *dest = I_BlendDarkGrayscale(dest[fuzz_offset], FUZZ_ALPHA);
    }

    // [PN] Restore fuzzpos
    fuzzpos = local_fuzzpos;
}

// -----------------------------------------------------------------------------
// R_DrawFuzzBWColumnLow
// [PN] Draw grayscale fuzz columnn. Low detail.
// -----------------------------------------------------------------------------

void R_DrawFuzzBWColumnLow(void)
{
    int count;
    pixel_t *dest, *dest2;
    int x;
    boolean cutoff = (dc_yh == viewheight - 1);
    int i;
    const int *fuzzoffsetbase;
    int local_fuzzpos = fuzzpos; // [PN] local copy of fuzzpos

    // [PN] Adjust borders
    if (!dc_yl)
        dc_yl = 1;

    if (cutoff)
        dc_yh = viewheight - 2;

    count = dc_yh - dc_yl;
    if (count < 0)
    {
        fuzzpos = local_fuzzpos; // [PN] restore fuzzpos before return
        return;
    }

    // [PN] Low detail mode: multiply x by 2
    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    // [PN] Local pointer to fuzzoffset
    fuzzoffsetbase = fuzzoffset;

    {
        int iterations = count + 1; // [PN] converting do/while to for
        for (i = 0; i < iterations; i++)
        {
            int fuzz_offset = SCREENWIDTH * fuzzoffsetbase[local_fuzzpos];

            *dest = I_BlendDarkGrayscale(dest[fuzz_offset], FUZZ_ALPHA);
            *dest2 = I_BlendDarkGrayscale(dest2[fuzz_offset], FUZZ_ALPHA);

            // [PN] Update fuzzpos
            local_fuzzpos = (local_fuzzpos + 1) % FUZZTABLE;

            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;
        }
    }

    // [PN] Handle cutoff line
    if (cutoff)
    {
        int fuzz_offset = SCREENWIDTH * (fuzzoffsetbase[local_fuzzpos] - FUZZOFF) / 2;

        *dest = I_BlendDarkGrayscale(dest[fuzz_offset], FUZZ_ALPHA);
        *dest2 = I_BlendDarkGrayscale(dest2[fuzz_offset], FUZZ_ALPHA);
    }

    // [PN] Restore fuzzpos
    fuzzpos = local_fuzzpos;
}

// -----------------------------------------------------------------------------
// R_DrawTransTLFuzzColumn
// [JN] Translucent, translated fuzz column.
// -----------------------------------------------------------------------------

void R_DrawTransTLFuzzColumn(void)
{
    int count = dc_yh - dc_yl;
    pixel_t *dest;
    fixed_t frac;
    fixed_t fracstep;
    int i;
    const byte *sourcebase;
    const byte *translation;
    const pixel_t *colormap0;

    if (count < 0)
        return;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[dc_x]];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers for global arrays
    sourcebase = dc_source;
    translation = dc_translation;
    colormap0 = dc_colormap[0];

    {
        int iterations = count + 1; // converting do/while to for
        for (i = 0; i < iterations; i++)
        {
            unsigned s;
            unsigned t;
            pixel_t destrgb;

            s = sourcebase[frac >> FRACBITS];
            t = translation[s];
            destrgb = colormap0[t];

            *dest = I_BlendOver(*dest, destrgb, FUZZTL_ALPHA);

            dest += SCREENWIDTH;
            frac += fracstep;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawTransTLFuzzColumnLow
// [JN] Translucent, translated fuzz column, low-resolution version.
// -----------------------------------------------------------------------------

void R_DrawTransTLFuzzColumnLow(void)
{
    int count = dc_yh - dc_yl;
    int x;
    pixel_t *dest, *dest2;
    fixed_t frac, fracstep;
    int i;
    const byte *sourcebase;
    const byte *translation;
    const pixel_t *colormap0;

    if (count < 0)
        return;

    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers for global arrays
    sourcebase = dc_source;
    translation = dc_translation;
    colormap0 = dc_colormap[0];

    {
        int iterations = count + 1; // converting do/while to for
        for (i = 0; i < iterations; i++)
        {
            unsigned s = sourcebase[frac >> FRACBITS];
            pixel_t destrgb = colormap0[translation[s]];

            *dest = I_BlendOver(*dest, destrgb, FUZZTL_ALPHA);
            *dest2 = I_BlendOver(*dest2, destrgb, FUZZTL_ALPHA);

            dest += SCREENWIDTH;
            dest2 += SCREENWIDTH;
            frac += fracstep;
        }
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

void R_DrawTranslatedColumnLow(void)
{
    int count;
    pixel_t *dest, *dest2;
    fixed_t frac, fracstep;
    int x;
    int i;
    const byte *sourcebase;
    const byte *brightmap;
    const byte *translation;
    const pixel_t *colormap0;
    const pixel_t *colormap1;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

    // [PN] Low detail mode: multiply x by 2
    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x + 1]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Local pointers to global arrays
    sourcebase = dc_source;
    brightmap = dc_brightmap;
    translation = dc_translation;
    colormap0 = dc_colormap[0];
    colormap1 = dc_colormap[1];

    {
        int iterations = count + 1; // since do/while decrements count after use
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
            *dest = I_BlendOver(*dest, destrgb, TRANMAP_ALPHA);
        }

        dest += SCREENWIDTH;
        frac += fracstep;
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLColumn
// [PN] Draw translucent column, overlay blending. Low detail.
// -----------------------------------------------------------------------------

void R_DrawTLColumnLow(void)
{
    int count, x, i;
    pixel_t *dest, *dest2;
    fixed_t frac, fracstep;
    const byte *sourcebase;
    const byte *brightmap;
    const pixel_t *colormap0;
    const pixel_t *colormap1;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

    x = dc_x << 1;

    dest = ylookup[dc_yl] + columnofs[flipviewwidth[x]];
    dest2 = ylookup[dc_yl] + columnofs[flipviewwidth[x+1]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // [PN] Using local pointers to reduce global lookups
    sourcebase = dc_source;
    brightmap = dc_brightmap;

    // [PN] dc_colormap is two pointers to arrays of pixel_t[256]
    colormap0 = dc_colormap[0];
    colormap1 = dc_colormap[1];

    // [PN] Using a for loop for clarity and potential compiler optimization
    for (i = 0; i <= count; i++)
    {
        unsigned s;

        s = sourcebase[frac >> FRACBITS];

        {
            pixel_t destrgb;
            // [PN] Choose colormap based on brightmap[s] and blend using I_BlendOver
            destrgb = (brightmap[s] ? colormap1[s] : colormap0[s]);
            *dest = I_BlendOver(*dest, destrgb, TRANMAP_ALPHA);
            *dest2 = I_BlendOver(*dest2, destrgb, TRANMAP_ALPHA);
        }

        dest += SCREENWIDTH;
        dest2 += SCREENWIDTH;
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
    pixel_t *dest;
    int count;
    int spot;
    unsigned int xtemp, ytemp;
    byte source;
    const byte *sourcebase;
    const byte *brightmap;
    const pixel_t *colormap0;
    const pixel_t *colormap1;

    // [PN] Calculate the span length
    count = ds_x2 - ds_x1 + 1;

    // [PN] Local pointers for global arrays
    sourcebase = ds_source;
    brightmap = ds_brightmap;
    colormap0 = ds_colormap[0];
    colormap1 = ds_colormap[1];

    // [PN] Check if level is not flipped
    if (!gp_flip_levels)
    {
        // [PN] Precompute destination pointer
        dest = ylookup[ds_y] + columnofs[ds_x1];

        // [PN] Process in chunks of four pixels
        // Use a for loop for the unrolled section
        for (; count >= 4; count -= 4)
        {
            // 1st pixel
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[0] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // 2nd pixel
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[1] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // 3rd pixel
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[2] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // 4th pixel
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[3] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            dest += 4;
        }

        // [PN] Process remaining pixels individually
        for (; count > 0; count--)
        {
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;

            source = sourcebase[spot];
            *dest = (brightmap[source] ? colormap1[source] : colormap0[source]);

            dest++;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;
        }
    }
    else
    {
        // [PN] Flipped levels
        // Convert do/while(count--) into a for loop
        {
            int i;
            int iterations = count;
            for (i = 0; i < iterations; i++)
            {
                ytemp = (ds_yfrac >> 10) & 0x0fc0;
                xtemp = (ds_xfrac >> 16) & 0x3f;
                spot = xtemp | ytemp;

                source = sourcebase[spot];
                // [PN] Recalculate destination pointer each time due to flip
                dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
                *dest = (brightmap[source] ? colormap1[source] : colormap0[source]);

                ds_xfrac += ds_xstep;
                ds_yfrac += ds_ystep;
            }
        }
    }
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
    unsigned int xtemp, ytemp;
    pixel_t *dest;
    int count;
    int spot;
    byte source;
    const byte *sourcebase;
    const byte *brightmap;
    const pixel_t *colormap0;
    const pixel_t *colormap1;
    int i, iterations;

    // [PN] Calculate the span length.
    count = ds_x2 - ds_x1 + 1;

    // [PN] Blocky mode: multiply by 2
    ds_x1 <<= 1;
    ds_x2 <<= 1;

    // [PN] Local pointers to global arrays
    sourcebase = ds_source;
    brightmap = ds_brightmap;
    colormap0 = ds_colormap[0];
    colormap1 = ds_colormap[1];

    if (!gp_flip_levels)
    {
        // [PN] Precompute dest pointer
        dest = ylookup[ds_y] + columnofs[ds_x1];

        // [PN] Loop unrolled by four sets of TWO pixels each iteration
        // Each iteration draws 4 * 2 = 8 pixels in total (2 pixels per iteration block)
        // Actually it's 4 iterations, each iteration draws 2 pixels => 8 pixels total per 4-iteration block.
        
        while (count >= 4)
        {
            // First pair
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[0] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest[1] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Second pair
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[0] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest[1] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Third pair
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[0] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest[1] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            // Fourth pair
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;
            source = sourcebase[spot];
            dest[0] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest[1] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest += 2;
            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;

            count -= 4;
        }

        // [PN] Render remaining pixels one by one if any
        while (count-- > 0)
        {
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;

            source = sourcebase[spot];
            dest[0] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest[1] = (brightmap[source] ? colormap1[source] : colormap0[source]);
            dest += 2;

            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;
        }
    }
    else
    {
        // [PN] Flipped levels in blocky mode
        // do/while(count--) into a for loop
        iterations = count;
        for (i = 0; i < iterations; i++)
        {
            ytemp = (ds_yfrac >> 10) & 0x0fc0;
            xtemp = (ds_xfrac >> 16) & 0x3f;
            spot = xtemp | ytemp;

            source = sourcebase[spot];

            // [PN] Draw first pixel
            dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = (brightmap[source] ? colormap1[source] : colormap0[source]);

            // [PN] Draw second pixel
            dest = ylookup[ds_y] + columnofs[flipviewwidth[ds_x1++]];
            *dest = (brightmap[source] ? colormap1[source] : colormap0[source]);

            ds_xfrac += ds_xstep;
            ds_yfrac += ds_ystep;
        }
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
