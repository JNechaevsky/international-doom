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
#include "doomdef.h"
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
            const pixel_t blended = truecolor_blend ? I_BlendOver168_32(bg, fg)
                                                    : I_BlendOver168_8(bg, fg);

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
            const pixel_t blended = truecolor_blend ? I_BlendOver168_32(bg, fg)
                                                    : I_BlendOver168_8(bg, fg);

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
            const pixel_t blended = truecolor_blend ? I_BlendOver168_32(bg, fg)
                                                    : I_BlendOver168_8(bg, fg);

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
            const pixel_t blended1 = truecolor_blend ? I_BlendOver168_32(bg1, fg) :
                                                       I_BlendOver168_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver168_32(bg2, fg) :
                                                       I_BlendOver168_8(bg2, fg);
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
            const pixel_t blended1 = truecolor_blend ? I_BlendOver168_32(bg1, fg)
                                                     : I_BlendOver168_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver168_32(bg2, fg)
                                                     : I_BlendOver168_8(bg2, fg);

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
            const pixel_t blended1 = truecolor_blend ? I_BlendOver168_32(bg1, fg)
                                                     : I_BlendOver168_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver168_32(bg2, fg)
                                                     : I_BlendOver168_8(bg2, fg);

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
// R_DrawDarkColumn
// [JN] Darken destination pixels using I_BlendDark, preserving sprite mask.
// -----------------------------------------------------------------------------

void R_DrawDarkColumn (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    const int dark_amount = shadow_alpha;
    int y_start = dc_yl;
    int y_end = dc_yh;
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    if (vid_resolution > 1)
    {
        const int step = 2;

        while (y_start < y_end)
        {
            const pixel_t blended = truecolor_blend ? I_BlendDark_32(*dest, dark_amount)
                                                    : I_BlendDark_8(*dest, dark_amount);

            dest[0] = blended;
            dest[screenwidth] = blended;

            dest += screenwidth * step;
            y_start += step;
        }

        if (y_start == y_end)
        {
            dest[0] = truecolor_blend ? I_BlendDark_32(*dest, dark_amount)
                                      : I_BlendDark_8(*dest, dark_amount);
        }
    }
    else
    {
        while (y_start <= y_end)
        {
            *dest = truecolor_blend ? I_BlendDark_32(*dest, dark_amount)
                                    : I_BlendDark_8(*dest, dark_amount);
            dest += screenwidth;
            ++y_start;
        }
    }
}

