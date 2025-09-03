//
// Copyright(C) 2025 Polina "Aura" N.
// Copyright(C) 2025 Julia Nechaevskaya
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
#include "r_local.h"
#include "v_video.h"

#include "id_vars.h"


// -----------------------------------------------------------------------------
// R_DrawTLColumn
// [PN/JN] Translucent column, overlay blending.
// -----------------------------------------------------------------------------

void R_DrawTLColumn (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Local pointers for improved memory access
    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const brightmap    = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    if (vid_resolution > 1) // Duplicate pixels vertically for performance
    {
        const int step = 2;
        const fixed_t fracstep = dc_iscale * step;

        // Compute one pixel, write it to two vertical lines
        while (y_start < y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendOver_96(bg, fg)
                                                    : I_BlendOver_96_8(bg, fg);

            // Write two pixels (current and next line)
            dest[0] = blended;
            dest[screenwidth] = blended;

            // Move to next pair
            dest    += screenwidth * step;
            frac    += fracstep;
            y_start += step;
        }

        // Handle final odd line
        if (y_start == y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendOver_96(bg, fg)
                                                    : I_BlendOver_96_8(bg, fg);

            dest[0] = blended;
        }
    }
    else // No vertical duplication in 1× mode
    {
        const fixed_t fracstep = dc_iscale;

        while (y_start <= y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendOver_96(bg, fg)
                                                    : I_BlendOver_96_8(bg, fg);

            *dest = blended;
            dest += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}

void R_DrawTLColumnLow (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Low detail: double horizontal resolution
    const int x = dc_x << 1;

    // Local pointers for improved memory access
    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const brightmap    = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    if (vid_resolution > 1) // Duplicate pixels vertically for performance
    {
        const int step = 2;
        const fixed_t fracstep = dc_iscale * step;

        // Compute one pixel, write it to two vertical lines
        while (y_start < y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_96(bg1, fg) :
                                                       I_BlendOver_96_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_96(bg2, fg) :
                                                       I_BlendOver_96_8(bg2, fg);
            // Process two lines for both columns
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

        // Handle final odd line
        if (y_start == y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_96(bg1, fg)
                                                     : I_BlendOver_96_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_96(bg2, fg)
                                                     : I_BlendOver_96_8(bg2, fg);

            dest1[0] = blended1;
            dest2[0] = blended2;
        }
    }
    else // No vertical duplication in 1× mode
    {
        const fixed_t fracstep = dc_iscale;

        while (y_start <= y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_96(bg1, fg)
                                                     : I_BlendOver_96_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_96(bg2, fg)
                                                     : I_BlendOver_96_8(bg2, fg);

            *dest1 = blended1;
            *dest2 = blended2;
            dest1 += screenwidth;
            dest2 += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLColumn
// [PN/JN] Alternative translucent column, overlay blending.
// -----------------------------------------------------------------------------

void R_DrawAltTLColumn (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Local pointers for improved memory access
    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const brightmap    = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    if (vid_resolution > 1) // Duplicate pixels vertically for performance
    {
        const int step = 2;
        const fixed_t fracstep = dc_iscale * step;

        // Compute one pixel, write it to two vertical lines
        while (y_start < y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendOver_142(bg, fg)
                                                    : I_BlendOver_142_8(bg, fg);

            // Write two pixels (current and next line)
            dest[0] = blended;
            dest[screenwidth] = blended;

            // Move to next pair
            dest    += screenwidth * step;
            frac    += fracstep;
            y_start += step;
        }

        // Handle final odd line
        if (y_start == y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendOver_142(bg, fg)
                                                    : I_BlendOver_142_8(bg, fg);

            dest[0] = blended;
        }
    }
    else // No vertical duplication in 1× mode
    {
        const fixed_t fracstep = dc_iscale;

        while (y_start <= y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendOver_142(bg, fg)
                                                    : I_BlendOver_142_8(bg, fg);

            *dest = blended;
            dest += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}

void R_DrawAltTLColumnLow (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Low detail: double horizontal resolution
    const int x = dc_x << 1;

    // Local pointers for improved memory access
    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const brightmap    = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    if (vid_resolution > 1) // Duplicate pixels vertically for performance
    {
        const int step = 2;
        const fixed_t fracstep = dc_iscale * step;

        // Compute one pixel, write it to two vertical lines
        while (y_start < y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_142(bg1, fg) :
                                                       I_BlendOver_142_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_142(bg2, fg) :
                                                       I_BlendOver_142_8(bg2, fg);
            // Process two lines for both columns
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

        // Handle final odd line
        if (y_start == y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_142(bg1, fg)
                                                     : I_BlendOver_142_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_142(bg2, fg)
                                                     : I_BlendOver_142_8(bg2, fg);

            dest1[0] = blended1;
            dest2[0] = blended2;
        }
    }
    else // No vertical duplication in 1× mode
    {
        const fixed_t fracstep = dc_iscale;

        while (y_start <= y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_142(bg1, fg)
                                                     : I_BlendOver_142_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_142(bg2, fg)
                                                     : I_BlendOver_142_8(bg2, fg);

            *dest1 = blended1;
            *dest2 = blended2;
            dest1 += screenwidth;
            dest2 += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLAddColumn
// [PN/JN] Translucent column, additive blending.
// -----------------------------------------------------------------------------

void R_DrawTLAddColumn (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Local pointers for improved memory access
    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const brightmap    = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    if (vid_resolution > 1) // Duplicate pixels vertically for performance
    {
        const int step = 2;
        const fixed_t fracstep = dc_iscale * step;

        // Compute one pixel, write it to two vertical lines
        while (y_start < y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendAdd(bg, fg)
                                                    : I_BlendAdd_8(bg, fg);

            // Write two pixels (current and next line)
            dest[0] = blended;
            dest[screenwidth] = blended;

            // Move to next pair
            dest    += screenwidth * step;
            frac    += fracstep;
            y_start += step;
        }

        // Handle final odd line
        if (y_start == y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendAdd(bg, fg)
                                                    : I_BlendAdd_8(bg, fg);

            dest[0] = blended;
        }
    }
    else // No vertical duplication in 1× mode
    {
        const fixed_t fracstep = dc_iscale;

        while (y_start <= y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendAdd(bg, fg)
                                                    : I_BlendAdd_8(bg, fg);

            *dest = blended;
            dest += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}

void R_DrawTLAddColumnLow (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Low detail: double horizontal resolution
    const int x = dc_x << 1;

    // Local pointers for improved memory access
    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const brightmap    = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    if (vid_resolution > 1) // Duplicate pixels vertically for performance
    {
        const int step = 2;
        const fixed_t fracstep = dc_iscale * step;

        // Compute one pixel, write it to two vertical lines
        while (y_start < y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendAdd(bg1, fg) :
                                                       I_BlendAdd_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendAdd(bg2, fg) :
                                                       I_BlendAdd_8(bg2, fg);
            // Process two lines for both columns
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

        // Handle final odd line
        if (y_start == y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendAdd(bg1, fg)
                                                     : I_BlendAdd_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendAdd(bg2, fg)
                                                     : I_BlendAdd_8(bg2, fg);

            dest1[0] = blended1;
            dest2[0] = blended2;
        }
    }
    else // No vertical duplication in 1× mode
    {
        const fixed_t fracstep = dc_iscale;

        while (y_start <= y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendAdd(bg1, fg)
                                                     : I_BlendAdd_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendAdd(bg2, fg)
                                                     : I_BlendAdd_8(bg2, fg);

            *dest1 = blended1;
            *dest2 = blended2;
            dest1 += screenwidth;
            dest2 += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawTranslatedTLColumn
// [PN/JN] Color translucent, translucent column.
// -----------------------------------------------------------------------------

void R_DrawTranslatedTLColumn (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Local pointers for improved memory access
    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const translation  = dc_translation;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    if (vid_resolution > 1) // Duplicate pixels vertically for performance
    {
        const int step = 2;
        const fixed_t fracstep = dc_iscale * step;

        // Compute one pixel, write it to two vertical lines
        while (y_start < y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = colormap0[translation[s]];
            const pixel_t blended = truecolor_blend ? I_BlendOver_96(bg, fg)
                                                    : I_BlendOver_96_8(bg, fg);

            // Write two pixels (current and next line)
            dest[0] = blended;
            dest[screenwidth] = blended;

            // Move to next pair
            dest    += screenwidth * step;
            frac    += fracstep;
            y_start += step;
        }

        // Handle final odd line
        if (y_start == y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = colormap0[translation[s]];
            const pixel_t blended = truecolor_blend ? I_BlendOver_96(bg, fg)
                                                    : I_BlendOver_96_8(bg, fg);

            dest[0] = blended;
        }
    }
    else // No vertical duplication in 1× mode
    {
        const fixed_t fracstep = dc_iscale;

        while (y_start <= y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = colormap0[translation[s]];
            const pixel_t blended = truecolor_blend ? I_BlendOver_96(bg, fg)
                                                    : I_BlendOver_96_8(bg, fg);

            *dest = blended;
            dest += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}

void R_DrawTranslatedTLColumnLow (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Low detail: double horizontal resolution
    const int x = dc_x << 1;

    // Local pointers for improved memory access
    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const translation  = dc_translation;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    if (vid_resolution > 1) // Duplicate pixels vertically for performance
    {
        const int step = 2;
        const fixed_t fracstep = dc_iscale * step;

        // Compute one pixel, write it to two vertical lines
        while (y_start < y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = colormap0[translation[s]];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_96(bg1, fg) :
                                                       I_BlendOver_96_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_96(bg2, fg) :
                                                       I_BlendOver_96_8(bg2, fg);
            // Process two lines for both columns
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

        // Handle final odd line
        if (y_start == y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = colormap0[translation[s]];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_96(bg1, fg)
                                                     : I_BlendOver_96_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_96(bg2, fg)
                                                     : I_BlendOver_96_8(bg2, fg);

            dest1[0] = blended1;
            dest2[0] = blended2;
        }
    }
    else // No vertical duplication in 1× mode
    {
        const fixed_t fracstep = dc_iscale;

        while (y_start <= y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = colormap0[translation[s]];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_96(bg1, fg)
                                                     : I_BlendOver_96_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_96(bg2, fg)
                                                     : I_BlendOver_96_8(bg2, fg);

            *dest1 = blended1;
            *dest2 = blended2;
            dest1 += screenwidth;
            dest2 += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLColumn
// [PN/JN] Extra translucent column, overlay blending.
// -----------------------------------------------------------------------------

void R_DrawExtraTLColumn (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Local pointers for improved memory access
    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const brightmap    = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    if (vid_resolution > 1) // Duplicate pixels vertically for performance
    {
        const int step = 2;
        const fixed_t fracstep = dc_iscale * step;

        // Compute one pixel, write it to two vertical lines
        while (y_start < y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendOver_142(bg, fg)
                                                    : I_BlendOver_142_8(bg, fg);

            // Write two pixels (current and next line)
            dest[0] = blended;
            dest[screenwidth] = blended;

            // Move to next pair
            dest    += screenwidth * step;
            frac    += fracstep;
            y_start += step;
        }

        // Handle final odd line
        if (y_start == y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendOver_142(bg, fg)
                                                    : I_BlendOver_142_8(bg, fg);

            dest[0] = blended;
        }
    }
    else // No vertical duplication in 1× mode
    {
        const fixed_t fracstep = dc_iscale;

        while (y_start <= y_end)
        {
            const unsigned s = sourcebase[frac >> FRACBITS];
            const pixel_t bg = *dest;
            const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended = truecolor_blend ? I_BlendOver_142(bg, fg)
                                                    : I_BlendOver_142_8(bg, fg);

            *dest = blended;
            dest += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}

void R_DrawExtraTLColumnLow (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Low detail: double horizontal resolution
    const int x = dc_x << 1;

    // Local pointers for improved memory access
    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const brightmap    = dc_brightmap;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const pixel_t *restrict const colormap1 = dc_colormap[1];
    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    if (vid_resolution > 1) // Duplicate pixels vertically for performance
    {
        const int step = 2;
        const fixed_t fracstep = dc_iscale * step;

        // Compute one pixel, write it to two vertical lines
        while (y_start < y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_142(bg1, fg) :
                                                       I_BlendOver_142_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_142(bg2, fg) :
                                                       I_BlendOver_142_8(bg2, fg);
            // Process two lines for both columns
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

        // Handle final odd line
        if (y_start == y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_142(bg1, fg)
                                                     : I_BlendOver_142_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_142(bg2, fg)
                                                     : I_BlendOver_142_8(bg2, fg);

            dest1[0] = blended1;
            dest2[0] = blended2;
        }
    }
    else // No vertical duplication in 1× mode
    {
        const fixed_t fracstep = dc_iscale;

        while (y_start <= y_end)
        {
            const unsigned s  = sourcebase[frac >> FRACBITS];
            const pixel_t bg1 = *dest1;
            const pixel_t bg2 = *dest2;
            const pixel_t fg  = brightmap[s] ? colormap1[s] : colormap0[s];
            const pixel_t blended1 = truecolor_blend ? I_BlendOver_142(bg1, fg)
                                                     : I_BlendOver_142_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver_142(bg2, fg)
                                                     : I_BlendOver_142_8(bg2, fg);

            *dest1 = blended1;
            *dest2 = blended2;
            dest1 += screenwidth;
            dest2 += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}
