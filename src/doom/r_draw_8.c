//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
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


//
// [PN] Inline helper functions for overlay and additive translucency.
//

static inline pixel_t TLBlendOver (pixel_t fg, pixel_t bg, int a)
{
    const int ia = 256 - a; // 0..256

    // Mix R|B together and G separately (using masks)
    const uint32_t fgRB = fg & 0x00FF00FFu;
    const uint32_t bgRB = bg & 0x00FF00FFu;
    const uint32_t fgG  = fg & 0x0000FF00u;
    const uint32_t bgG  = bg & 0x0000FF00u;

    const uint32_t rb = (((fgRB * a) + (bgRB * ia)) >> 8) & 0x00FF00FFu;
    const uint32_t g8 = (((fgG  * a) + (bgG  * ia)) >> 8) & 0x0000FF00u;

    const int r = (rb >> 16) & 0xFF;
    const int g = (g8 >> 8)  & 0xFF;
    const int b =  rb        & 0xFF;

    // Map into the palette through the 3D LUT
    return pal_color[ RGB_TO_PAL(r, g, b) ];
}

static inline pixel_t TLBlendAdd (pixel_t fg, pixel_t bg)
{
    // Extract channels
    const int fg_r = (fg >> 16) & 0xFF;
    const int fg_g = (fg >> 8)  & 0xFF;
    const int fg_b =  fg        & 0xFF;

    const int bg_r = (bg >> 16) & 0xFF;
    const int bg_g = (bg >> 8)  & 0xFF;
    const int bg_b =  bg        & 0xFF;

    // Fast additive blending via LUT per channel
    const int r = addchan_lut[(bg_r << 8) | fg_r];
    const int g = addchan_lut[(bg_g << 8) | fg_g];
    const int b = addchan_lut[(bg_b << 8) | fg_b];

    // Map to palette (gamma-aware)
    return pal_color[ RGB_TO_PAL(r, g, b) ];
}

// -----------------------------------------------------------------------------
// R_DrawTLColumn_8
// [PN/JN] Translucent column, overlay blending. 8-bit mode.
// -----------------------------------------------------------------------------

void R_DrawTLColumn_8 (void)
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
    const int step = 2;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    const int a  = 168;     // 0..256 (I_BlendOver_168)

    // Compute one pixel, write it to two vertical lines
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t bg = *dest;

        // Final color via the current pal_color
        const pixel_t blended = TLBlendOver(fg, bg, a);

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
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t bg = *dest;
        const pixel_t blended = TLBlendOver(fg, bg, a);

        dest[0] = blended;
    }
}

void R_DrawTLColumnLow_8 (void)
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
    const int step = 2;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Two destination columns
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    const int a  = 168;     // 0..256 (I_BlendOver_168)

    // Process screen in 2×2 pixel blocks (2 lines, 2 columns)
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];

        // Column 1 (blend with its own background)
        {
            const pixel_t bg = *dest1;
            const pixel_t blended = TLBlendOver(fg, bg, a);

            dest1[0]           = blended;
            dest1[screenwidth] = blended;
        }

        // Column 2 (its own background)
        {
            const pixel_t bg = *dest2;
            const pixel_t blended = TLBlendOver(fg, bg, a);

            dest2[0]           = blended;
            dest2[screenwidth] = blended;
        }

        // Next pair of lines/advance
        dest1   += screenwidth * step;
        dest2   += screenwidth * step;
        frac    += fracstep;
        y_start += step;
    }

    // Handle final row if height is odd (draw single line, both columns)
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];

        // Column 1 tail
        {
            const pixel_t bg = *dest1;
            const pixel_t blended = TLBlendOver(fg, bg, a);

            dest1[0] = blended;
        }

        // Column 2 tail
        {
            const pixel_t bg = *dest2;
            const pixel_t blended = TLBlendOver(fg, bg, a);

            dest2[0] = blended;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawTransTLFuzzColumn_8
// [PN/JN] Translucent, translated fuzz column. 8-bit mode.
// -----------------------------------------------------------------------------

void R_DrawTransTLFuzzColumn_8 (void)
{
    const int count = dc_yh - dc_yl;
    if (count < 0)
        return; // No pixels to draw

    // Local pointers for improved memory access
    const byte *restrict const sourcebase   = dc_source;
    const byte *restrict const translation  = dc_translation;
    const pixel_t *restrict const colormap0 = dc_colormap[0];
    const int screenwidth = SCREENWIDTH;
    const int step = 2;

    int y_start = dc_yl;
    const int y_end = dc_yh;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest =
        ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    // Alpha for "fuzz" translucency (matches I_BlendOver_64)
    const int a  = 64;          // 0..256 (≈25% foreground)

    // Compute one pixel, write it to two vertical lines
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const unsigned t = translation[s];

        // Foreground in truecolor, background from framebuffer
        const pixel_t fg = colormap0[t];
        const pixel_t bg = *dest;

        // Final color via the current pal_color
        const pixel_t blended = TLBlendOver(fg, bg, a);

        // Write two pixels (current and next line)
        dest[0]           = blended;
        dest[screenwidth] = blended;

        // Advance
        dest  += screenwidth * step;
        frac  += fracstep;
        y_start += step;
    }

    // Handle final odd line
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const unsigned t = translation[s];
        const pixel_t fg = colormap0[t];
        const pixel_t bg = *dest;
        const pixel_t blended = TLBlendOver(fg, bg, a);

        dest[0] = blended;
    }
}