void R_DrawDarkColumnLow (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    const int x = dc_x << 1;
    const int screenwidth = SCREENWIDTH;
    const int truecolor_blend = vid_truecolor;
    const int dark_amount = shadow_alpha;
    int y_start = dc_yl;
    int y_end = dc_yh;
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    if (vid_resolution > 1)
    {
        const int step = 2;

        while (y_start < y_end)
        {
            const pixel_t blended1 = truecolor_blend ? I_BlendDark_32(*dest1, dark_amount)
                                                     : I_BlendDark_8(*dest1, dark_amount);
            const pixel_t blended2 = truecolor_blend ? I_BlendDark_32(*dest2, dark_amount)
                                                     : I_BlendDark_8(*dest2, dark_amount);

            dest1[0] = blended1;
            dest1[screenwidth] = blended1;
            dest2[0] = blended2;
            dest2[screenwidth] = blended2;

            dest1 += screenwidth * step;
            dest2 += screenwidth * step;
            y_start += step;
        }

        if (y_start == y_end)
        {
            dest1[0] = truecolor_blend ? I_BlendDark_32(*dest1, dark_amount)
                                       : I_BlendDark_8(*dest1, dark_amount);
            dest2[0] = truecolor_blend ? I_BlendDark_32(*dest2, dark_amount)
                                       : I_BlendDark_8(*dest2, dark_amount);
        }
    }
    else
    {
        while (y_start <= y_end)
        {
            *dest1 = truecolor_blend ? I_BlendDark_32(*dest1, dark_amount)
                                     : I_BlendDark_8(*dest1, dark_amount);
            *dest2 = truecolor_blend ? I_BlendDark_32(*dest2, dark_amount)
                                     : I_BlendDark_8(*dest2, dark_amount);
            dest1 += screenwidth;
            dest2 += screenwidth;
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
            const pixel_t blended = truecolor_blend ? I_BlendAdd_32(bg, fg)
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
            const pixel_t blended = truecolor_blend ? I_BlendAdd_32(bg, fg)
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
            const pixel_t blended = truecolor_blend ? I_BlendAdd_32(bg, fg)
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
            const pixel_t blended1 = truecolor_blend ? I_BlendAdd_32(bg1, fg) :
                                                       I_BlendAdd_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendAdd_32(bg2, fg) :
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
            const pixel_t blended1 = truecolor_blend ? I_BlendAdd_32(bg1, fg)
                                                     : I_BlendAdd_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendAdd_32(bg2, fg)
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
            const pixel_t blended1 = truecolor_blend ? I_BlendAdd_32(bg1, fg)
                                                     : I_BlendAdd_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendAdd_32(bg2, fg)
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
// R_DrawFuzzTLColumn
// [PN/JN] Draw translucent column for fuzz effect, overlay blending.
// -----------------------------------------------------------------------------

void R_DrawFuzzTLColumn (void)
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
            const pixel_t blended = truecolor_blend ? I_BlendOver64_32(bg, fg)
                                                    : I_BlendOver64_8(bg, fg);

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
            const pixel_t blended = truecolor_blend ? I_BlendOver64_32(bg, fg)
                                                    : I_BlendOver64_8(bg, fg);

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
            const pixel_t blended = truecolor_blend ? I_BlendOver64_32(bg, fg)
                                                    : I_BlendOver64_8(bg, fg);

            *dest = blended;
            dest += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}

void R_DrawFuzzTLColumnLow (void)
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
            const pixel_t blended1 = truecolor_blend ? I_BlendOver64_32(bg1, fg) :
                                                       I_BlendOver64_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver64_32(bg2, fg) :
                                                       I_BlendOver64_8(bg2, fg);
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
            const pixel_t blended1 = truecolor_blend ? I_BlendOver64_32(bg1, fg)
                                                     : I_BlendOver64_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver64_32(bg2, fg)
                                                     : I_BlendOver64_8(bg2, fg);

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
            const pixel_t blended1 = truecolor_blend ? I_BlendOver64_32(bg1, fg)
                                                     : I_BlendOver64_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver64_32(bg2, fg)
                                                     : I_BlendOver64_8(bg2, fg);

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
// R_DrawFuzzTLTransColumn
// [PN/JN] Translucent, translated fuzz column.
// -----------------------------------------------------------------------------

void R_DrawFuzzTLTransColumn (void)
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
            const pixel_t blended = truecolor_blend ? I_BlendOver64_32(bg, fg)
                                                    : I_BlendOver64_8(bg, fg);

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
            const pixel_t blended = truecolor_blend ? I_BlendOver64_32(bg, fg)
                                                    : I_BlendOver64_8(bg, fg);

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
            const pixel_t blended = truecolor_blend ? I_BlendOver64_32(bg, fg)
                                                    : I_BlendOver64_8(bg, fg);

            *dest = blended;
            dest += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}

void R_DrawFuzzTLTransColumnLow (void)
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
            const pixel_t blended1 = truecolor_blend ? I_BlendOver64_32(bg1, fg) :
                                                       I_BlendOver64_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver64_32(bg2, fg) :
                                                       I_BlendOver64_8(bg2, fg);
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
            const pixel_t blended1 = truecolor_blend ? I_BlendOver64_32(bg1, fg)
                                                     : I_BlendOver64_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver64_32(bg2, fg)
                                                     : I_BlendOver64_8(bg2, fg);

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
            const pixel_t blended1 = truecolor_blend ? I_BlendOver64_32(bg1, fg)
                                                     : I_BlendOver64_8(bg1, fg);
            const pixel_t blended2 = truecolor_blend ? I_BlendOver64_32(bg2, fg)
                                                     : I_BlendOver64_8(bg2, fg);

            *dest1 = blended1;
            *dest2 = blended2;
            dest1 += screenwidth;
            dest2 += screenwidth;
            frac += fracstep;
            ++y_start;
        }
    }
}