void R_DrawTransTLFuzzColumnLow_8 (void)
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
    const int step = 2;

    int y_start = dc_yl;
    const int y_end = dc_yh;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointers
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    // Alpha for "fuzz" translucency (matches I_BlendOver_64)
    const int a  = 64;          // 0..256 (≈25% foreground)

    // Process screen in 2×2 pixel blocks (2 lines, 2 columns)
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const unsigned t = translation[s];
        const pixel_t fg = colormap0[t];

        // Column 1 (blend with its own background)
        {
            const pixel_t bg = *dest1;
            const pixel_t blended = TLBlendOver(fg, bg, a);

            dest1[0]           = blended;
            dest1[screenwidth] = blended;
        }

        // Column 2 (its own background)
        {
            const pixel_t bg = *dest2;
            const pixel_t blended = TLBlendOver(fg, bg, a);

            dest2[0]           = blended;
            dest2[screenwidth] = blended;
        }

    // Move to next pair of lines
        dest1 += screenwidth * step;
        dest2 += screenwidth * step;
        frac  += fracstep;
        y_start += step;
    }

    // Handle final row if height is odd (draw single line, both columns)
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const unsigned t = translation[s];
        const pixel_t fg = colormap0[t];

        // Column 1 tail
        {
            const pixel_t bg = *dest1;
            const pixel_t blended = TLBlendOver(fg, bg, a);

            dest1[0] = blended;
        }

        // Column 2 tail
        {
            const pixel_t bg = *dest2;
            const pixel_t blended = TLBlendOver(fg, bg, a);

            dest2[0] = blended;
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawTLAddColumn_8
// [PN/JN] Translucent column, additive blending. 8-bit mode.
// -----------------------------------------------------------------------------

void R_DrawTLAddColumn_8(void)
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
    const int step = 2;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Precompute initial destination pointer
    pixel_t *restrict dest = ylookup[y_start] + columnofs[flipviewwidth[dc_x]];

    // Compute one pixel, write it to two vertical lines
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t bg = *dest;

        // Write the final palette color (already gamma-corrected)
        const pixel_t blended = TLBlendAdd(fg, bg);

        dest[0]           = blended;
        dest[screenwidth] = blended;

        dest    += screenwidth * step;
        frac    += fracstep;
        y_start += step;
    }

    // Handle final row if height is odd (draw single line, both columns)
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];
        const pixel_t bg = *dest;
        const pixel_t blended = TLBlendAdd(fg, bg);

        dest[0] = blended;
    }
}

void R_DrawTLAddColumnLow_8(void)
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
    const int step = 2;
    int y_start = dc_yl;
    int y_end = dc_yh;

    // Setup scaling
    const fixed_t fracstep = dc_iscale * step;
    fixed_t frac = dc_texturemid + (y_start - centery) * dc_iscale;

    // Two destination columns
    pixel_t *restrict dest1 = ylookup[y_start] + columnofs[flipviewwidth[x]];
    pixel_t *restrict dest2 = ylookup[y_start] + columnofs[flipviewwidth[x + 1]];

    // Process screen in 2×2 pixel blocks (2 lines, 2 columns)
    while (y_start < y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];

        // Column 1 (blend with its own background)
        {
            const pixel_t bg = *dest1;
            const pixel_t blended = TLBlendAdd(fg, bg);

            dest1[0]           = blended;
            dest1[screenwidth] = blended;
        }

        // Column 2 (its own background)
        {
            const pixel_t bg = *dest2;
            const pixel_t blended = TLBlendAdd(fg, bg);

            dest2[0]           = blended;
            dest2[screenwidth] = blended;
        }

        // Next pair of lines/advance
        dest1   += screenwidth * step;
        dest2   += screenwidth * step;
        frac    += fracstep;
        y_start += step;
    }

    // Handle final row if height is odd (draw single line, both columns)
    if (y_start == y_end)
    {
        const unsigned s = sourcebase[frac >> FRACBITS];
        const pixel_t fg = brightmap[s] ? colormap1[s] : colormap0[s];

        // Column 1 tail
        {
            const pixel_t bg = *dest1;
            const pixel_t blended = TLBlendAdd(fg, bg);

            dest1[0] = blended;
        }

        // Column 2 tail
        {
            const pixel_t bg = *dest2;
            const pixel_t blended = TLBlendAdd(fg, bg);

            dest2[0] = blended;
        }
    }
}
